# IR Opcodes

Caustic's IR defines 46 opcodes numbered 0 through 45. Each opcode is stored as an `i32` constant. This document describes every opcode, its operand usage, and its semantics.

## Notation

- `dest`, `src1`, `src2` refer to the three `Operand` fields of an `IRInst`.
- `V(n)` means an `OP_VREG` operand referencing virtual register `n`.
- `I(n)` means an `OP_IMM` operand with immediate value `n`.
- `L(n)` means an `OP_LABEL` operand referencing label index `n`.

---

## Data Movement

### IR_IMM (0) -- Load Immediate
Loads a 64-bit immediate value into a destination vreg.

- `dest`: V(d) -- destination vreg
- `src1`: I(val) -- the immediate value
- `src2`: unused

For float literals, the f64 bit pattern is stored as an i64 via `f64_to_i64`.

### IR_MOV (1) -- Copy Virtual Register
Copies the value of one vreg to another.

- `dest`: V(d) -- destination vreg
- `src1`: V(s) -- source vreg
- `src2`: unused

---

## Integer Arithmetic

### IR_ADD (2) -- Addition
`dest = src1 + src2`

- `dest`: V(d)
- `src1`: V(a) -- left operand
- `src2`: V(b) -- right operand

Also used for pointer arithmetic after scaling the index by element size.

### IR_SUB (3) -- Subtraction
`dest = src1 - src2`

Operand layout identical to IR_ADD.

### IR_MUL (4) -- Multiplication
`dest = src1 * src2`

Emitted as `imul` in codegen (signed multiply).

### IR_DIV (5) -- Division
`dest = src1 / src2`

The `cast_to` field on the instruction carries the operand type to determine signed (`idiv`) vs unsigned (`div`) division in codegen.

### IR_MOD (6) -- Modulo
`dest = src1 % src2`

Same signed/unsigned behavior as IR_DIV. Codegen reads the remainder from `rdx` after division.

### IR_NEG (7) -- Integer Negate
`dest = -src1`

- `dest`: V(d)
- `src1`: V(s)
- `src2`: unused

---

## Comparison

All comparison opcodes produce a boolean result (0 or 1) in the destination vreg. The `cast_to` field carries the operand type for signed vs unsigned comparison selection in codegen.

### IR_EQ (8) -- Equal
`dest = (src1 == src2) ? 1 : 0`

### IR_NE (9) -- Not Equal
`dest = (src1 != src2) ? 1 : 0`

### IR_LT (10) -- Less Than
`dest = (src1 < src2) ? 1 : 0`

Signed: uses `setl`. Unsigned: uses `setb`.

### IR_LE (11) -- Less or Equal
`dest = (src1 <= src2) ? 1 : 0`

Signed: uses `setle`. Unsigned: uses `setbe`.

### IR_GT (12) -- Greater Than
`dest = (src1 > src2) ? 1 : 0`

Signed: uses `setg`. Unsigned: uses `seta`.

### IR_GE (13) -- Greater or Equal
`dest = (src1 >= src2) ? 1 : 0`

Signed: uses `setge`. Unsigned: uses `setae`.

---

## Control Flow

### IR_JMP (14) -- Unconditional Jump
Transfers control to the specified label.

- `dest`: L(n) -- target label index
- `src1`, `src2`: unused

### IR_JZ (15) -- Jump if Zero
Jumps to the target label if the source vreg is zero.

- `dest`: L(n) -- target label index
- `src1`: V(s) -- condition vreg
- `src2`: unused

Codegen emits `test r, r` followed by `jz`.

### IR_JNZ (16) -- Jump if Not Zero
Jumps to the target label if the source vreg is nonzero.

- `dest`: L(n) -- target label index
- `src1`: V(s) -- condition vreg
- `src2`: unused

Codegen emits `test r, r` followed by `jnz`.

### IR_LABEL (17) -- Label Definition
Defines a branch target in the instruction stream.

- `dest`: L(n) -- the label index
- `src1`, `src2`: unused

Codegen emits `.LN:` where N is the label index.

---

## Function Call Interface

### IR_CALL (18) -- Function Call
Calls a named function and stores the return value.

- `dest`: V(d) -- receives the return value (from rax)
- `call_name_ptr` / `call_name_len`: the callee's assembly name
- `is_variadic_call`: whether the callee is variadic
- `cast_to`: pointer to the return type (used for SRET detection)

### IR_RET (19) -- Return
Returns from the current function.

- `src1`: V(s) -- the return value vreg, or `OP_NONE` for void returns
- `dest`, `src2`: unused

Codegen restores callee-saved registers and emits `ret`.

### IR_GET_ARG (31) -- Get Function Argument
Reads a function argument by index into a destination vreg.

- `dest`: V(d) -- destination vreg
- `src1`: I(idx) -- argument index (0-based)
- `src2`: unused

For indices 0-5, reads from the System V ABI registers (rdi, rsi, rdx, rcx, r8, r9). For index 6+, reads from the stack at `[rbp + 16 + (idx-6)*8]`.

### IR_SET_ARG (32) -- Set Call Argument
Prepares an argument for a subsequent IR_CALL.

- `dest`: I(idx) -- argument index
- `src1`: V(s) -- the value vreg
- `cast_to`: argument type pointer (currently unused in codegen)

For indices 0-5, moves the value into the corresponding ABI register. For index 6+, pushes the value onto the stack.

### IR_SET_SYS_ARG (33) -- Set Syscall Argument
Prepares an argument for a subsequent IR_SYSCALL.

