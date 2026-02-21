# Instruction Encoding

The `encoder.cst` file contains the core x86_64 instruction encoder. It translates parsed instructions (identified by instruction ID and operand types) into raw machine code bytes. The encoder handles REX prefixes, opcode selection, ModR/M bytes, SIB bytes, displacements, and immediates.

## x86_64 Instruction Format

An x86_64 instruction can contain up to six components, all optional except the opcode:

```
[Prefix] [REX] [Opcode] [ModR/M] [SIB] [Displacement] [Immediate]
```

| Component | Size | Description |
|-----------|------|-------------|
| Prefix | 1 byte | Operand size override (0x66 for 16-bit), SSE prefix (0xF2 for scalar double) |
| REX | 1 byte | 64-bit operand size (W), register extensions (R, X, B) |
| Opcode | 1-2 bytes | Instruction identifier. Some use a 0x0F escape byte for two-byte opcodes |
| ModR/M | 1 byte | Addressing mode (mod), register operand (reg), register/memory operand (r/m) |
| SIB | 1 byte | Scale-Index-Base for complex memory addressing |
| Displacement | 1 or 4 bytes | Memory offset (disp8 or disp32) |
| Immediate | 1, 2, 4, or 8 bytes | Constant operand value |

## REX Prefix

The REX prefix byte has the format `0100 WRXB` (0x40-0x4F):

```
REX.W = 1: 64-bit operand size
REX.R = 1: extends ModR/M reg field (accesses r8-r15)
REX.X = 1: extends SIB index field
REX.B = 1: extends ModR/M r/m field or SIB base field (accesses r8-r15)
```

The encoder's `make_rex(w, r, x, b)` function computes: `64 + w*8 + r*4 + x*2 + b`.

A REX prefix is required when:
- Using 64-bit operand size (REX.W = 1)
- Accessing registers r8-r15 in any position (REX.R, REX.X, or REX.B = 1)
- Using the new 8-bit registers spl, bpl, sil, dil (forces REX even with W=R=X=B=0)

## ModR/M Byte

The ModR/M byte encodes addressing mode and register selections:

```
Bits 7-6: mod  (addressing mode)
Bits 5-3: reg  (register operand or opcode extension)
Bits 2-0: r/m  (register or memory operand)
```

| mod | Meaning |
|-----|---------|
| 00 | `[r/m]` -- indirect, no displacement (except rbp/r13 which use disp32) |
| 01 | `[r/m + disp8]` -- indirect with 8-bit displacement |
| 10 | `[r/m + disp32]` -- indirect with 32-bit displacement |
| 11 | Direct register (r/m is a register, not memory) |

Special cases:
- `r/m = 4` (rsp/r12): requires a SIB byte for memory operands
- `r/m = 5` with `mod = 00`: RIP-relative addressing `[rip + disp32]`
- `r/m = 5` with `mod = 00` (rbp base): must use `mod = 01` with `disp8 = 0`

The encoder's `make_modrm(m, reg, rm)` computes: `(m%4)*64 + (reg%8)*8 + (rm%8)`.

## SIB Byte

The SIB byte is used when the ModR/M r/m field is 4 (indicating "SIB follows"):

```
Bits 7-6: scale (00=1, 01=2, 10=4, 11=8)
Bits 5-3: index register
Bits 2-0: base register
```

The assembler uses SIB primarily for stack-relative addressing (when the base register is rsp/r12, which encodes as 4). In this case, the SIB byte is `make_sib(0, 4, base_code)` where index=4 means "no index register" (the special encoding for SIB with rsp as index).

## Memory Operand Encoding

The `emit_mem_modrm()` function handles all memory operand encoding:

1. **No displacement** (`disp == 0` and base is not rbp/r13): `mod=00`, just ModR/M (+ SIB if base is rsp/r12).
2. **8-bit displacement** (`-128 <= disp <= 127`): `mod=01`, ModR/M + disp8 (+ SIB if needed).
3. **32-bit displacement**: `mod=10`, ModR/M + disp32 (+ SIB if needed).

The function `mem_modrm_size()` calculates the byte count without emitting, used during pass 1 for size estimation.

## Register Encoding

Registers are identified by numeric IDs organized into groups:

| ID Range | Size | Registers |
|----------|------|-----------|
| 0-15 | 64-bit | rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8-r15 |
| 16-31 | 32-bit | eax, ecx, edx, ebx, esp, ebp, esi, edi, r8d-r15d |
| 32-47 | 16-bit | ax, cx, dx, bx, sp, bp, si, di, r8w-r15w |
| 48-63 | 8-bit | al, cl, dl, bl, spl, bpl, sil, dil, r8b-r15b |
| 64-79 | 128-bit (SSE) | xmm0-xmm15 |
| 99 | special | rip (instruction pointer, used in RIP-relative addressing) |
| 255 | special | REG_NONE (no register) |

