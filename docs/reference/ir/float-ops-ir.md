# Float Operations in IR

Caustic's IR separates floating-point operations from integer operations using dedicated opcodes. The IR generator selects float opcodes when the expression type is `f32` or `f64`, determined by the `is_float_kind()` helper.

## Float Opcodes

| Opcode | Value | Operation | Codegen Instruction |
|--------|-------|-----------|-------------------|
| IR_FADD | 41 | Addition | `addsd xmm0, xmm1` |
| IR_FSUB | 42 | Subtraction | `subsd xmm0, xmm1` |
| IR_FMUL | 43 | Multiplication | `mulsd xmm0, xmm1` |
| IR_FDIV | 44 | Division | `divsd xmm0, xmm1` |
| IR_FNEG | 45 | Negation | `xorpd xmm0, xmm1` (sign bit flip) |

All float opcodes use the same operand layout as their integer counterparts:
- `dest`: V(d) -- destination vreg
- `src1`: V(a) -- first operand
- `src2`: V(b) -- second operand (unused for IR_FNEG)

## Float Value Representation

Caustic stores floating-point values as their IEEE 754 bit pattern in i64-width virtual registers. There is no separate float register class in the IR. This means:

1. **Float literals**: `NK_NUM` nodes with float type store their value as `fval` (f64). The IR generator converts this to an i64 bit pattern via `f64_to_i64()` and emits it as a regular `IR_IMM`.

2. **Vreg storage**: Float vregs hold 64-bit values that are the bit representation of the float. They are allocated, spilled, and loaded the same way as integer vregs.

3. **Codegen transfer**: Before any float operation, codegen must transfer the value from a GPR to an XMM register using `movq`, perform the SSE operation, then transfer the result back to a GPR.

## Codegen Pattern for Float Binary Operations

All four binary float operations (FADD, FSUB, FMUL, FDIV) follow the same pattern:

```asm
-- Load operands from vreg locations into GPRs
mov r14, <src1_location>
mov r15, <src2_location>

-- Transfer to XMM registers (bit-pattern copy, no conversion)
movq xmm0, r14
movq xmm1, r15

-- Perform the SSE operation
addsd xmm0, xmm1       ; or subsd, mulsd, divsd

-- Transfer result back to GPR
movq r14, xmm0

-- Store to destination vreg location
mov <dest_location>, r14
```

The `movq` instruction performs a bitwise copy between a 64-bit GPR and the low 64 bits of an XMM register without any conversion.

## Codegen Pattern for Float Negation

Float negation flips the sign bit (bit 63) using XOR:

```asm
mov r14, <src1_location>
movq xmm0, r14

-- Load sign bit mask
mov r15, 0x8000000000000000
movq xmm1, r15

-- XOR flips the sign bit
xorpd xmm0, xmm1

movq r14, xmm0
mov <dest_location>, r14
```

## Float Selection in the IR Generator

The IR generator checks `is_float_kind(ty.kind)` for arithmetic operations to select between integer and float opcodes:

```cst
if (is_float_kind(ty.kind) == 1) { return emit_binary(d.IR_FADD, lhs, rhs, line); }
return emit_binary(d.IR_ADD, lhs, rhs, line);
```

This check occurs for addition, subtraction, multiplication, division, and negation. Modulo is not supported for floats. Comparisons do not have float-specific opcodes -- they use the same integer comparison opcodes, though this means float comparisons use integer `cmp` which may produce incorrect results for edge cases like NaN.

## Float Type Casting

Conversions between float and integer types are handled by `IR_CAST`:

- **int to float**: Codegen emits `cvtsi2sd xmm0, r14` followed by `movq r14, xmm0`.
- **float to int**: Codegen emits `movq xmm0, r14` followed by `cvttsd2si r14, xmm0`. The `cvttsd2si` instruction truncates toward zero.
- **float to float** (f32 to f64 or vice versa): Currently not differentiated at the codegen level; all operations use `sd` (scalar double) instructions. f32 support is present in the type system but codegen treats all floats as f64.

## Caustic Float ABI

Unlike the System V ABI, Caustic does not pass or return float values in XMM registers for internal function calls. Float values are passed in GPRs as their i64 bit representations, just like integer values. The return value similarly comes back in `rax`. This simplifies the calling convention at the cost of requiring GPR-to-XMM transfers at every float operation site.
