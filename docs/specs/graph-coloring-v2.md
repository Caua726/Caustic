# Graph Coloring Register Allocator v2

## Overview

Rewrite of the graph coloring register allocator using Iterative Register Coalescing (George & Appel, 1996). Replaces the buggy `graph_coloring_alloc` in `src/codegen/alloc.cst`. Only active under `-O1`; `-O0` continues using linear scan.

**Target:** benchmark < 800ms (current linear scan -O1: ~1105ms).

## Algorithm

Iterative Register Coalescing with:
- Bit matrix for interference graph
- George criterion for coalescing (merge A into B if every high-degree neighbor of A that is NOT already a neighbor of B has degree < K_eff(neighbor))
- Spill cost weighted by loop depth (10^depth per use, capped at depth 4)
- Optimistic coloring (Briggs) for potential spills — nodes pushed as potential spill are colored normally in Select; they only truly spill if no color is available among their actually-colored neighbors
- K_effective = 5 for call-crossing vregs, 10 for others

## Flow

```
build_intervals (existing, reuse)
    ↓
build interference (bit matrix, O(n²))
    ↓
detect loop depth (back-edge nesting analysis)
    ↓
compute spill costs (weighted by 10^depth)
    ↓
init worklists (simplify, freeze, spill based on degree + move-relatedness)
    ↓
Main loop (priority order, restart from top after each action):
│   1. SIMPLIFY (if simplify_wl not empty): pop node, push to stack
│   2. COALESCE (if active MOVs pass George): merge two nodes
│   3. FREEZE (if freeze_wl not empty): give up coalescing, move to simplify
│   4. SPILL (if spill_wl not empty): push lowest-cost node as potential spill
│   repeat until ALL worklists empty and no active MOVs
    ↓
SELECT: pop stack LIFO, assign colors
    ↓
VERIFY: safety check (no conflicts, no caller-saved across calls)
```

## Data Structures

### Bit matrix (interference graph)
```
Size: (n * n) / 8 bytes (n = vreg_count, typically 10-800)
For n=800: 80KB. Full symmetric matrix (both triangles stored).
Index computation MUST use i64: cast(i64, i) * cast(i64, n) + cast(i64, j) to avoid i32 overflow.
```

### GCNode (per-vreg info)
```
Parallel arrays indexed by VREG NUMBER (0..vreg_count-1), NOT by interval index.
Build a vreg_to_interval[] mapping from build_intervals output to look up LiveInterval.

- degree[v]: number of active (non-coalesced, non-stacked) neighbors
- color[v]: assigned register (-1 = uncolored)
- alias[v]: coalesce target (-1 = not merged). Use resolve_alias() with path compression.
- status[v]: 0=active, 1=on_stack, 2=coalesced
- is_move_related[v]: 1 if involved in any active (non-frozen, non-coalesced) MOV
- spill_cost[v]: weighted cost (higher = harder to spill)
- crosses_call[v]: 1 if vreg's interval spans a call (from spans_call()). Updated on coalesce (OR).
```

### Move list
```
Flat array of {src_vreg, dst_vreg, status}
status: 0=active, 1=coalesced, 2=frozen, 3=constrained
Count IR_MOV instructions in a first pass to determine allocation size.
```

### Worklists
```
Flat arrays, each allocated to n entries (max possible):
- simplify_wl[]: low-degree, non-move-related
- freeze_wl[]: low-degree, move-related
- spill_wl[]: high-degree
- stack[]: removed nodes awaiting coloring
Each with a count variable. Add/remove by swap-with-last.
```

All allocated with `mem.galloc` at start, freed at end.

## Phase Details

### Build interference
- Reuse `build_intervals` output (LiveInterval with start/end_pos per vreg)
- Build `vreg_to_interval[v]` mapping: scan sorted intervals, for each interval set `vreg_to_interval[iv.vreg] = interval_index`
- For each pair of vregs (i,j) where intervals overlap: set bit in matrix
- Overlap check: `a.start <= b.end && b.start <= a.end`
- Skip dead vregs (start == INT_MAX) in both build AND worklist init
- Reuse existing `spans_call()` function (already accounts for IR_SET_ARG)

### Loop depth detection
- Find all back-edges: JMP/JZ/JNZ to a label at earlier position
- Each back-edge defines a loop range [header_pos, backedge_pos]
- Build `inst_loop_depth[position]`: for each instruction position, count how many loop ranges contain it
- Used in spill cost computation per-use, not per-vreg

