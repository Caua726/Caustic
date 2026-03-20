# Register Cache & SIB Addressing Mode Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-enable register cache and `[base + index*scale]` addressing modes to recover ~30% codegen performance.

**Architecture:** Two independent features: (1) fix register cache invalidation so it's correct for self-bootstrap, (2) add SIB byte support to caustic-as so addressing modes work.

**Tech Stack:** Caustic self-hosted compiler (.cst), x86_64 assembly, ELF

**Current state:** Both features are implemented but disabled. Register cache (`rc_find` returns -1). Addressing modes (`&& 0 == 1` hack). -O1 gen4 bootstrap works at 873KB/1391ms. Target: ~938ms (matching pre-regression).

---

## Feature A: Register Cache

### Problem

The register cache (`rc_vreg[16]`) tracks which physical register holds which spilled vreg. On `load_op`, if the vreg is already cached, skip the memory load. This eliminates redundant `mov reg, QWORD PTR [rbp-X]` instructions.

**Why it's disabled:** When enabled, gen2-O1 produces a gen3 that crashes. The gen3 binary is 846KB (different from gen2's 873KB), proving the cache produces different (incorrect) code when the compiled program is large enough.

**Root cause:** Some codegen path writes to a physical register without invalidating the cache. The cache then returns stale data on a subsequent `load_op`.

### Known invalidation points (already implemented)

- `gen_inst` entry: rax(0), rcx(1), rdx(2) cleared
- `gen_inst` entry: dest register cleared (if dest is OP_VREG)
- `gen_inst` entry: SET_ARG argument registers cleared
- `ph_invalidate()` (full clear): labels, jumps, calls, syscalls
- `store_op`: sets cache for source register
- `load_op`: sets cache for target register
- `IR_COPY`: rdi(5), rsi(4), rcx(1) cleared after rep movsb
- `IR_SET_CTX`: r10(8) cleared on IMM path

### Task A1: Audit every direct register write

**Files:**
- Read: `src/codegen/emit.cst`

Every `out_cstr("  mov ...")`, `out_cstr("  add ...")`, etc. that writes to a named register WITHOUT going through `load_op`/`store_op` is a potential cache bug. Systematically audit:

- [ ] **Step 1:** grep all `out_cstr` calls that emit x86 instructions writing to specific registers. Build a list of (function, register written, line number).

```bash
grep -n 'out_cstr.*"  mov \|out_cstr.*"  add \|out_cstr.*"  sub \|out_cstr.*"  lea \|out_cstr.*"  xor \|out_cstr.*"  imul \|out_cstr.*"  shl \|out_cstr.*"  shr \|out_cstr.*"  sar \|out_cstr.*"  and \|out_cstr.*"  or \|out_cstr.*"  neg \|out_cstr.*"  not \|out_cstr.*"  set\|out_cstr.*"  cqo\|out_cstr.*"  div\|out_cstr.*"  idiv\|out_line.*"push\|out_line.*"pop\|out_line.*"cld\|out_line.*"rep\|out_line.*"syscall\|out_line.*"xor e' src/codegen/emit.cst
```

- [ ] **Step 2:** For each register write found, check if the cache is invalidated for that register. The categories:

| Write pattern | Register affected | Already invalidated? |
|--------------|-------------------|---------------------|
| `out_line("xor eax, eax")` | rax(0) | Yes (scratch) |
| `out_line("cqo")` | rdx(2) | Yes (scratch) |
| `out_line("push rdx")` / `out_line("pop rdx")` | rdx(2) | Yes (scratch) |
| `out_line("syscall")` | rax(0), rcx(1), r11(9) | rax,rcx yes; r11 **NO** |
| `out_cstr("  mov rcx, ")` in gen_inst_arith | rcx(1) | Yes (scratch) |
| `out_cstr("  shl "); out_cstr(scr)` | scr (varies) | **MAYBE NOT** |
| `gen_binop` conflict path: writes rax | rax(0) | Yes (scratch) |
| `gen_inst_float`: `movq xmm0/xmm1, ...` | No GPR affected | OK |
| `gen_inst_compare`: `out_line("xor eax, eax")` | rax(0) | Yes |
| `emit_mov_imm`: writes to dest or rax | dest **MAYBE NOT** |

- [ ] **Step 3:** For every "MAYBE NOT" or "NO", add `rc_invalidate_reg(idx)` after the write.

