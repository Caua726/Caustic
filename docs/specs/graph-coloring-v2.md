# Graph Coloring Register Allocator v2

## Overview

Rewrite of the graph coloring register allocator using Iterative Register Coalescing (George & Appel, 1996). Replaces the buggy `graph_coloring_alloc` in `src/codegen/alloc.cst`. Only active under `-O1`; `-O0` continues using linear scan.

**Target:** benchmark < 800ms (current linear scan -O1: ~1105ms).

## Algorithm

Iterative Register Coalescing with:
- Bit matrix for interference graph
- George criterion for coalescing (merge A into B if every high-degree neighbor of A is already a neighbor of B)
- Spill cost weighted by loop depth (10^depth per use)
- Optimistic coloring (Briggs) for potential spills
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
┌─→ SIMPLIFY: pop low-degree non-move-related node, push to stack
│   ↓
│   COALESCE: merge MOV-connected nodes passing George criterion
│   ↓
│   FREEZE: give up coalescing on a low-degree move-related node → becomes simplifiable
│   ↓
│   SPILL: push lowest-cost high-degree node as potential spill
│   ↓
│   repeat until all worklists empty
└───┘
    ↓
SELECT: pop stack LIFO, assign colors (optimistic retry for potential spills)
    ↓
VERIFY: safety check (no conflicts, no caller-saved across calls)
```

## Data Structures

### Bit matrix (interference graph)
```
Size: (n * n) / 8 bytes (n = vreg_count, typically 10-800)
For n=800: 80KB
Symmetric: set(i,j) always sets (j,i)
Access: O(1) via bit manipulation
```

### GCNode (per-vreg info)
```
struct-like array (flat fields, each a separate i32 array for Caustic compat):
- degree[v]: number of active neighbors
- color[v]: assigned register (-1 = uncolored)
- alias[v]: coalesce target (-1 = not merged)
- status[v]: 0=active, 1=on_stack, 2=coalesced
- is_move_related[v]: 1 if involved in any active MOV
- spill_cost[v]: weighted cost (higher = harder to spill)
```

### Move list
```
Flat array of {src_vreg, dst_vreg, status}
status: 0=active, 1=coalesced, 2=frozen, 3=constrained
Extracted from IR_MOV instructions
```

### Worklists
```
Flat arrays with count (no linked lists):
- simplify_wl[]: low-degree, non-move-related
- freeze_wl[]: low-degree, move-related
- spill_wl[]: high-degree
- stack[]: removed nodes awaiting coloring
```

All allocated with `mem.galloc` at start, freed at end.

## Phase Details

### Build interference
- Reuse `build_intervals` output (LiveInterval with start/end_pos per vreg)
- For each pair (i,j) where intervals overlap: set bit in matrix
- Overlap check: `a.start <= b.end && b.start <= a.end`
- Skip dead vregs (start == INT_MAX)

### Loop depth detection
- Find all back-edges: JMP/JZ/JNZ to a label at earlier position
- Each back-edge defines a loop range [header_pos, backedge_pos]
- For each vreg, loop_depth = max nesting depth across all its uses
- Array: `loop_depth[vreg]` computed by scanning instructions and counting containing loops

### Spill cost computation
```
For each vreg v:
    cost = 0
    for each use of v at position p:
        depth = loop_depth_at_position(p)
        cost += pow10(depth)  // 1 outside, 10 in loop, 100 in nested loop
    spill_cost[v] = cost
```

### Worklist initialization
```
For each active vreg v:
    k_eff = spans_call(v) ? 5 : 10
    if degree[v] >= k_eff:
        add to spill_wl
    else if is_move_related[v]:
        add to freeze_wl
    else:
        add to simplify_wl
```

### Simplify
```
Pop node from simplify_wl
Push onto stack (status = on_stack)
For each neighbor n (via bit matrix row scan):
    if n is active: degree[n] -= 1
    if degree[n] drops below k_eff(n):
        move n from spill_wl → freeze_wl or simplify_wl
```

### Coalesce (George criterion)
```
For each active MOV (src=a, dst=b):
    a = resolve_alias(a), b = resolve_alias(b)
    if a == b: mark MOV coalesced, skip
    if interferes(a, b): mark MOV constrained, skip

    // George criterion: can merge a into b?
    k_eff = spans_call(a) ? 5 : 10
    ok = true
    for each neighbor t of a:
        if degree[t] >= k_eff and not interferes(t, b):
            ok = false; break

    if ok:
        alias[a] = b
        status[a] = coalesced
        // Transfer edges: for each neighbor t of a, add edge (t, b)
        degree[b] = recount from matrix
        // Update move-relatedness of b
        // Reclassify b in worklists if degree changed
        mark MOV coalesced
```

### Freeze
```
Pop node from freeze_wl
Mark all its MOVs as frozen
is_move_related[node] = 0
Move node to simplify_wl
```

### Spill
```
Pick node from spill_wl with LOWEST spill_cost
Push onto stack as potential spill (status = on_stack)
Decrement neighbors' degrees (same as simplify)
```

### Select (coloring)
```
While stack not empty:
    Pop node v
    Find colors used by active neighbors (scan bit matrix row, check color[resolve_alias(n)])

    k_eff = spans_call(v) ? 5 : 10
    if crosses_call: mark caller-saved slots unavailable

    // Try coalesce hint first
    if alias partner has a color and it's available: use it

    // Otherwise first-fit
    Pick first available color from regs[0..9]

    if found:
        color[v] = chosen register
        vreg_to_loc[v.vreg] = chosen
    else:
        // Actual spill
        vreg_to_loc[v.vreg] = -(next_spill++)
```

### Verify
```
For each pair of colored vregs that interfere:
    assert they have different colors
For each call-crossing vreg with a color:
    assert color is callee-saved
If violation found: force spill the offender (safety net)
```

## Registers

| Index | Register | Type | Allocatable slot |
|-------|----------|------|-----------------|
| 0 | rax | scratch | not allocatable |
| 1 | rcx | scratch | not allocatable |
| 2 | rdx | scratch | not allocatable |
| 3 | rbx | callee-saved | 0 |
| 4 | rsi | caller-saved | 7 |
| 5 | rdi | caller-saved | 8 |
| 6 | r8 | caller-saved | 4 |
| 7 | r9 | caller-saved | 5 |
| 8 | r10 | caller-saved | 6 |
| 10 | r12 | callee-saved | 1 |
| 11 | r13 | callee-saved | 2 |
| 12 | r14 | callee-saved | 3 |
| 15 | r15 | callee-saved | 9 |

K = 10 allocatable registers. K_callee = 5 (rbx, r12, r13, r14, r15).

`is_caller_saved(reg)` returns 1 for: rsi(4), rdi(5), r8(6), r9(7), r10(8).

## Files

| File | Change |
|------|--------|
| `src/codegen/alloc.cst` | Delete `graph_coloring_alloc` (289 lines). Add `graph_coloring_alloc_v2` (~400-500 lines) with all helper functions. |
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
