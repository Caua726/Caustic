# Available Registers

This document describes the x86_64 register usage in Caustic's code generator.

## Allocatable Registers (10)

The register allocator assigns virtual registers to 10 physical registers, split into two groups based on calling convention behavior.

### Callee-Saved (5 registers)

Always available. Values preserved across function calls.

| Index | Register | Alloc Slot |
|-------|----------|------------|
| 3 | rbx | 0 |
| 10 | r12 | 1 |
| 11 | r13 | 2 |
| 12 | r14 | 3 |
| 15 | r15 | 9 |

### Caller-Saved (5 registers)

Only assigned to intervals that **do not span any call or SET_ARG instruction**. The allocator uses binary search (`spans_call()`) to determine if a live interval crosses a call position.

| Index | Register | Alloc Slot |
|-------|----------|------------|
| 6 | r8 | 4 |
| 7 | r9 | 5 |
| 8 | r10 | 6 |
| 4 | rsi | 7 |
| 5 | rdi | 8 |

### Allocation Priority

The allocator tries registers in this order: callee-saved first (rbx, r12, r13, r14, r8, r9, r10, rsi, rdi, r15). Callee-saved are preferred because they work across calls. A hint register (from MOV coalescing) is tried before the priority list.

## Reserved Registers (5)

| Register | Purpose |
|----------|---------|
| rax | Scratch, return value, division (idiv/div uses rax:rdx) |
| rcx | Shift count (shl/shr need cl), 4th call arg |
| rdx | Division remainder, 3rd call arg |
| rsp | Stack pointer |
| rbp | Frame pointer |

These are never assigned by the allocator. `rax` is used as a scratch register when the destination vreg is spilled.

## Callee-Saved Save/Restore

The function prologue only pushes callee-saved registers that are actually used (`used_callee_mask` tracks this):

| Bit | Register |
|-----|----------|
| 0 | rbx |
| 1 | r12 |
| 2 | r13 |
| 3 | r14 |
| 4 | r15 |

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

Note: the fourth argument uses `r10` instead of `rcx` (which the `syscall` instruction overwrites with the return address).

## XMM Registers

SSE registers `xmm0` through `xmm15` are not managed by the register allocator. They are used transiently during float operations:

- `xmm0` and `xmm1` as temporaries for float arithmetic (addsd, subsd, mulsd, divsd)
- `xmm0` for float-to-int and int-to-float conversions (cvttsd2si, cvtsi2sd)

Float values are stored in GPRs as their i64 bit representation and transferred to XMM registers via `movq` only for the duration of the float operation.
