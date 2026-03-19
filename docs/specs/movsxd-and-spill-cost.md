# Eliminate redundant movsxd + better spill cost

## Overview

Two optimizations to reduce benchmark runtime from ~972ms toward 800ms:

1. **Reorder addr_fusion before mem2reg** — enables promotion of i32 variables to SSA vregs
2. **Better spill cost in graph coloring** — stronger loop weighting + cost/range tiebreaker

Both are small changes (~10 lines each) with large runtime impact.

---

## Problem 1: i32 variables stuck on stack

### What happens now

For a Caustic function like:
```cst
fn sieve(limit as i32) as i32 {
    let is i32 as i with mut = 0;
    while (i < limit) {
        flags[i] = 1;
        i = i + 1;
    }
    ...
}
```

The IR generator produces:
```
IR_ADDR v3 = offset_64        ; take address of 'i' on stack
IR_IMM  v4 = 0
IR_STORE [v3] = v4            ; i = 0  (dest=OP_VREG, not IMM!)

.L0:
IR_ADDR  v5 = offset_64       ; &i again
IR_LOAD  v6 = [v5]            ; load i (src1=OP_VREG, not IMM!)
IR_LT    v7 = v6, limit
IR_JZ    .L1, v7
...
IR_ADDR  v8 = offset_64       ; &i again
IR_LOAD  v9 = [v8]            ; load i AGAIN
IR_ADD   v10 = v9, 1
IR_ADDR  v11 = offset_64      ; &i AGAIN
IR_STORE [v11] = v10          ; i = i+1 (dest=OP_VREG)
JMP .L0
```

The `mem2reg` pass promotes stack slots to SSA vregs, but ONLY when LOAD/STORE use
`OP_IMM` (direct stack offset). The IR above uses `OP_VREG` (address in a vreg from
IR_ADDR), so **mem2reg ignores it**.

The `addr_fusion` pass converts `IR_ADDR(IMM) + IR_STORE(dest=VREG)` into
`IR_STORE(dest=IMM)` — exactly what mem2reg needs. But addr_fusion runs AFTER mem2reg.

Result in assembly (sieve init loop, 14 instructions per iteration):
```asm
.L52:
  movsxd rdi, DWORD PTR [rbp-64]   ; load i, sign-extend i32→i64
  movsxd rbx, ebx                  ; sign-extend limit
  cmp rax, rbx                     ; i < limit
  jge .L53
  movsxd r13, DWORD PTR [rbp-64]   ; load i AGAIN
  mov r14, QWORD PTR [rbp-80]      ; load flags ptr
  add r14, r13                      ; flags + i
  mov r8, 1
  mov r9, r8
  movzx r9, r9b                    ; truncate to byte
  mov BYTE PTR [rcx], al           ; flags[i] = 1
  movsxd r10, DWORD PTR [rbp-64]   ; load i THIRD TIME
  add rsi, 1                       ; i + 1
  mov DWORD PTR [rbp-64], eax      ; store i back
  jmp .L52
```

With i32 promoted, the loop would be ~5-6 instructions.

### The fix

In `src/ir/opt.cst`, function `optimize_func`:

**Current order (lines 8-36):**
```
fn optimize_func(func):
    vc = func.vreg_count
    vc_alloc = vc + 512

    ssa.mem2reg(func)         ← line 13: runs first, can't see i32 slots
    vc = func.vreg_count
    vc_alloc = vc + 512

    // allocate is_const, imm_val, copy_of...

    pass_const_copy_prop(...)
    pass_addr_fusion(...)     ← line 29: fuses ADDR+STORE, but too late
    pass_store_load_fwd(...)
    ...
```

**New order:**
```
fn optimize_func(func):
    vc = func.vreg_count
    vc_alloc = vc + 512

    pass_addr_fusion(func, vc_alloc)  ← MOVED HERE: fuse ADDR+STORE → STORE(IMM)

    ssa.mem2reg(func)                 ← now sees all slots including i32
    vc = func.vreg_count
    vc_alloc = vc + 512

    // allocate is_const, imm_val, copy_of...

    pass_const_copy_prop(...)
    // NO addr_fusion here (already ran)
    pass_store_load_fwd(...)
    ...
```