- `dest`: I(idx) -- syscall argument index (0=rax, 1=rdi, 2=rsi, 3=rdx, 4=r10, 5=r8, 6=r9)
- `src1`: V(s) -- the value vreg

### IR_SYSCALL (34) -- System Call
Executes a Linux syscall instruction.

- `dest`: V(d) -- receives the return value from rax
- `src1`, `src2`: unused

Arguments must be set up beforehand via IR_SET_SYS_ARG.

---

## Memory Operations

### IR_LOAD (20) -- Load from Memory
Reads a value from a memory address into a vreg.

- `dest`: V(d) -- destination vreg
- `src1`: V(addr) or I(stack_offset) -- the address (vreg for heap/pointer, immediate for stack-local)
- `cast_to`: pointer to the value's type (determines load width: 1, 2, 4, or 8 bytes and sign extension)

### IR_STORE (21) -- Store to Memory
Writes a value from a vreg to a memory address.

- `dest`: V(addr) or I(stack_offset) -- the target address
- `src1`: V(s) -- the value to store
- `cast_to`: pointer to the value's type (determines store width)

For structs larger than 8 bytes stored to a stack offset, codegen performs a multi-word copy.

### IR_COPY (35) -- Memory Copy (Struct Copy)
Copies a block of memory from one address to another.

- `dest`: V(d) -- destination address
- `src1`: V(s) -- source address
- `src2`: I(size) -- number of bytes to copy

Codegen emits `rep movsb` with rdi=dest, rsi=src, rcx=size.

### IR_ADDR (36) -- Address of Local Variable
Computes the address of a local variable on the stack.

- `dest`: V(d) -- receives the address
- `src1`: I(offset) -- the variable's stack offset from rbp

Codegen emits `lea r, [rbp - offset]`.

### IR_ADDR_GLOBAL (37) -- Address of Global Variable
Computes the address of a global variable or string literal.

- `dest`: V(d) -- receives the address
- `global_name_ptr` / `global_name_len`: the symbol name
- `is_extern_global`: if 1, uses `mov` (GOT-relative) instead of `lea` (RIP-relative)

### IR_GET_ALLOC_ADDR (38) -- Get Allocation Address
Returns the address of a dynamically-sized stack allocation area (used for SRET buffers and enum payloads).

- `dest`: V(d) -- receives the address
- `src1`: I(offset) -- offset within the allocation area

The actual stack position is computed as `[rbp - (arg_spill_base + stack_slots*8 + offset)]`.

### IR_SET_CTX (39) -- Set Call Context
Sets the r10 register as a context pointer before a function call that writes its return value directly to a destination address (used for large struct returns to avoid unnecessary copies).

- `src1`: V(addr) or I(0) -- the destination address, or 0 to clear

---

## Miscellaneous

### IR_PHI (22) -- Phi Node (Unused)
Reserved opcode. Not emitted by the IR generator.

### IR_ASM (23) -- Inline Assembly
Emits a raw assembly string into the output.

- `asm_str_ptr` / `asm_str_len`: the assembly text
- No operand fields are used.

Also used internally for stack cleanup after function calls with more than 6 arguments.

### IR_CAST (24) -- Type Cast
Converts a value from one type to another.

- `dest`: V(d) -- receives the converted value
- `src1`: V(s) -- source value
- `cast_to`: pointer to target type
- `cast_from`: pointer to source type (null if unknown)

Codegen handles four cases:
1. **float -> int**: `cvttsd2si`
2. **int -> float**: `cvtsi2sd`
3. **int -> smaller int**: truncation via `movzx`/`movsx`
4. **same size or widening**: simple move

### IR_FN_ADDR (40) -- Function Pointer Address
Loads the address of a named function into a vreg.

- `dest`: V(d) -- receives the function address
- `global_name_ptr` / `global_name_len`: the function's assembly name

Codegen emits `lea r, [rip + name]`.

---

## Bitwise Operations

### IR_SHL (25) -- Shift Left
`dest = src1 << src2`

Codegen loads the shift amount into `rcx` and uses `shl`.

### IR_SHR (26) -- Shift Right
`dest = src1 >> src2`

Uses `sar` (arithmetic shift) for signed types, `shr` (logical shift) for unsigned. The `cast_to` field determines signedness.

### IR_AND (27) -- Bitwise AND
`dest = src1 & src2`

### IR_OR (28) -- Bitwise OR
`dest = src1 | src2`

### IR_XOR (29) -- Bitwise XOR
`dest = src1 ^ src2`

### IR_NOT (30) -- Bitwise NOT
`dest = ~src1`

- `dest`: V(d)
- `src1`: V(s)
- `src2`: unused

---

## Floating-Point Operations

These opcodes mirror the integer arithmetic opcodes but use SSE instructions in codegen.

### IR_FADD (41) -- Float Addition
`dest = src1 + src2` (f64)

Codegen: `movq xmm0, r; movq xmm1, r; addsd xmm0, xmm1; movq r, xmm0`

### IR_FSUB (42) -- Float Subtraction
`dest = src1 - src2` (f64)

Codegen uses `subsd`.

### IR_FMUL (43) -- Float Multiplication
`dest = src1 * src2` (f64)

Codegen uses `mulsd`.

### IR_FDIV (44) -- Float Division
`dest = src1 / src2` (f64)

Codegen uses `divsd`.

### IR_FNEG (45) -- Float Negate
`dest = -src1` (f64)

Codegen XORs the sign bit: loads `0x8000000000000000` into xmm1, then `xorpd xmm0, xmm1`.
