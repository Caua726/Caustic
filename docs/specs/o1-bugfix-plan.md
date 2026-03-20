# -O1 Bootstrap Bugfix Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `-O1` produce correct code for ALL programs including the compiler itself (gen8 -O1 bootstrap with 0-diff fixpoint).

**Architecture:** Three independent bugs prevent -O1 self-bootstrap. Fix them in order of isolation difficulty: (1) caustic-as `[reg + reg*scale]` encoding, (2) optimizer `pass_const_copy_prop` stale const info in loops, (3) optimizer `pass_store_load_fwd` missing invalidation. After all fixes, the -O1 bootstrap must pass gen4+ fixpoint.

**Tech Stack:** Caustic self-hosted compiler (.cst files), x86_64 assembly, ELF

**Current state:** -O0 gen8 bootstrap works perfectly. -O1 compiles all 6 example programs correctly. -O1 self-bootstrap hangs at gen2 (compiler compiled with -O1 produces an infinite loop).

---

## Root Cause Analysis

The gen2-O1 hang means the -O1 optimizer produces **incorrect code** for some function in the compiler itself. All 6 small examples pass, but the compiler is ~37K lines with complex control flow.

Three bugs were identified:

### Bug A: `caustic-as` doesn't encode `[reg + reg*scale]` correctly
- **File:** `caustic-assembler/encoder.cst`
- **Symptom:** `mov rsi, QWORD PTR [rax + rcx * 8]` assembles to wrong bytes
- **Impact:** Addressing modes (codegen Item 2) are disabled with `&& 0 == 1` hack
- **Verification:** GNU `as` + GNU `ld` produces correct output for the same .s file; `caustic-as` + `caustic-ld` produces Exit 40 instead of Exit 100
- **Test case:** `examples/test_arr.cst` — array sum loop

### Bug B: `pass_const_copy_prop` marks multi-def vregs as constant
- **File:** `src/ir/opt_passes.cst` lines 82-107
- **Symptom:** In a loop, `IR_IMM v3 = 3` is seen first (marks `is_const[v3]=1`), then `IR_MOV v3 = v7` (phi back-edge) should clear it but only does so AFTER the loop body already used the stale constant
- **Impact:** Constant folding produces wrong results for loop variables (e.g. `i*8` folded to `0*8=0`)
- **Partial fix applied:** Single-def guard in IR_IMM tracking + `else { *dst_const = 0; }` in IR_MOV. These help but don't fully solve because the forward scan sees the IMM before the MOV in the instruction order
- **Root cause:** Forward-scan constant propagation is fundamentally unsound for vregs with multiple definitions (from SSA phi destruction). The `is_const` array must NEVER be set for a vreg with >1 definition.
- **Test case:** `examples/test_arr.cst` — gives Exit 40 (only last element) instead of Exit 100 (sum of all)

### Bug C: `pass_store_load_fwd` doesn't invalidate at branches
- **File:** `src/ir/opt_passes.cst` line 496
- **Symptom:** After a conditional branch (`JZ .L1`), the forwarding table carries entries from the fall-through path. If the branch target merges back (loop), the forwarded value is from a different iteration.
- **Partial fix applied:** Added `IR_JMP || IR_JZ || IR_JNZ` to the invalidation condition
- **Impact:** `fibonacci.cst` segfaults with -O1 (recursive function with conditional returns)
- **Test case:** `examples/fibonacci.cst` — crashes with -O1 without the JMP/JZ/JNZ fix