### Exact code change in `src/ir/opt.cst`

Before (current lines 8-29):
```cst
fn optimize_func(func as *d.IRFunction) as void {
    let is i32 as vc with mut = func.vreg_count;
    if (vc == 0) { return; }
    let is i32 as vc_alloc with mut = vc + 512;

    ssa.mem2reg(func);
    vc = func.vreg_count;
    vc_alloc = vc + 512;

    // ... allocate tracking arrays ...

    passes.pass_const_copy_prop(func, is_const, imm_val, copy_of, vc);
    passes.pass_addr_fusion(func, vc_alloc);      // ← REMOVE FROM HERE
    passes.pass_store_load_fwd(func);
```

After:
```cst
fn optimize_func(func as *d.IRFunction) as void {
    let is i32 as vc with mut = func.vreg_count;
    if (vc == 0) { return; }
    let is i32 as vc_alloc with mut = vc + 512;

    passes.pass_addr_fusion(func, vc_alloc);       // ← ADD HERE (before mem2reg)

    ssa.mem2reg(func);
    vc = func.vreg_count;
    vc_alloc = vc + 512;

    // ... allocate tracking arrays ...

    passes.pass_const_copy_prop(func, is_const, imm_val, copy_of, vc);
    // addr_fusion removed from here
    passes.pass_store_load_fwd(func);
```

### Why this is safe

- `addr_fusion` only reads IR instructions and rewrites ADDR+STORE/LOAD operands.
  It does NOT create new vregs (vreg_count unchanged).
- `addr_fusion` needs `vc_alloc` for its use_count array — it uses the PRE-mem2reg
  vreg count, which is correct (all vregs exist, none have been added yet).
- `addr_fusion` does NOT depend on const_copy_prop or any other pass — it only needs
  the raw IR with ADDR+STORE/LOAD patterns.
- This reordering was attempted before and crashed — but the crash was caused by a
  SECOND mem2reg pass that ran after all other passes (commit ef2dea3 removed it).
  With only one mem2reg, the reordering is safe.

### Expected impact

Every loop with an i32 counter loses ~4-8 instructions per iteration:
- Eliminates `movsxd reg, DWORD PTR [rbp-X]` (load + sign-extend)
- Eliminates `mov DWORD PTR [rbp-X], eax` (store back)
- Loop counter stays in a register (graph coloring assigns one)

For sieve (10M iterations): ~4 fewer insns × 10M = ~40M fewer instructions.
For collatz (millions of iterations): similar savings.

Estimated total improvement: **-100 to -200ms** on benchmark.

---

## Problem 2: spill cost too low for inner loops

### What happens now

The graph coloring v2 in `src/codegen/alloc.cst` computes spill cost with loop depth
weighting. The code at line 781-818:

```cst
let is [5]i32 as pow10;
pow10[0] = 1; pow10[1] = 10; pow10[2] = 100; pow10[3] = 1000; pow10[4] = 10000;
// ... for each use of vreg at position p:
//     depth = loop_depth_at(p)
//     spill_cost[vreg] += pow10[depth]
```

A vreg used once inside a depth-1 loop gets cost 10. A vreg used 10x outside loops
gets cost 10. They're equal — but the inner-loop vreg executes millions of times more
and is far more important to keep in a register.

The SPILL phase (lines 1104-1134) picks the vreg with lowest `spill_cost`:
```cst
if (*sci < best_cost) {
    best_cost = *sci;
    best = si;
}
```

No tiebreaker — two vregs with equal cost are broken arbitrarily.

### The fix

**Change 1:** Increase loop weights in pow10 table (line 781-782):

Before:
```cst
pow10[0] = 1; pow10[1] = 10; pow10[2] = 100; pow10[3] = 1000; pow10[4] = 10000;
```

After:
```cst
pow10[0] = 1; pow10[1] = 100; pow10[2] = 10000; pow10[3] = 100000; pow10[4] = 100000;
```

