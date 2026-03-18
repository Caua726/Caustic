# Caustic Compiler Optimization Plan

## Overview

Three releases to improve robustness, code quality, and performance of the Caustic self-hosted x86_64 compiler.

**Validation criteria for ALL releases:** Gen4 fixpoint (assembly identical gen3==gen4) + all `examples/` compile and produce correct output + benchmark correctness.

## Release 1: Bugs + Quality

### Bug Fixes

1. **opt.cst:649** — DCE `used[]` allocated with pre-SSA `vc`. Fix: use `func.vreg_count` post-SSA.
2. **opt.cst:380,408** — `use_count[]` and `addr_offset[]` same problem. Fix: use `vc_alloc`.
3. **ssa.cst:240** — DFS stack `nblocks` too small. Fix: allocate `nblocks * 2 + 4`.
4. **ssa.cst:541** — `has_phi[4096]` too small. Fix: heap-allocate `nblocks * nslots`.
5. **ssa.cst:772** — STORE reads `src1.vreg` without op_type check. Fix: guard with `OP_VREG` check.
6. **alloc.cst:308** — `spill_register` can return caller-saved reg across call. Fix: pass `crosses_call` flag.

### Quality

1. **mem.cst:102** — `free()` bounds check (ptr within heap range).
2. **mem.cst:51** — Alignment: `size = (size + 7) & (0 - 8)`.
3. **slice.cst:19** — `push()` cap=0 guard: `if (new_cap < 4) { new_cap = 4; }`.
4. **slice.cst:40** — `pop()` empty guard.
5. **gen.cst:1529** — `ir_done_bodies` bounds check.

### Docs

1. Update CLAUDE.md pipeline diagram with SSA, store-load forwarding, register coalescing.
2. Document `-O0`/`-O1` flags when implemented.

## Release 2: Refactoring

Split 14 files > 600 lines to < 400 lines each. Zero behavior change. Assembly output must be byte-identical before/after each split.

Order (least risk → most risk):

1. `caustic-assembler/encoder.cst` (1427L) → `asm_defs.cst` + `buf.cst` + `encoder.cst`
2. `caustic-assembler/lexer.cst` (683L) → `ht.cst` + `lexer.cst`
3. `caustic-assembler/main.cst` (1018L) → `asm_parser.cst` + `asm_passes.cst` + `main.cst`
4. `caustic-linker/elf_reader.cst` (612L) → remove ByteBuffer dupe, use buf.cst
5. `caustic-linker/elf_writer.cst` (608L) → `elf_common.cst` + `elf_writer.cst`
6. `caustic-linker/linker.cst` (1119L) → `dynamic.cst` + `resolve.cst` + `linker.cst`
7. `src/ir/ssa.cst` (1067L) → `cfg.cst` + `dom.cst` + `ssa.cst`
8. `src/ir/opt.cst` (730L) → `opt_constprop.cst` + `opt_fusion.cst` + `opt_dce.cst` + `opt.cst`
9. `src/ir/cache.cst` (606L) → `ir_cache_read.cst` + `ir_cache_write.cst`
10. `src/parser/cache.cst` (669L) → `tok_cache.cst` + `ast_cache.cst`
11. `src/codegen/emit.cst` (1345L) → `emit_arith.cst` + `emit_memory.cst` + `emit_control.cst` + `emit_calls.cst` + `emit.cst`
12. `src/ir/gen.cst` (1596L) → `gen_expr.cst` + `gen_helpers.cst` + `gen.cst`
13. `src/semantic/walk.cst` (1583L) → `walk_expr.cst` + `walk_stmt.cst` + `walk_decl.cst` + `walk.cst`
14. `src/main.cst` (947L) → `pipeline.cst` + `args.cst` + `prof.cst` + `main.cst`

New shared file: `std/util.cst` with `strlen`, `print_str`, `print_int`.

## Release 3: -O0 and -O1

Default = `-O0` (no optimizations). `-O1` enables all optimizations.

`-O1` equivalence target: gcc -O2 level performance.

### Flag implementation

Global `opt_level` in `opt.cst`. Parsed in `main.cst`. Each pass checks level.

- `-O0`: DCE only (dead code elimination).
- `-O1`: All passes: const prop, copy prop, ADDR fusion, store-load fwd (i64+i32), SSA mem2reg, branch folding, algebraic simplification, strength reduction, codegen improvements.

### Optimizations (all under -O1)

By implementation order:

1. **Constant fold comparisons** — `opt_constprop.cst` — EQ/NE/LT/LE/GT/GE with both constants → IMM 0/1.
2. **Branch folding** — `opt_constprop.cst` — JZ/JNZ on known constant → JMP or dead.
3. **`test r15, r15`** — `emit_control.cst` — replace `cmp r15, 0` for zero checks.
4. **`imul r15, r15, imm`** — `emit_arith.cst` — direct 3-operand imul instead of mov+imul.
5. **Push/pop pruning** — `emit.cst` + `alloc.cst` — track `used_callee_mask`, only push/pop used regs.
6. **Store-load fwd i32** — `opt_fusion.cst` — allow forwarding when `store_sz == load_sz`.
7. **Algebraic simplification** — `opt_constprop.cst` — `x - x = 0`, `x ^ x = 0`, `x & x = x`.
8. **SHL for pow2 stride** — `gen_helpers.cst` — `* 4` → `<< 2` in array index.
9. **Signed div/mod pow2** — `opt_constprop.cst` — shift+adjust sequence.
10. **LAND/LOR short-circuit** — `gen_expr.cst` — jump directly without materializing 0/1.

### Performance targets

| Test | Current | -O1 target | gcc -O0 | LuaJIT |
|---|---:|---:|---:|---:|
| fib(38) | 199ms | ~160ms | 177ms | 326ms |
| sieve(10M) | 165ms | ~80ms | 42ms | 33ms |
| sort(10K) | 306ms | ~150ms | 87ms | 146ms |
| matmul(64) | 309ms | ~120ms | 41ms | 89ms |
| collatz(1M) | 281ms | ~200ms | 307ms | 436ms |
| **Total** | **1262ms** | **~710ms** | **654ms** | **1030ms** |

### Future (-O2, -O3)

- `-O2`: inlining, LICM, loop unrolling (gcc -O3 equivalent)
- `-O3`: auto-vectorization, profile-guided (target: faster than gcc -O3)