### Remaining unknown bug (gen2-O1 hang)
After fixing A, B, C — all 6 examples pass with -O1 but gen2-O1 still hangs. This is likely another manifestation of B or C in a more complex function (the compiler has deeply nested if/else chains, large switch-like dispatchers, and complex loop patterns that small examples don't exercise).

---

## Debugging Strategy for the Hang

The hang means some function compiled with -O1 enters an infinite loop. To find it:

### Task 1: Isolate which function hangs

**Files:**
- Modify: `src/main.cst` (add early-exit debug)

- [ ] **Step 1:** Build gen2-O1 (gen1 compiles src/main.cst with -O1):
```bash
./caustic -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen2o1
```

- [ ] **Step 2:** Run gen2-O1 with strace to see where it hangs:
```bash
timeout 5 strace -e trace=write /tmp/gen2o1 examples/hello.cst 2>&1 | tail -20
```
This shows the last write syscall before the hang, revealing which function is executing.

- [ ] **Step 3:** If strace isn't informative, add `io.write_str(linux.STDERR, "checkpoint N\n")` markers throughout `main()`, `parse_args()`, `compile_frontend()`, `run_single_file()` etc. Recompile with -O1 and run to see which checkpoint is the last one printed.

- [ ] **Step 4:** Once the hanging function is identified, extract it as a standalone test case and bisect which optimizer pass breaks it (same technique as used for fibonacci).

### Task 2: Fix `pass_const_copy_prop` definitively

**Files:**
- Modify: `src/ir/opt_passes.cst:52-381`

The current partial fix (single-def guard) is the right approach but may have edge cases. The definitive fix:

- [ ] **Step 1:** Pre-compute `def_count[vreg]` array counting ALL non-dead definitions (IR_IMM, IR_MOV, IR_ADD, etc — any instruction where `dest.op_type == OP_VREG` and `op != IR_STORE/IR_COPY`). This is already implemented as `ccp_defs`.

- [ ] **Step 2:** In the IR_IMM handler (line ~98), ONLY set `is_const[dv] = 1` if `ccp_defs[dv] == 1`. Already implemented.

- [ ] **Step 3:** In the IR_MOV handler (line ~92-107), when `copy_of[dv] = sv`, also check: if `ccp_defs[dv] > 1`, do NOT mark `is_dead = 1` on the MOV (it's a phi-destruction MOV that must execute). Currently, ALL IR_MOV are marked dead — this is wrong for phi MOVs that have multiple definitions.

- [ ] **Step 4:** In the constant folding section (line ~122-296), guard ALL constant folds with `ccp_defs[dest] == 1` check before converting to IR_IMM. If a fold produces a result for a multi-def vreg, skip the fold.

- [ ] **Step 5:** Test with `examples/test_arr.cst` (-O1 should give Exit 100):
```bash
./caustic -O1 examples/test_arr.cst && ./caustic-as examples/test_arr.cst.s && ./caustic-ld examples/test_arr.cst.s.o -o /tmp/ta && /tmp/ta; echo $?
```

### Task 3: Verify `pass_store_load_fwd` fix is sufficient

**Files:**
- Modify: `src/ir/opt_passes.cst:496`

- [ ] **Step 1:** The current fix (invalidate on JMP/JZ/JNZ) is conservative but correct. Verify it doesn't over-invalidate by checking that -O1 code is still smaller than -O0:
```bash
./caustic examples/fibonacci.cst && wc -c examples/fibonacci.cst.s     # -O0 size
./caustic -O1 examples/fibonacci.cst && wc -c examples/fibonacci.cst.s # -O1 size (should be smaller)
```

- [ ] **Step 2:** A more precise fix (future): only invalidate entries for the SPECIFIC branch target, not the entire table. But the conservative fix is correct and the performance impact is minimal.

### Task 4: Fix `caustic-as` SIB byte encoding for `[reg + reg*scale]`

**Files:**
- Modify: `caustic-assembler/encoder.cst`

- [ ] **Step 1:** Find the instruction encoding function that handles memory operands with SIB (Scale-Index-Base) byte. Search for `SIB` or scale/index encoding in `caustic-assembler/encoder.cst`.

- [ ] **Step 2:** The x86_64 SIB byte format is: `scale(2 bits) | index(3 bits) | base(3 bits)`. For `[rax + rcx * 8]`:
  - ModRM: mod=00, reg=dest, rm=100 (SIB follows)
  - SIB: scale=11 (×8), index=001 (rcx), base=000 (rax)
  - Verify the encoder produces these bytes correctly.

- [ ] **Step 3:** Write a minimal test:
```asm
.intel_syntax noprefix
.text
.globl main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 32
  mov QWORD PTR [rbp-32], 10
  mov QWORD PTR [rbp-24], 20
  lea rax, [rbp-32]
  mov rcx, 1
  mov rsi, QWORD PTR [rax + rcx * 8]
  mov rax, 60
  mov rdi, rsi
  syscall
```
Assemble with `caustic-as`, link with `caustic-ld`, run. Should exit with code 20.
Compare with `as` (GNU) output byte-by-byte using `objdump -d`.

- [ ] **Step 4:** Fix the SIB encoding bug, re-enable addressing modes in `src/codegen/emit.cst` (remove the `&& 0 == 1` hack).

### Task 5: Full -O1 bootstrap test

- [ ] **Step 1:** After all fixes, rebuild gen1:
```bash
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
```

- [ ] **Step 2:** Test all examples with -O0 AND -O1:
```bash
for ex in hello fibonacci structs enums linked_list bench; do
    /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O1: $?"
done
```

- [ ] **Step 3:** -O1 gen8 bootstrap:
```bash
for g in 2 3 4 5 6 7 8; do
    prev=$((g-1))
    /tmp/gen${prev} -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen${g}
    echo "Gen$g-O1: $(stat -c%s /tmp/gen$g)"
done
# Fixpoint
/tmp/gen7 -O1 src/main.cst && cp src/main.cst.s /tmp/g7.s
/tmp/gen8 -O1 src/main.cst && cp src/main.cst.s /tmp/g8.s
diff /tmp/g7.s /tmp/g8.s | wc -l  # Must be 0
```

- [ ] **Step 4:** Test gen8-O1 examples:
```bash
for ex in hello fibonacci structs enums linked_list bench; do
    /tmp/gen8 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "gen8-O1 $ex: $?"
done
```

- [ ] **Step 5:** Update `./caustic` binary to gen8-O1, commit all changes.

---

## Priority Order

1. **Task 1** (isolate hang) — fastest path to identifying the remaining bug
2. **Task 2** (const_prop fix) — most likely cause of the hang
3. **Task 3** (store_load_fwd verify) — already partially fixed
4. **Task 4** (caustic-as SIB) — independent, can be done in parallel
5. **Task 5** (full bootstrap) — final validation

## Key Files Reference

| File | What it does |
|------|-------------|
| `src/ir/opt_passes.cst:52-381` | `pass_const_copy_prop` — constant/copy propagation with folding |
| `src/ir/opt_passes.cst:487-580` | `pass_store_load_fwd` — local store-load forwarding |
| `src/ir/opt_passes.cst:590-630` | `pass_fold_immediates` — folds known constants into instruction operands |
| `src/ir/opt.cst` | Pass ordering and orchestration |
| `src/codegen/emit.cst:910` | Addressing mode integration (disabled with `&& 0 == 1`) |
| `src/codegen/alloc.cst:500-760` | Graph coloring allocator (has verification pass) |
| `caustic-assembler/encoder.cst` | x86_64 instruction encoding (SIB byte bug) |

## Test Cases

| Test | Expected -O0 | Expected -O1 | Tests what |
|------|-------------|-------------|-----------|
| `examples/test_arr.cst` | Exit 100 | Exit 100 | Array loop accumulation (const_prop + fold_imm bug) |
| `examples/test_argv.cst` | prints args | prints args | Argv parsing (const_prop in loops) |
| `examples/fibonacci.cst` | Exit 0, prints fib values | Exit 0 | Recursive calls + conditional returns (store_load_fwd) |
| `examples/linked_list.cst` | Exit 0, prints list ops | Exit 0 | Heap alloc + pointer chasing (store_load_fwd + const_prop) |
| `examples/bench.cst` | Exit 0, all 5 benchmarks | Exit 0 | Complex loops, array ops, function calls |
| `-O1 self-bootstrap` | gen8 0-diff | gen8 0-diff | Full compiler correctness under optimization |