Now inner loop (depth 1) = 100x outside. Double-nested = 10000x. This makes it nearly
impossible to spill a vreg used in a hot inner loop.

**Change 2:** Cost/range tiebreaker in SPILL phase (lines 1110-1119):

Before:
```cst
if (*sci < best_cost) {
    best_cost = *sci;
    best = si;
}
```

After:
```cst
// cost / range = cost density (higher = more important to keep)
// spill the vreg with LOWEST density
let is *LiveInterval as iv_cur = ... // get interval for current candidate
let is *LiveInterval as iv_best = ... // get interval for current best
let is i32 as range_cur = iv_cur.end_pos - iv_cur.start + 1;
let is i32 as range_best = iv_best.end_pos - iv_best.start + 1;
// Compare: sci/range_cur vs best_cost/range_best
// Cross-multiply to avoid division: sci * range_best vs best_cost * range_cur
if (cast(i64, *sci) * cast(i64, range_best) < cast(i64, best_cost) * cast(i64, range_cur)) {
    best_cost = *sci;
    best = si;
}
```

Uses `i64` cross-multiplication to avoid integer overflow and division.

### Why this helps

matmul has 48 spills with the current weights. A vreg used once in the triple-nested
inner loop (depth 3) gets cost 1000. Another vreg used 100x outside gets cost 100.
The inner-loop vreg correctly survives (1000 > 100).

But with depth-1 loops (most common), cost is only 10 vs 10 for 10 outside uses —
a toss-up. With the new weights (100 vs 10), the inner-loop vreg wins clearly.

The cost/range tiebreaker means: between two vregs with similar cost, we spill the one
that lives longer (frees the register for more instructions). A vreg with cost=100 over
50 instructions (density=2) is less important than cost=100 over 5 instructions
(density=20).

### Edge case: all vregs too expensive to spill

If ALL vregs have high spill cost (deeply nested function), the SPILL phase still works —
it picks the least expensive one. The algorithm never fails to spill; it just picks the
best candidate. Higher weights don't change the algorithm, only the ranking.

### Expected impact

Fewer spills in multi-loop functions. Each avoided spill eliminates:
- 1 store to stack per definition
- 1 load from stack per use
- Each load/store = ~4 cycles on cache hit

For matmul (48 spills, inner loop ~64×64×64 iterations): estimated **-30 to -50ms**.

---

## Files to change

| File | Lines | Change |
|------|-------|--------|
| `src/ir/opt.cst` | ~8-15 | Move `pass_addr_fusion` before `ssa.mem2reg`, remove from post-mem2reg position |
| `src/codegen/alloc.cst` | ~782 | Change pow10 table values |
| `src/codegen/alloc.cst` | ~1110-1119 | Add cost/range tiebreaker (cross-multiply, ~8 lines) |

Total: ~20 lines changed.

---

## Validation

```bash
rm -rf .caustic

# Build gen1
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1

# Examples -O0 (linear scan unchanged, must all pass)
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done

# Examples -O1 (must all pass, test_arr must exit 100)
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O1: $?"
done

# Bootstrap gen4 -O1 (gen2=gen3=gen4, same size)
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done

# Bench -O1 (target: < 800ms)
/tmp/gen4 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b

# Verify movsxd reduction: compare sieve assembly before/after
# Before fix: ~14 instructions in sieve init loop
# After fix: ~5-6 instructions (i32 counter in register)
grep -c "movsxd.*DWORD PTR" examples/bench.cst.s
# Should be significantly fewer than current ~18
```

## Caustic gotchas

- `with mut` for mutable variables
- `rm -rf .caustic` before rebuilding (cache goes stale)
- `fn` and `gen` are keywords — can't use as variable names
- Commits: lowercase, sem Co-Authored-By
- `cast(i64, x)` for i32→i64 arithmetic (avoid overflow in cross-multiply)
- `0 - 1` instead of `-1`
- `sizeof(LiveInterval)` for struct array stride
- `cast(*LiveInterval, cast(i64, ctx.intervals) + cast(i64, idx) * sizeof(LiveInterval))` for interval access