### Spill cost computation
```
pow10 lookup table: [1, 10, 100, 1000, 10000] (cap depth at 4)

For each vreg v:
    cost = 0
    scan all live instructions, when v appears as src1/src2/dest:
        depth = inst_loop_depth[position]
        if depth > 4: depth = 4
        cost += pow10_table[depth]
    spill_cost[v] = cost

crosses_call[v] = spans_call(iv.start, iv.end_pos, ctx)
```

### Worklist initialization
```
For each vreg v where status[v] == active AND start != INT_MAX:
    k_eff = crosses_call[v] ? 5 : 10
    if degree[v] >= k_eff:
        add to spill_wl
    else if is_move_related[v]:
        add to freeze_wl
    else:
        add to simplify_wl
```

### Simplify
```
Pop node v from simplify_wl
Push onto stack (status[v] = on_stack)
For each neighbor n of v (scan bit matrix row, skip coalesced/stacked):
    degree[n] -= 1
    k_eff_n = crosses_call[n] ? 5 : 10
    if degree[n] == k_eff_n - 1:  // just dropped below threshold
        if is_move_related[n]:
            move n from spill_wl → freeze_wl
        else:
            move n from spill_wl → simplify_wl
```

### Coalesce (George criterion)
```
For each active MOV (src=a, dst=b):
    a = resolve_alias(a), b = resolve_alias(b)
    if a == b: mark MOV coalesced, skip
    if interferes(a, b): mark MOV constrained, skip

    // George criterion: can merge a into b?
    // For each neighbor t of a that is NOT already a neighbor of b,
    // t must have degree < K_eff(t). Uses K of the NEIGHBOR, not of a.
    ok = true
    for each active neighbor t of a:
        if not interferes(t, b):
            k_eff_t = crosses_call[t] ? 5 : 10
            if degree[t] >= k_eff_t:
                ok = false; break

    if ok:
        // Merge a into b
        alias[a] = b
        status[a] = coalesced
        crosses_call[b] = crosses_call[b] | crosses_call[a]  // propagate call-crossing

        // Transfer edges: for each active neighbor t of a
        //   if not interferes(t, b): set bit(t, b), degree[b] += 1
        //   (edge t-a is now subsumed by t-b since a is coalesced)
        // Do NOT clear a's edges — coalesced nodes are skipped via status check

        // Transfer move-relatedness
        is_move_related[b] = is_move_related[b] | is_move_related[a]

        // Reclassify b
        k_eff_b = crosses_call[b] ? 5 : 10
        if degree[b] >= k_eff_b: ensure b is in spill_wl
        else if is_move_related[b]: ensure b is in freeze_wl
        else: ensure b is in simplify_wl

        // Check if neighbors that were high-degree are now low-degree
        for each active neighbor t of b:
            k_eff_t = crosses_call[t] ? 5 : 10
            if degree[t] < k_eff_t and t is in spill_wl:
                move t to freeze_wl or simplify_wl

        mark MOV coalesced
```

### Freeze
```
Pop node v from freeze_wl
For each MOV involving v:
    mark MOV frozen
    // Check if MOV partner p is still move-related
    p = (mov.src == v) ? mov.dst : mov.src
    p = resolve_alias(p)
    if p has no remaining active MOVs:
        is_move_related[p] = 0
        if p is in freeze_wl:
            move p to simplify_wl

is_move_related[v] = 0
Move v to simplify_wl
```

### Spill
```
Pick node v from spill_wl with LOWEST spill_cost
Push onto stack (status[v] = on_stack)
For each active neighbor n of v:
    degree[n] -= 1
    k_eff_n = crosses_call[n] ? 5 : 10
    if degree[n] == k_eff_n - 1:
        if is_move_related[n]: move from spill_wl → freeze_wl
        else: move from spill_wl → simplify_wl
```

