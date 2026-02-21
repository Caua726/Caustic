# Available Registers

This document describes the x86_64 register usage in Caustic's code generator.

## General-Purpose Register Table

Caustic indexes 12 general-purpose registers from 0 to 11:

| Index | 64-bit | 32-bit | 16-bit | 8-bit | Role |
|-------|--------|--------|--------|-------|------|
| 0 | rax | eax | ax | al | Return value, division, scratch |
| 1 | rcx | ecx | cx | cl | Shift count, 4th call arg |
| 2 | rdx | edx | dx | dl | Division remainder, 3rd call arg |
| 3 | rbx | ebx | bx | bl | **Allocatable** (callee-saved) |
| 4 | rsi | esi | si | sil | 2nd call arg, rep movsb source |
| 5 | rdi | edi | di | dil | 1st call arg, rep movsb dest |
| 6 | r8 | r8d | r8w | r8b | 5th call arg |
| 7 | r9 | r9d | r9w | r9b | 6th call arg |
| 8 | r10 | r10d | r10w | r10b | Context register (SET_CTX) |
| 9 | r11 | r11d | r11w | r11b | Scratch (caller-saved) |
| 10 | r12 | r12d | r12w | r12b | **Allocatable** (callee-saved) |
| 11 | r13 | r13d | r13w | r13b | **Allocatable** (callee-saved) |

## Reserved Registers (Not in the 12-register table)

| Register | Purpose |
|----------|---------|
| rsp | Stack pointer (hardware-managed) |
| rbp | Frame pointer (base of current stack frame) |
| r14 | Scratch register 1 (used by codegen for operand loads) |
| r15 | Scratch register 2 (used by codegen for operand loads, large immediates) |

## Allocatable Registers

The linear scan register allocator only assigns vregs to three callee-saved registers:

- **rbx** (index 3)
- **r12** (index 10)
- **r13** (index 11)

These are callee-saved, meaning their values are preserved across function calls. This is important because any vreg assigned to one of these registers remains valid after an `IR_CALL` instruction without needing save/restore logic around calls.

The remaining registers in the table (rax, rcx, rdx, rsi, rdi, r8-r11) are not used for allocation because they are either caller-saved (clobbered by function calls) or have dedicated purposes (argument passing, division, shifts).

## Scratch Registers

`r14` and `r15` are reserved exclusively for the codegen's use as temporary storage during instruction emission. Every IR instruction follows this pattern:

1. Load source operands from their allocated locations (registers or spill slots) into r14/r15.
2. Perform the operation using r14/r15.
3. Store the result from r14/r15 back to the destination's allocated location.

For example:
```asm
-- IR_ADD dest, src1, src2
mov r15, <src1_loc>       ; load first operand
mov r14, <src2_loc>       ; load second operand
add r15, r14              ; compute
mov <dest_loc>, r15       ; store result
```

`r15` is also used for large immediate loads (values that do not fit in a 32-bit sign-extended immediate) when the destination is a spill slot.

## Callee-Saved vs Caller-Saved

### Callee-Saved (preserved across calls)
rbx, r12, r13, r14, r15, rbp

Caustic's function prologue pushes all five (rbx, r12, r13, r14, r15) unconditionally, regardless of whether they are used. This simplifies codegen but wastes 40 bytes of stack per function frame.

### Caller-Saved (clobbered by calls)
rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11

These registers may be modified by any function call. Since the allocator only uses callee-saved registers, no save/restore logic is needed around `IR_CALL` instructions.

## Function Call Argument Registers

System V AMD64 ABI integer argument order:

| Arg Index | Register |
|-----------|----------|
| 0 | rdi |
| 1 | rsi |
| 2 | rdx |
| 3 | rcx |
| 4 | r8 |
| 5 | r9 |

Arguments beyond index 5 are pushed onto the stack in reverse order.

## Syscall Argument Registers

Linux x86_64 syscall convention:

| Arg Index | Register | Purpose |
|-----------|----------|---------|
| 0 | rax | Syscall number |
| 1 | rdi | Arg 1 |
| 2 | rsi | Arg 2 |
| 3 | rdx | Arg 3 |
| 4 | r10 | Arg 4 |
| 5 | r8 | Arg 5 |
| 6 | r9 | Arg 6 |

Note that the fourth argument uses `r10` instead of `rcx` (which the `syscall` instruction overwrites with the return address).

## XMM Registers

SSE registers `xmm0` through `xmm15` are available but are not managed by the register allocator. They are used transiently during float operations:

- `xmm0` and `xmm1` are used as temporaries for float arithmetic (addsd, subsd, mulsd, divsd).
- `xmm0` is used for float-to-int and int-to-float conversions (cvttsd2si, cvtsi2sd).

Float values are stored in GPRs as their i64 bit representation and transferred to XMM registers only for the duration of the float operation via `movq`.