Helper functions:
- `reg_code(id)`: Returns the 3-bit encoding (0-7) used in ModR/M/SIB fields. Computed as `id % 8` (with adjustment for XMM: `(id - 64) % 8`).
- `reg_ext(id)`: Returns 1 if the register needs the REX extension bit (registers 8-15 in each group).
- `reg_size(id)`: Returns the operand size in bits (8, 16, 32, 64, or 128 for XMM).
- `reg_base(id)`: Returns the base register ID (maps 32/16/8-bit variants to the 64-bit ID).

## Operand Types

```cst
struct Operand {
    kind as i32;        // OP_NONE, OP_REG, OP_IMM, OP_MEM, OP_LABEL, OP_RIP_LABEL
    reg_id as i32;      // register ID (for OP_REG)
    imm as i64;         // immediate value (for OP_IMM)
    mem_base as i32;    // base register ID (for OP_MEM)
    mem_disp as i64;    // displacement (for OP_MEM)
    mem_size as i32;    // operand size hint in bits (for OP_MEM, from size prefix)
    label_ptr as *u8;   // label name pointer (for OP_LABEL, OP_RIP_LABEL)
    label_len as i32;   // label name length
}
```

| Kind | Description | Example |
|------|-------------|---------|
| `OP_NONE` | No operand | (second operand of `ret`) |
| `OP_REG` | Register | `rax`, `xmm0`, `cl` |
| `OP_IMM` | Immediate constant | `42`, `0xFF`, `-8` |
| `OP_MEM` | Memory via base+displacement | `[rbp - 8]`, `QWORD PTR [rsp + 16]` |
| `OP_LABEL` | Label reference (for jumps/calls) | `my_function`, `.L0` |
| `OP_RIP_LABEL` | RIP-relative label reference | `[rip + .LC0]` |

## The encode() Function

The central `encode()` function accepts:
- `buf`: Output ByteBuffer for emitting bytes
- `inst`: Instruction ID constant
- `op1`, `op2`: Pointers to Operand structs
- `resolve_label`: Resolved absolute offset for label operands (from pass 1 symbol lookup)
- `cur_off`: Current offset in the .text section (for relative offset calculation)

It returns the number of bytes emitted. The function uses a large sequence of conditionals to match the instruction ID and operand kinds, then emits the appropriate encoding.

## Encoding Patterns

### Register-to-Register

For instructions like `mov reg, reg`, `add reg, reg`:

1. Emit operand size prefix (0x66) if 16-bit.
2. Emit REX prefix if needed (64-bit operand, extended registers, or 8-bit spl/bpl/sil/dil).
3. Emit opcode (with 8-bit variant if operand size is 8).
4. Emit ModR/M with `mod=11` (register-direct).

### Register-to-Immediate

For `mov reg, imm`, `add reg, imm`:

1. Emit size prefix if 16-bit.
2. Emit REX if needed.
3. For `mov`: use the short `B8+rd` encoding for register-direct immediate loads. For 64-bit with 32-bit-fit immediates, use `C7 /0` instead of the 10-byte `REX.W B8+rd imm64`.
4. For ALU ops: use sign-extended imm8 encoding (opcode 0x83) when the immediate fits in 8 bits, otherwise full imm32.

### Memory Operations

For `mov reg, [mem]` and `mov [mem], reg`:

1. Determine operand size from the register (or from the `mem_size` hint for memory-to-immediate).
2. Emit prefixes and REX.
3. Emit opcode (0x89 for store, 0x8B for load, with 8-bit variants 0x88/0x8A).
4. Call `emit_mem_modrm()` to encode the memory addressing.

### Relative Jumps and Calls

For `call label` and `jmp label`:

1. Emit the opcode (0xE8 for call, 0xE9 for jmp).
2. Compute `rel32 = target - (current_offset + instruction_size)`.
3. Emit the 32-bit relative offset.

Conditional jumps use the two-byte `0F 8x` encoding with a 32-bit relative offset (always near jumps, never short).

## ByteBuffer

The encoder uses a `ByteBuffer` struct for all output:

```cst
struct ByteBuffer {
    data as *u8;
    len as i64;
    cap as i64;
}
```

Helper functions: `buf_emit8`, `buf_emit16_le`, `buf_emit32_le`, `buf_emit64_le` for little-endian byte emission; `buf_patch32_le` for patching a 32-bit value at a given offset; `buf_append` for bulk copy; `buf_align` for padding to alignment boundaries.