Key suspects:
1. **`syscall` clobbers r11(9)** — not invalidated
2. **`get_scratch`/`get_scratch_idx`** returns the dest register, then arithmetic instructions write to it. The dest register IS invalidated at gen_inst entry (the fix we added). But `get_scratch` can also return rax(0) when dest is spilled — and that's already invalidated.
3. **`gen_binop`** loads src1 into dest register, then does the operation in-place. After the operation, the register no longer holds src1 — it holds the result. But `store_op` is called after, which sets the cache for the RESULT vreg. The issue: `load_op(src1, dr)` sets cache entry `dr → src1`. Then `add dr, src2` changes dr's value. Now cache says `dr → src1` but dr actually holds `dest`. Then `store_op(dest, dr)` updates cache to `dr → dest`. This should be correct IF store_op properly clears the old entry. Let me check...

- [ ] **Step 4:** Verify `store_op` properly handles the transition. In `store_op` for register-allocated vregs:
```
mov rn, source   // rn = physical reg of dest vreg
```
The cache should be: `rn → dest_vreg`. But `rc_invalidate_reg(sidx)` only clears the SOURCE register's cache. It does NOT clear `rn`'s old cache entry if `rn` had a different vreg cached. **This is likely the bug.** `store_op` for register-allocated case needs `rc_invalidate_reg(loc)` where loc is the dest's physical register.

Check if line 360 (`rc_invalidate_reg(loc)`) exists and is correct.

- [ ] **Step 5:** After all fixes, enable `rc_find` (restore the loop body).

- [ ] **Step 6:** Test:
```bash
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
/tmp/gen1 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/g2
/tmp/g2 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/g3
/tmp/g3 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/g4
# gen2, gen3, gen4 must all be same size
# gen4 -O1 examples must all pass
```

### Task A2: Targeted test for cache correctness

- [ ] **Step 1:** Create a test that exercises the exact pattern that breaks:
```cst
fn is_alpha(c as u8) as i32 {
    if (c >= 65 && c <= 90) { return 1; }
    if (c >= 97 && c <= 122) { return 1; }
    if (c == 95) { return 1; }
    return 0;
}
```
This function has multiple return paths writing to the same vreg. Compile with -O0 and -O1, compare assembly. The -O1 version with cache should NOT skip any `mov rax, 0` or `mov rax, 1` loads.

---

## Feature B: SIB Addressing Mode in caustic-as

### Problem

The codegen emits `mov rsi, QWORD PTR [rax + rcx * 8]` for array access. This uses x86 SIB (Scale-Index-Base) byte encoding: `[base + index * scale + disp]`. The caustic-as assembler doesn't support this — it only handles `[base + disp]`.

**Why it's disabled:** `src/codegen/emit.cst` has `&& 0 == 1` on the addressing mode match.

### x86_64 SIB encoding reference

For `[base + index * scale]`:
- ModRM byte: mod=00, reg=dest_reg, rm=100 (signals SIB follows)
- SIB byte: `(log2(scale) << 6) | (index_reg << 3) | base_reg`
- Scales: 1→00, 2→01, 4→10, 8→11
- Register codes: rax=0, rcx=1, rdx=2, rbx=3, rsp=4(special), rbp=5, rsi=6, rdi=7, r8-r15=0-7 with REX.X/REX.B

For `[base + index * scale + disp8]`:
- ModRM: mod=01, rm=100
- SIB byte same
- disp8 follows

For `[base + index * scale + disp32]`:
- ModRM: mod=10, rm=100
- SIB byte same
- disp32 follows

### Task B1: Add index/scale fields to ParsedLine

**Files:**
- Modify: `caustic-assembler/main.cst:101-135`

- [ ] **Step 1:** Add fields to `ParsedLine` struct:
```
    op1_mem_index as i32;   // index register id (99 = none)
    op1_mem_scale as i32;   // scale: 1, 2, 4, or 8 (0 = no index)
    ...
    op2_mem_index as i32;
    op2_mem_scale as i32;
```

- [ ] **Step 2:** Initialize to 0/99 in `pl_new_line` (or equivalent init function).

- [ ] **Step 3:** Update `set_op_mem` to accept index and scale parameters. Create `set_op_mem_sib` variant:
```
fn set_op_mem_sib(line, which, base, index, scale, disp, sz)
```

### Task B2: Parse `[base + index * scale]` syntax

**Files:**
- Modify: `caustic-assembler/main.cst:255-290` (`parse_mem_operand`)

