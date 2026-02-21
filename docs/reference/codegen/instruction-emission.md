# Instruction Emission

This document describes how each IR opcode is translated to x86_64 assembly instructions by the `gen_inst` function in `src/codegen/emit.cst`.

## General Pattern

Every instruction follows the same structure:
1. Load source operands from their allocated locations (register or spill slot) into scratch registers (r14, r15).
2. Perform the operation.
3. Store the result to the destination vreg's allocated location.

The `load_op(vreg, ctx, target)` and `store_op(vreg, ctx, source)` helpers handle the register/memory distinction transparently.

## Data Movement

### IR_IMM -- Load Immediate

```asm
-- Small immediate (fits in 32-bit sign-extended):
mov <dest_loc>, <value>

-- Large immediate to spill slot:
mov r15, <value>
mov QWORD PTR [rbp-<offset>], r15
```

### IR_MOV -- Register Copy

```asm
mov r15, <src1_loc>
mov <dest_loc>, r15
```

## Integer Arithmetic

### IR_ADD

```asm
mov r15, <src1_loc>
mov r14, <src2_loc>
add r15, r14
mov <dest_loc>, r15
```

### IR_SUB

```asm
mov r15, <src1_loc>
mov r14, <src2_loc>
sub r15, r14
mov <dest_loc>, r15
```

### IR_MUL

```asm
mov r15, <src1_loc>
mov r14, <src2_loc>
imul r15, r14
mov <dest_loc>, r15
```

### IR_DIV (Signed)

```asm
mov rax, <src1_loc>
mov r15, <src2_loc>
push rdx                    ; save rdx (may hold allocated vreg)
cqo                         ; sign-extend rax into rdx:rax
idiv r15                    ; rax = quotient
mov r15, rax
pop rdx
mov <dest_loc>, r15
```

### IR_DIV (Unsigned)

```asm
mov rax, <src1_loc>
mov r15, <src2_loc>
push rdx
xor rdx, rdx               ; zero-extend
div r15
mov r15, rax
pop rdx
mov <dest_loc>, r15
```

### IR_MOD

Same as IR_DIV, but the result is taken from `rdx` (remainder) instead of `rax`.

### IR_NEG

```asm
mov r15, <src1_loc>
neg r15
mov <dest_loc>, r15
```

## Comparisons

All six comparison opcodes follow the same pattern, differing only in the `setCC` instruction:

```asm
mov r15, <src1_loc>
mov r14, <src2_loc>
xor rax, rax                ; clear rax (zero-extend the byte result)
cmp r15, r14
setCC al                    ; set al to 0 or 1
mov <dest_loc>, rax
```

The `setCC` instruction varies by opcode and signedness:

| Opcode | Signed | Unsigned |
|--------|--------|----------|
| IR_EQ | sete | sete |
| IR_NE | setne | setne |
| IR_LT | setl | setb |
| IR_LE | setle | setbe |
| IR_GT | setg | seta |
| IR_GE | setge | setae |

Signedness is determined by the `cast_to` type on the instruction.

## Bitwise Operations

### IR_SHL (Shift Left)

```asm
mov rcx, <src2_loc>         ; shift amount must be in cl
mov r15, <src1_loc>
shl r15, cl
mov <dest_loc>, r15
```

### IR_SHR (Shift Right)

```asm
mov rcx, <src2_loc>
mov r15, <src1_loc>
sar r15, cl                 ; signed (arithmetic)
; or
shr r15, cl                 ; unsigned (logical)
mov <dest_loc>, r15
```

### IR_AND / IR_OR / IR_XOR

```asm
mov r14, <src1_loc>
mov r15, <src2_loc>
and r14, r15                ; or: or r14, r15 / xor r14, r15
mov <dest_loc>, r14
```

### IR_NOT

```asm
mov r14, <src1_loc>
not r14
mov <dest_loc>, r14
```

## Control Flow

### IR_LABEL

```asm
.LN:                        ; where N is the label index
```