### Select (coloring)
```
While stack not empty:
    Pop node v
    Build set of used colors: for each neighbor n of v in bit matrix:
        n = resolve_alias(n)
        if color[n] >= 0:
            slot = reg_to_slot(color[n])
            if slot >= 0: used[slot] = 1

    // Exclude caller-saved if call-crossing
    if crosses_call[v]:
        used[4] = 1  // r8
        used[5] = 1  // r9
        used[6] = 1  // r10
        used[7] = 1  // rsi
        used[8] = 1  // rdi

    // Try coalesce hint: if v was coalesced partner, try partner's color
    chosen = -1
    if alias partner exists and has a color and slot is available: chosen = partner's color

    // Otherwise first-fit
    if chosen < 0:
        for slot 0..9: if not used[slot]: chosen = regs[slot]; break

    if chosen >= 0:
        color[v] = chosen
        vreg_to_loc[vreg] = chosen
        gc_mark_callee(chosen, ctx)  // reuse existing helper
    else:
        // Actual spill (optimistic coloring failed for this potential spill)
        spill = ctx.next_spill
        ctx.next_spill = ctx.next_spill + 1
        vreg_to_loc[vreg] = 0 - spill
```

### Verify
```
For each pair of colored vregs that interfere:
    assert they have different colors
For each call-crossing vreg with a color:
    assert color is callee-saved
If violation found: force spill the offender (safety net, should never trigger)
```

## resolve_alias
```
fn resolve_alias(v):
    let root = v
    while alias[root] >= 0: root = alias[root]
    // Path compression
    while alias[v] >= 0:
        next = alias[v]
        alias[v] = root
        v = next
    return root
```

## Registers

| Index | Register | Type | Slot | Notes |
|-------|----------|------|------|-------|
| 0 | rax | scratch | - | not allocatable |
| 1 | rcx | scratch | - | not allocatable |
| 2 | rdx | scratch | - | not allocatable |
| 3 | rbx | callee-saved | 0 | |
| 4 | rsi | caller-saved | 7 | also arg reg 1 |
| 5 | rdi | caller-saved | 8 | also arg reg 0 |
| 6 | r8 | caller-saved | 4 | also arg reg 4 |
| 7 | r9 | caller-saved | 5 | also arg reg 5 |
| 8 | r10 | caller-saved | 6 | |
| 9 | r11 | scratch | - | not allocatable (codegen scratch) |
| 10 | r12 | callee-saved | 1 | |
| 11 | r13 | callee-saved | 2 | |
| 12 | r14 | callee-saved | 3 | |
| 15 | r15 | callee-saved | 9 | |

Index = physical register index in emit.cst. Slot = position in the 10-element allocatable array.

K = 10 allocatable registers. K_callee = 5 (rbx, r12, r13, r14, r15).

`is_caller_saved(reg)` returns 1 for: rsi(4), rdi(5), r8(6), r9(7), r10(8).

Reuse existing helpers: `spans_call()`, `reg_to_slot()`, `is_caller_saved()`, `gc_mark_callee()`.

## Files

| File | Change |
|------|--------|
| `src/codegen/alloc.cst` | Delete `graph_coloring_alloc` (289 lines). Add `graph_coloring_alloc_v2` (~500-600 lines) with all helper functions. Keep all existing helpers. |
| `src/codegen/emit.cst` | Switch allocator: `if (al.al_opt_level >= 1) { al.graph_coloring_alloc_v2(func, ctx); } else { al.linear_scan_alloc(func, ctx); }` |

No changes to: `build_intervals`, `LiveInterval`, `AllocCtx`, `linear_scan_alloc`, `get_loc`, `is_mem`.

## Validation

1. All examples pass with `-O0` (linear scan unchanged)
2. All examples pass with `-O1` (graph coloring): hello, fibonacci, structs, enums, linked_list, bench, test_arr
3. Gen4 `-O1` bootstrap: gen2=gen3=gen4 (same size, 0-diff fixpoint)
4. Benchmark `-O1` < 800ms
5. test_arr exits 100 (array loop correctness)

## Caustic Language Gotchas

- `with mut` for mutable variables
- `cast(*i32, cast(i64, array) + cast(i64, index) * 4)` for array access
- `sizeof(LiveInterval)` for struct array stride
- `mem.galloc` / `mem.gfree` for heap allocation
- `fn` and `gen` are keywords (cannot use as variable names)
- Use literal `3, 34` for mmap flags (never `linux.PROT_READ + linux.PROT_WRITE`)
- Structs are packed (no padding)
- `0 - 1` instead of `-1` for negative values
- `cast(i32, 0 - 1)` for -1 as i32
- No `pow` function — use lookup table `[1, 10, 100, 1000, 10000]`
- No `++` operator — use `x = x + 1`
- Bit matrix indexing: `cast(i64, i) * cast(i64, n) + cast(i64, j)` to avoid i32 overflow
