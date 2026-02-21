# Struct Return (SRET)

When a function returns a struct larger than 8 bytes, the System V AMD64 ABI requires that the
caller allocate space for the return value and pass a pointer to it as a hidden first argument.
This is called a "struct return" or SRET convention.

## When SRET Applies

The IR phase classifies struct return using `classify_struct_sysv()`:

- **<= 8 bytes**: Returned in `rax` (1-eightbyte, INTEGER class).
- **9-16 bytes**: Returned in `rax` + `rdx` (2-eightbyte, split classification).
- **> 16 bytes**: SRET -- caller allocates stack space, passes pointer in `rdi`.

Note: Caustic structs are **packed** (no alignment padding between fields). This affects
classification. For example, `{i32, f64}` is 12 bytes packed, not 16 bytes padded as in C.
A struct that crosses an 8-byte boundary with different types may be classified as MEMORY even
if a C equivalent would be split into two registers.

## Caller Side (SRET)

For a call returning a struct via SRET:

1. The caller allocates a stack buffer of the appropriate size (via `IR_GET_ALLOC_ADDR`).
2. The pointer to this buffer is loaded into `rdi` as the hidden argument 0.
3. All explicit arguments shift one position: the first explicit argument goes in `rsi`, the
   second in `rdx`, etc.
4. After the call, the return value is in the buffer pointed to by the address passed in `rdi`.

```asm
-- Allocate stack buffer for return value (e.g., 24-byte struct)
lea r15, [rbp-<alloc_offset>]
mov rdi, r15                   ; hidden first arg = pointer to output buffer

-- Shift explicit args:
mov rsi, <arg0_loc>            ; arg0 goes in rsi (not rdi)
mov rdx, <arg1_loc>            ; arg1 goes in rdx

call big_struct_func

-- Result is now in [rbp-<alloc_offset>]
```

## Callee Side (SRET)

Inside a function declared to return a large struct, the hidden pointer arrives in `rdi` and is
treated as a regular hidden argument at index 0:

```asm
-- IR_GET_ARG 0: receive hidden return pointer
mov r15, rdi
mov <dest_loc>, r15            ; dest_loc holds the output pointer

-- ... compute the struct fields ...

-- IR_RET: write fields through the pointer
mov rax, <field1_loc>
mov r15, <output_ptr_loc>
mov QWORD PTR [r15], rax

mov rax, <field2_loc>
mov QWORD PTR [r15+8], rax
```

The callee does not place anything in `rax` or `rdx` for SRET returns. The output is written
directly into the caller-provided buffer.

## Two-Eightbyte Struct Return (9-16 bytes)

Structs in the 9-16 byte range are returned in two registers:

- Low 8 bytes in `rax`
- High 8 bytes in `rdx`

The codegen emits a pair of stores after `IR_CALL` when it detects a two-eightbyte return:

```asm
call func_returning_two_eightbytes
-- rax = low 8 bytes, rdx = high 8 bytes
mov QWORD PTR [rbp-<offset>], rax
mov QWORD PTR [rbp-<offset+8>], rdx
```

## Stack Allocation for SRET Buffers

The IR phase tracks the total space needed for SRET buffers in `func.alloc_stack_size`. The
codegen adds this to the total stack allocation in `gen_func`:

```cst
let is i32 as alloc_size = func.alloc_stack_size;
let is i32 as total = local_size + spill_size + alloc_size;
```

SRET buffers are addressed via `IR_GET_ALLOC_ADDR`, which computes:

```
offset = arg_spill_base + stack_slots * 8 + alloc_offset
```

## Important Notes

- **Caustic-to-Caustic calls**: SRET is handled symmetrically -- the caller sets up the pointer
  and the callee writes to it.
- **Extern fn calls**: When calling C functions that return structs, the struct layout must
  match C's alignment rules, not Caustic's packed layout. This requires manual field layout.
- **Float fields in structs**: A packed struct with a float field that crosses the 8-byte
  boundary will be classified as MEMORY (SRET), even if a C equivalent would use SSE registers.