### IR_JMP

```asm
jmp .LN
```

### IR_JZ

```asm
mov r15, <src1_loc>
test r15, r15
jz .LN
```

### IR_JNZ

```asm
mov r15, <src1_loc>
test r15, r15
jnz .LN
```

### IR_RET

```asm
mov rax, <src1_loc>         ; (omitted for void returns)
add rsp, <aligned_stack_size>
pop r15
pop r14
pop r13
pop r12
pop rbx
pop rbp
ret
```

## Memory Operations

### IR_ADDR

```asm
lea r15, [rbp-<offset>]
mov <dest_loc>, r15
```

### IR_ADDR_GLOBAL (Non-extern)

```asm
lea r15, [rip+<symbol_name>]
mov <dest_loc>, r15
```

### IR_ADDR_GLOBAL (Extern)

```asm
mov r15, QWORD PTR [rip+<symbol_name>]
mov <dest_loc>, r15
```

### IR_FN_ADDR

```asm
lea r15, [rip+<function_name>]
mov <dest_loc>, r15
```

### IR_GET_ALLOC_ADDR

```asm
lea r15, [rbp-<computed_offset>]
mov <dest_loc>, r15
```

### IR_LOAD (from vreg address)

Width depends on type size:
```asm
mov r15, <addr_loc>
mov r15, QWORD PTR [r15]       ; 8 bytes
; or movsxd r15, DWORD PTR [r15]  ; 4 bytes signed
; or movzx r15, BYTE PTR [r15]    ; 1 byte unsigned
; etc.
mov <dest_loc>, r15
```

### IR_LOAD (from stack offset)

```asm
mov r15, QWORD PTR [rbp-<offset>]
; (with width variants as above)
mov <dest_loc>, r15
```

### IR_STORE (to vreg address)

```asm
mov rax, <value_loc>
mov r15, <addr_loc>
mov QWORD PTR [r15], rax       ; 8 bytes
; or mov DWORD PTR [r15], eax   ; 4 bytes
; etc.
```

### IR_STORE (to stack offset)

```asm
mov rax, <value_loc>
mov QWORD PTR [rbp-<offset>], rax
```

### IR_COPY

```asm
mov rdi, <dest_addr_loc>
mov rsi, <src_addr_loc>
mov rcx, <byte_count>
cld
rep movsb
```

## Function Call Interface

### IR_SET_ARG (index < 6)

```asm
mov r15, <value_loc>
mov <arg_register>, r15         ; rdi/rsi/rdx/rcx/r8/r9
```

### IR_SET_ARG (index >= 6)

```asm
mov r15, <value_loc>
push r15
```

### IR_GET_ARG (index < 6)

```asm
mov r15, <arg_register>
mov <dest_loc>, r15
```

### IR_GET_ARG (index >= 6)

```asm
mov r15, QWORD PTR [rbp+<16+(idx-6)*8>]
mov <dest_loc>, r15
```

### IR_SET_SYS_ARG

```asm
mov r15, <value_loc>
mov <syscall_register>, r15     ; rax/rdi/rsi/rdx/r10/r8/r9
```

### IR_SYSCALL

```asm
syscall
mov <dest_loc>, rax
```

### IR_CALL

```asm
call <function_name>
mov <dest_loc>, rax
```

### IR_SET_CTX

```asm
mov r10, <addr_loc>             ; if src1 is a vreg
; or
xor r10, r10                   ; if src1 is immediate 0
```

## Miscellaneous

### IR_ASM

```asm
<raw assembly string>
```

### IR_CAST

See [Float Codegen](float-codegen.md) for float-related casts. For integer-to-integer casts:

```asm
mov r14, <src1_loc>
movsxd r14, r14d               ; i32 signed
; or mov r14d, r14d             ; u32 (zero-extend)
; or movsx r14, r14w            ; i16 signed
; or movzx r14, r14b            ; u8 unsigned
; etc.
mov <dest_loc>, r14
```
