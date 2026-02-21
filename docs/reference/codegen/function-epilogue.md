# Function Epilogue

The function epilogue is emitted by the codegen for every `IR_RET` instruction. It restores the callee-saved registers, deallocates the stack frame, and returns to the caller.

## Epilogue Sequence

```asm
  mov rax, <return_value>   ; (omitted for void returns)
  add rsp, <aligned_size>   ; deallocate stack space
  pop r15                   ; restore callee-saved registers
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp                   ; restore caller's frame pointer
  ret                       ; return to caller
```

## Step-by-Step Breakdown

### 1. Set Return Value

If the `IR_RET` instruction has a non-`OP_NONE` source operand, the return value is loaded into `rax`:

```asm
mov rax, <src1_location>
```

This handles both register-allocated and spilled return values. For void functions (or when `src1.op_type == OP_NONE`), this step is skipped.

### 2. Deallocate Stack Space

```asm
add rsp, <aligned_stack_size>
```

This reverses the `sub rsp, <aligned_size>` from the prologue, reclaiming the space used for local variables, spill slots, and allocation buffers. If `aligned_stack_size` is 0, this instruction is omitted.

### 3. Restore Callee-Saved Registers

```asm
pop r15
pop r14
pop r13
pop r12
pop rbx
```

The registers are popped in reverse order of how they were pushed in the prologue. This restores the caller's values for all callee-saved registers.

### 4. Restore Frame Pointer

```asm
pop rbp
```

Restores the caller's frame pointer.

### 5. Return

```asm
ret
```

Pops the return address from the stack and transfers control back to the caller. The return value is in `rax` (or `xmm0` for System V ABI float returns, though Caustic does not use this).

## Multiple Return Points

A function may contain multiple `IR_RET` instructions (e.g., early returns in different branches). Each one generates a complete copy of the epilogue sequence. There is no shared epilogue label that multiple returns jump to. This duplicates a few instructions per return point but avoids the need for an extra jump.

## Implicit Return

Every function has an implicit `return 0` appended after its body during IR generation. This means even functions that appear to have all paths covered still get an epilogue at the end. If all explicit returns are reached before this implicit one, the trailing epilogue is dead code but is emitted regardless (the IR generator does not remove it).

## Stack State at Return

When `ret` executes, the stack pointer points to the return address that was pushed by the `call` instruction in the caller. After `ret` pops this address, the stack is in exactly the state it was in before the `call`, with all callee-saved registers restored to their original values.

```
Before ret:
  rsp -> [return address]

After ret:
  rsp -> [caller's stack frame]
  rip = return address
```
