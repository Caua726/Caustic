# Optimization flags: -O0 / -O1

## What

Add `-O0` (default, no optimizations) and `-O1` (all optimizations) flags to the compiler.

## Changes

### src/main.cst

- Add global `let is i32 as g_opt_level with mut = 0;`
- In argument parsing, add:
  - `-O0` → `g_opt_level = 0`
  - `-O1` → `g_opt_level = 1`
- Guard the `iropt.optimize()` calls: only call when `g_opt_level >= 1`
- There are two call sites for `iropt.optimize()` — one in the single-file path and one in `compile_one`/`compile_frontend`. Guard both.

### What -O0 disables

All of these only run inside `iropt.optimize()`:
- SSA mem2reg (both passes — pre-fusion and post-fusion)
- Constant propagation + copy propagation
- ADDR+STORE fusion
- Store-load forwarding
- Constant folding into immediates
- LICM
- DCE
- Register coalescing hints (built from IR_MOV which SSA creates)

### What -O0 keeps

These are in the codegen, not the optimizer:
- Push/pop pruning (based on allocator output)
- test r15,r15 (codegen instruction selection)
- SHL for pow2 stride (IR gen, not optimizer)
- Dynamic scratch selection (codegen)

### SHL for pow2 stride

This is in `gen.cst` (IR generation), not the optimizer. It always runs regardless of -O level. This is correct — it generates better IR without any analysis cost.

### Test

- `./caustic examples/bench.cst` → compiles with -O0 (no optimizations), should be slower
- `./caustic -O1 examples/bench.cst` → compiles with all optimizations, should match current speed
- Gen4 fixpoint with `-O1`
- All examples compile with both `-O0` and `-O1`
