# Float Codegen

Caustic uses SSE2 instructions for all floating-point arithmetic. Float values (f32 and f64)
are stored as their raw bit patterns in general-purpose registers (GPRs) and on the stack.
They are transferred into XMM registers only for the duration of individual arithmetic or
comparison operations.

## Value Representation

Float values are never stored directly in XMM registers between IR instructions. Instead:

- In an allocated vreg (rbx, r12, r13): the float bit pattern is in the GPR as an integer.
- In a spill slot: stored as 8 bytes on the stack (`QWORD PTR [rbp-offset]`).
- In transit through scratch: `r14` or `r15` hold the bit pattern.

The `movq` instruction transfers between GPRs and XMM registers without conversion:

```asm
movq xmm0, r14    ; move bit pattern from GPR to XMM (no conversion)
movq r14, xmm0    ; move bit pattern from XMM to GPR (no conversion)
```

## Float Arithmetic (IR_FADD, IR_FSUB, IR_FMUL, IR_FDIV)

All four binary float operations follow the same pattern:

```asm
; IR_FADD dest, src1, src2
mov r14, <src1_loc>       ; load src1 bit pattern
mov r15, <src2_loc>       ; load src2 bit pattern
movq xmm0, r14            ; move to XMM for float op
movq xmm1, r15
addsd xmm0, xmm1          ; f64 addition (result in xmm0)
movq r14, xmm0            ; move result back to GPR
mov <dest_loc>, r14       ; store result
```

The same pattern applies for the other operations:

| IR Opcode | SSE Instruction (f64) |
|-----------|----------------------|
| IR_FADD | addsd xmm0, xmm1 |
| IR_FSUB | subsd xmm0, xmm1 |
| IR_FMUL | mulsd xmm0, xmm1 |
| IR_FDIV | divsd xmm0, xmm1 |

Note: Caustic's current codegen always uses the `sd` (scalar double) variants, meaning f32
values are also operated on as f64 at the instruction level. Precision differences are generally
not visible in practice because values are stored and loaded at 64-bit width throughout.

## Float Negation (IR_FNEG)

Float negation flips the sign bit using `xorpd` with a constant mask:

```asm
mov r14, <src1_loc>
movq xmm0, r14
mov r15, 0x8000000000000000    ; sign bit mask
movq xmm1, r15
xorpd xmm0, xmm1              ; flip sign bit
movq r14, xmm0
mov <dest_loc>, r14
```

This avoids any dedicated float negation instruction and works for both f32 and f64 since the
sign bit is always the MSB of the 64-bit representation.

## Float Comparison

Float comparisons use the same `cmp` + `setCC` pattern as integer comparisons, operating on the
bit patterns stored in GPRs:

```asm
mov r15, <src1_loc>
mov r14, <src2_loc>
xor rax, rax
cmp r15, r14
setCC al                   ; sete / setne / setl / setle / setg / setge
mov <dest_loc>, rax
```

Note: This applies integer bit-pattern comparison. This is correct for well-ordered finite
values. For full IEEE 754 NaN semantics, `ucomisd` would be required, but Caustic does not
currently use it.

## Int-to-Float Conversion (IR_CAST int -> float)

```asm
mov r14, <src1_loc>        ; integer value in GPR
cvtsi2sd xmm0, r14         ; convert 64-bit signed integer to f64
movq r14, xmm0             ; move result bit pattern to GPR
mov <dest_loc>, r14
```

`cvtsi2sd` converts a 64-bit signed integer to a double-precision float.

## Float-to-Int Conversion (IR_CAST float -> int)

```asm
mov r14, <src1_loc>        ; float bit pattern in GPR
movq xmm0, r14             ; move to XMM for conversion
cvttsd2si r14, xmm0        ; truncate f64 to 64-bit signed integer
mov <dest_loc>, r14
```

`cvttsd2si` truncates toward zero (matching C's `(int)` cast behavior).

## Float Loads and Stores

Float values are loaded and stored using the same `IR_LOAD` / `IR_STORE` machinery as integers.
Since float values are stored as 8-byte bit patterns, all float loads and stores use 8-byte
(QWORD) memory operations:

```asm
; Load f64 from pointer
mov r15, <addr_loc>
mov r15, QWORD PTR [r15]
mov <dest_loc>, r15

; Store f64 to pointer
mov rax, <value_loc>
mov r15, <addr_loc>
mov QWORD PTR [r15], rax
```

## Float Return Values

Caustic functions return float values in `rax` as an i64 bit pattern. This differs from the
System V AMD64 ABI which returns floats in `xmm0`. When interoperating with C code via
`extern fn`, this distinction matters:

- **Caustic-to-Caustic calls**: float in `rax` (bit pattern). `IR_CALL` stores `rax` to the
  destination vreg directly.
- **Caustic calling C (extern fn)**: the C function returns a float in `xmm0`. The value in
  `rax` will be incorrect. A `movq` from `xmm0` after the call is needed to extract the result.
- **C calling Caustic**: Caustic places the float in `rax`, but C expects it in `xmm0`. Avoid
  exposing float-returning Caustic functions as `extern` entry points without a wrapper.

## Example: f64 Arithmetic

```cst
fn add_f64(a as f64, b as f64) as f64 {
    return a + b;
}
```

Key portion of generated assembly (simplified):

```asm
add_f64:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 8
  ; GET_ARG 0 -> rbx (a)
  mov r15, rdi
  mov rbx, r15
  ; GET_ARG 1 -> r12 (b)
  mov r15, rsi
  mov r12, r15
  ; IR_FADD: a + b
  mov r14, rbx
  mov r15, r12
  movq xmm0, r14
  movq xmm1, r15
  addsd xmm0, xmm1
  movq r14, xmm0
  mov r13, r14
  ; IR_RET
  mov rax, r13
  add rsp, 8
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
```
