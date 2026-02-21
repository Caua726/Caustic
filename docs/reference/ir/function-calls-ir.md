# Function Calls in IR

This document describes how function calls, syscalls, and argument passing are represented in Caustic's IR.

## Regular Function Calls (IR_CALL)

A function call is decomposed into three phases: argument setup, the call instruction, and result capture.

### Argument Setup

Arguments are evaluated left-to-right, their vregs stored in a temporary array, then emitted as `IR_SET_ARG` instructions in the correct ABI order.

For functions with 6 or fewer arguments, all arguments use register passing:

```
IR_SET_ARG 0, vreg_arg0    -- rdi
IR_SET_ARG 1, vreg_arg1    -- rsi
IR_SET_ARG 2, vreg_arg2    -- rdx
IR_SET_ARG 3, vreg_arg3    -- rcx
IR_SET_ARG 4, vreg_arg4    -- r8
IR_SET_ARG 5, vreg_arg5    -- r9
IR_CALL dest, "function_name"
```

For functions with more than 6 arguments, arguments 6+ are pushed onto the stack in reverse order before the register arguments are set:

```
-- Stack alignment padding (if odd number of stack args)
IR_ASM "sub rsp, 8"

-- Stack args in reverse (index 7, then 6)
IR_SET_ARG 7, vreg_arg7    -- push
IR_SET_ARG 6, vreg_arg6    -- push

-- Register args (0-5)
IR_SET_ARG 0, vreg_arg0    -- rdi
...
IR_SET_ARG 5, vreg_arg5    -- r9

IR_CALL dest, "function_name"

-- Stack cleanup
IR_ASM "add rsp, 24"       -- (stack_args + padding) * 8
```

The stack must be 16-byte aligned before the `call` instruction. If the number of stack arguments is odd, an 8-byte padding is subtracted from rsp before pushing.

### The CALL Instruction

`IR_CALL` stores:
- `dest`: vreg to receive the return value (from rax)
- `call_name_ptr` / `call_name_len`: the callee's assembly name
- `is_variadic_call`: variadic flag
- `cast_to`: pointer to the return type

In codegen, `IR_CALL` simply emits `call <name>` and moves rax into the destination vreg's location.

### Caustic ABI Note

Caustic uses a simplified calling convention where all values, including floats, are passed and returned in general-purpose registers. Float values are stored as their i64 bit representation. The standard System V xmm0-xmm7 float argument convention is not used for Caustic-to-Caustic calls. Extern functions (calls to libc or other C code) would require the full System V ABI, but Caustic programs typically use raw syscalls instead.

## SRET (Struct Return)

When a function returns a struct larger than 8 bytes, the caller allocates a stack buffer and passes its address as a hidden first argument (SRET -- struct return).

### Caller Side

```
-- Allocate stack buffer for result
alloc_stack_size += struct_size (aligned to 16)
IR_GET_ALLOC_ADDR sret_addr, <offset>

-- Shift all args by 1 (sret_addr becomes arg 0)
IR_SET_ARG 0, sret_addr
IR_SET_ARG 1, vreg_arg0
IR_SET_ARG 2, vreg_arg1
...
IR_CALL dest, "function_name"

-- Return value is sret_addr (pointer to the buffer)
```

The return value of the IR_CALL is ignored; the caller uses `sret_addr` as the result since the callee writes directly to that buffer.

### Callee Side

When a function's return type is larger than 8 bytes:
- `ctx_has_sret` is set to 1
- `ctx_sret_vreg` receives the hidden first argument via `IR_GET_ARG 0`
- Regular parameters start at argument index 1 instead of 0
- On `return expr`, the return value is copied to the sret buffer via `IR_COPY`, then the sret pointer is returned in rax

```
IR_GET_ARG ctx_sret_vreg, 0    -- hidden sret pointer
IR_GET_ARG param0_vreg, 1      -- first visible parameter
...
-- At return:
IR_COPY ctx_sret_vreg, result_vreg, <struct_size>
IR_RET ctx_sret_vreg
```

## Syscalls (IR_SYSCALL)

Linux syscalls bypass the function call mechanism entirely. Arguments are placed in the syscall-specific registers:

| Index | Register | Purpose |
|-------|----------|---------|
| 0 | rax | Syscall number |
| 1 | rdi | Argument 1 |
| 2 | rsi | Argument 2 |
| 3 | rdx | Argument 3 |
| 4 | r10 | Argument 4 |
| 5 | r8 | Argument 5 |
| 6 | r9 | Argument 6 |

The IR sequence is:

```
-- Evaluate all arguments
vreg0 = gen_expr(syscall_number)
vreg1 = gen_expr(arg1)
...

-- Set syscall registers
IR_SET_SYS_ARG 0, vreg0    -- rax
IR_SET_SYS_ARG 1, vreg1    -- rdi
IR_SET_SYS_ARG 2, vreg2    -- rsi
...
IR_SYSCALL dest             -- execute syscall, result in rax -> dest
```

Note that syscall register assignment differs from the function call ABI: the syscall number goes in rax (index 0), and the fourth argument goes in r10 instead of rcx (which is clobbered by the `syscall` instruction).

## GET_ARG -- Receiving Parameters

At the start of a function body, parameters are received via `IR_GET_ARG` and stored to their local variable stack slots:

```
IR_GET_ARG vreg0, 0         -- first arg from rdi
IR_STORE [rbp-offset0], vreg0, type0

IR_GET_ARG vreg1, 1         -- second arg from rsi
IR_STORE [rbp-offset1], vreg1, type1
...
```

For arguments beyond index 5, codegen reads from the stack at `[rbp + 16 + (idx-6)*8]`, where 16 accounts for the saved rbp and return address.

## SET_CTX -- Call Context

Before calls or syscalls that write large struct results, `IR_SET_CTX` sets the r10 register to the destination address. This allows the callee to write its result directly to the caller's buffer without an intermediate copy.

```
IR_SET_CTX addr_vreg        -- mov r10, addr
<function call>
```

When the context is not needed, `IR_SET_CTX` with `src1` as `OP_IMM(0)` clears r10 via `xor r10, r10`.

## Implicit Return

Every function body ends with an implicit `return 0` to handle the case where control flow falls through without an explicit return statement:

```
IR_IMM zero, 0
IR_RET zero
```

This is emitted unconditionally after `gen_stmt` for the function body, but the `ctx_has_returned` flag prevents it from being reached if all paths through the function already contain explicit returns.
