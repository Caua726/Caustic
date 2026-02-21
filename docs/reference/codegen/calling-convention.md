# Calling Convention

Caustic follows the System V AMD64 ABI for all function calls. This ensures compatibility with
`extern fn` declarations and with C libraries when linking dynamically.

## Integer Argument Registers

The first six integer/pointer arguments are passed in registers, in this order:

| Arg Index | Register |
|-----------|----------|
| 0 | rdi |
| 1 | rsi |
| 2 | rdx |
| 3 | rcx |
| 4 | r8 |
| 5 | r9 |

Arguments at index 6 and beyond are pushed onto the stack in reverse order (rightmost argument
pushed first) before the `call` instruction.

## Stack-Passed Arguments

For argument index `idx >= 6`, the emitter pushes the value:

```asm
-- IR_SET_ARG with idx >= 6
mov r15, <value_loc>
push r15
```

On the callee side, stack arguments are accessed relative to `rbp`:

```asm
-- IR_GET_ARG with idx >= 6
-- offset = 16 + (idx - 6) * 8
mov r15, QWORD PTR [rbp+16]    ; arg 6
mov r15, QWORD PTR [rbp+24]    ; arg 7
```

The `+16` accounts for the return address (`+8`) and saved `rbp` (`+8`) pushed by the standard
prologue sequence.

## Return Values

| Type | Register |
|------|----------|
| Integer (any width) | rax |
| Pointer | rax |
| Float (f32/f64) | rax (bit-pattern, not xmm0) |
| Struct <= 8 bytes | rax |
| Struct 9-16 bytes | rax (low 8) + rdx (high 8) |
| Struct > 16 bytes | SRET (hidden pointer argument, see sret.md) |

Note: Caustic does not return floats in `xmm0`. Float values are stored as their i64 bit
representation in `rax`. This deviates from the System V ABI for Caustic-to-Caustic calls but
is documented for `extern fn` interoperability purposes (where xmm0 is the standard).

## Stack Alignment

The System V AMD64 ABI requires the stack pointer to be 16-byte aligned at the `call`
instruction (i.e., `rsp % 16 == 0` just before `call` executes).

Caustic's prologue pushes 6 items (return addr + rbp + rbx + r12 + r13 + r14 + r15 = 56 bytes),
which is 8 mod 16. The `sub rsp, N` in the prologue must therefore make `N` satisfy
`N % 16 == 8`, so that `rsp` is aligned before any inner `call`.

The alignment calculation in `gen_func`:

```cst
if (total % 16 != 8) {
    total = total + (8 - (total % 8));
    if (total % 16 != 8) { total = total + 8; }
}
if (total < 8) { total = 8; }
```

## Callee-Saved Register Contract

The caller expects rbx, r12, r13, r14, r15, and rbp to be preserved across a call. Caustic's
prologue saves all five (rbx, r12, r13, r14, r15) unconditionally and restores them in the
epilogue. The caller-saved registers (rax, rcx, rdx, rsi, rdi, r8-r11) may be modified freely
by the callee.

Because the register allocator only assigns vregs to callee-saved registers (rbx, r12, r13),
no special save/restore logic is needed around `IR_CALL` instructions. Any vreg live across a
call is guaranteed to survive it.

## Syscall Convention

Linux x86_64 syscalls use a different register order than function calls:

| Slot | Register | Role |
|------|----------|------|
| 0 | rax | Syscall number |
| 1 | rdi | Argument 1 |
| 2 | rsi | Argument 2 |
| 3 | rdx | Argument 3 |
| 4 | r10 | Argument 4 (note: not rcx) |
| 5 | r8 | Argument 5 |
| 6 | r9 | Argument 6 |

The `syscall` instruction clobbers `rcx` and `r11` and returns the result in `rax`.

## Example: Call Sequence

```asm
-- Calling: result = write(fd, buf, len)
-- IR_SET_ARG 0: fd
mov r15, rbx                 ; load fd from rbx (allocated vreg)
mov rdi, r15

-- IR_SET_ARG 1: buf
mov r15, QWORD PTR [rbp-8]   ; load buf from spill slot
mov rsi, r15

-- IR_SET_ARG 2: len
mov r15, r12                 ; load len from r12 (allocated vreg)
mov rdx, r15

-- IR_CALL write
call write

-- IR_result stored from rax
mov r13, rax                 ; store result into r13 (allocated vreg)
```