- [ ] **Step 1:** After reading `base_reg` and seeing `+`, check if the next token is a register (not a number). If so, it's `[base + index ...]`:

```
[base + index * scale]
[base + index * scale + disp]
[base + index * scale - disp]
```

- [ ] **Step 2:** Parse the `* scale` part: expect TK_STAR (need to add to lexer if missing), then a number (1, 2, 4, or 8).

- [ ] **Step 3:** Optionally parse `+ disp` or `- disp` after the scale.

- [ ] **Step 4:** Call `set_op_mem_sib(line, which, base_reg, index_reg, scale, disp, mem_size)`.

### Task B3: Add TK_STAR to lexer (if missing)

**Files:**
- Modify: `caustic-assembler/lexer.cst`

- [ ] **Step 1:** Check if `*` (ASCII 42) is already tokenized. If not, add `TK_STAR` constant and lexer case.

### Task B4: Encode SIB in emit_mem_modrm

**Files:**
- Modify: `caustic-assembler/encoder.cst:291-317`

- [ ] **Step 1:** Add `emit_mem_modrm_sib(b, reg_field, base_id, index_id, scale, disp)`:

```
fn emit_mem_modrm_sib(b, reg_field, base_id, index_id, scale, disp):
    base_code = reg_code(base_id)
    index_code = reg_code(index_id)
    scale_bits = log2(scale)  // 1→0, 2→1, 4→2, 8→3

    // REX prefix: need REX.X if index is r8-r15, REX.B if base is r8-r15

    if disp == 0 && base_code != 5:
        modrm = make_modrm(0, reg_field, 4)  // rm=4 signals SIB
        sib = make_sib(scale_bits, index_code, base_code)
        emit(modrm, sib)
    elif disp_is8(disp):
        modrm = make_modrm(1, reg_field, 4)
        sib = make_sib(scale_bits, index_code, base_code)
        emit(modrm, sib, disp8)
    else:
        modrm = make_modrm(2, reg_field, 4)
        sib = make_sib(scale_bits, index_code, base_code)
        emit(modrm, sib, disp32)
```

- [ ] **Step 2:** Add `mem_modrm_sib_size` for pass 1 (size calculation).

- [ ] **Step 3:** Update the `encode` function to detect SIB operands and call `emit_mem_modrm_sib` instead of `emit_mem_modrm`.

- [ ] **Step 4:** Handle REX prefix for extended registers (r8-r15 as index/base need REX.X/REX.B bits).

### Task B5: Enable addressing modes in codegen

**Files:**
- Modify: `src/codegen/emit.cst`

- [ ] **Step 1:** Remove the `&& 0 == 1` hack from the addressing mode match.

- [ ] **Step 2:** Test:
```bash
./caustic -O1 examples/test_arr.cst && ./caustic-as examples/test_arr.cst.s && ./caustic-ld examples/test_arr.cst.s.o -o /tmp/ta && /tmp/ta
# Must exit 100
```

### Task B6: Validation

- [ ] **Step 1:** Compare caustic-as output with GNU as for a SIB test:
```bash
echo '.intel_syntax noprefix
.text
.globl main
main:
  lea rax, [rbp-32]
  mov rcx, 1
  mov rsi, QWORD PTR [rax + rcx * 8]
  mov rax, 60
  mov rdi, rsi
  syscall' > /tmp/sib_test.s

# GNU as
as -o /tmp/sib_gnu.o /tmp/sib_test.s
objdump -d /tmp/sib_gnu.o | grep "mov.*rsi"

# caustic-as
./caustic-as /tmp/sib_test.s
objdump -d /tmp/sib_test.s.o | grep "mov.*rsi"

# Byte sequences must match
```

- [ ] **Step 2:** Full -O1 gen4 bootstrap with addressing modes enabled.

- [ ] **Step 3:** Benchmark comparison:
```bash
# Without addressing modes
./caustic -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && ./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b
```

---

## Priority

1. **Feature B (SIB)** — more impactful, independent of cache, mechanical work
2. **Feature A (cache)** — harder to debug, higher risk, needs systematic audit

## Expected Performance Impact

| Feature | Code size impact | Runtime impact |
|---------|-----------------|----------------|
| SIB addressing modes | -5% instructions in loops | -10-15% on array-heavy benchmarks |
| Register cache | -15-20% mov instructions for spilled vregs | -15-25% overall |
| Both combined | | Target: ~938ms (vs current 1391ms) |
