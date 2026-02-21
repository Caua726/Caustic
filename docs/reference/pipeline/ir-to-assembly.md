# IR to Assembly

The code generation phase is the fifth phase of the Caustic compiler. It takes the virtual-register IR and produces x86_64 assembly text in Intel syntax, performing register allocation to map unlimited virtual registers to physical hardware registers.

## Files

| File | Purpose |
|------|---------|
| `src/codegen/alloc.cst` | Linear scan register allocator: builds live intervals, assigns physical registers or spill slots |
| `src/codegen/emit.cst` | x86_64 assembly emitter: translates each IR instruction to one or more assembly instructions, manages output buffering |

## Input and Output

**Input**: `*IRProgram` containing functions, globals, and string literals.

**Output**: x86_64 assembly text written to a file descriptor, structured with `.intel_syntax noprefix`, `.text`, `.data`, and `.rodata` sections.

```cst
cg.codegen(cast(*u8, prog), cast(i32, out_fd));
```

## Register Allocation

### Overview

Caustic uses a **linear scan register allocator**. The algorithm:

1. **Build live intervals**: Scan each function's IR instruction chain, recording the first and last use position of every virtual register
2. **Sort intervals**: Order by start position (insertion sort), with ties broken by end position
3. **Allocate registers**: Walk sorted intervals, assigning available physical registers. When no register is free, spill the interval with the lowest spill cost
4. **Generate assembly**: Emit instructions using the assigned physical registers or spill/reload from stack

### LiveInterval

Each virtual register gets a `LiveInterval`:

```cst
struct LiveInterval {
    vreg as i32;        // virtual register number
    start as i32;       // first use position (instruction index)
    end_pos as i32;     // last use position
    reg as i32;         // assigned physical register (-1 if spilled)
    spill_loc as i32;   // stack spill offset (-1 if in register)
    use_count as i32;   // number of times used
    spill_cost as i32;  // cost heuristic for spilling
}
```

### AllocCtx

Per-function allocation state:

```cst
struct AllocCtx {
    intervals as *LiveInterval;    // array of intervals
    count as i32;                  // number of intervals
    vreg_to_loc as *i32;          // vreg -> physical reg or spill loc
    stack_slots as i32;           // number of spill slots used
    next_spill as i32;            // next available spill offset
    is_leaf as i32;               // 1 if function makes no calls
    arg_spill_base as i32;        // base offset for argument spills
    aligned_stack_size as i32;    // 16-byte aligned total stack frame
    func as *u8;                  // pointer to IRFunction
}
```

### Available Registers

The allocator uses 3 callee-saved general-purpose registers as the primary allocation pool:

| Register | x86_64 Name | Register ID |
|----------|-------------|-------------|
| rbx | rbx | 3 |
| r12 | r12 | 10 |
| r13 | r13 | 11 |

Two additional registers are reserved as scratch (temporaries for spill/reload and complex instruction sequences):

| Register | Purpose |
|----------|---------|
| r14 | Scratch register 1 |
| r15 | Scratch register 2 |

The six System V ABI argument registers (rdi, rsi, rdx, rcx, r8, r9) are used for passing function arguments but are not part of the general allocation pool (they are caller-saved and have dedicated uses).

### Spill Cost Heuristic

The spill cost is computed as: `(use_count * 10 + definition_count) * 100 / interval_span`. Intervals with higher cost per span are less likely to be spilled. When a register must be freed, the interval with the lowest spill cost among those currently active is evicted.

### Loop Awareness

The interval builder extends live ranges for backward jumps. When a `IR_JMP`, `IR_JZ`, or `IR_JNZ` targets a label at an earlier position (indicating a loop back-edge), all intervals that are live between the target and the jump are extended to cover the full loop range. This prevents incorrect register reuse within loops.

## Assembly Emission

### Output Buffering

The emitter uses a 4MB output buffer. Assembly text is built in memory and flushed to the output file descriptor via `write` syscalls when the buffer fills or at the end.

```cst
let is *u8 as out_buf with mut;
let is i64 as out_len with mut;
let is i64 as out_cap with mut;     // 4194304 (4MB)
let is i32 as out_fd with mut;
```

Helper functions (`out_char`, `out_str`, `out_cstr`, `out_int`, `out_line`) build assembly text incrementally.

### Assembly Structure

The generated assembly file has the following structure:

```asm
.intel_syntax noprefix

.section .rodata
.str0:
  .string "hello world"
.str1:
  .string "another string"
.flt0:
  .quad 4614253070214989087    # f64 bit pattern for 3.14

.section .data
global_var:
  .quad 42
another_global:
  .zero 8

.section .text
.globl main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 32
  ...
  leave
  ret
```

### Function Prologue and Epilogue

Each function emits:

**Prologue**:
```asm
  push rbp
  mov rbp, rsp
  sub rsp, <aligned_stack_size>
  push rbx          # if rbx is used
  push r12          # if r12 is used
  push r13          # if r13 is used
```

**Epilogue** (at each return point):
```asm
  pop r13           # if r13 was pushed
  pop r12           # if r12 was pushed
  pop rbx           # if rbx was pushed
  leave
  ret
```

### Calling Convention

Caustic follows the **System V AMD64 ABI**:

**Integer arguments**: rdi, rsi, rdx, rcx, r8, r9 (in order)

**Float arguments**: xmm0 through xmm7

**Return values**: rax (integer), xmm0 (float)

**Caller-saved**: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11

**Callee-saved**: rbx, r12, r13, r14, r15, rbp, rsp

For `IR_SET_ARG`, the emitter moves the value into the appropriate argument register based on the argument index. For `IR_CALL`, it emits a `call` instruction targeting the function's assembly name.

### Syscall Emission

`IR_SYSCALL` maps arguments to the Linux syscall convention:

| Argument | Register |
|----------|----------|
| Syscall number | rax |
| Arg 1 | rdi |
| Arg 2 | rsi |
| Arg 3 | rdx |
| Arg 4 | r10 |
| Arg 5 | r8 |
| Arg 6 | r9 |

The instruction `syscall` is emitted, and the result (from rax) is moved to the destination register.

### Float Operations

Float operations use SSE instructions:

| IR Opcode | x86_64 Instruction (f64) | x86_64 Instruction (f32) |
|-----------|-------------------------|-------------------------|
| `IR_FADD` | `addsd` | `addss` |
| `IR_FSUB` | `subsd` | `subss` |
| `IR_FMUL` | `mulsd` | `mulss` |
| `IR_FDIV` | `divsd` | `divss` |
| `IR_FNEG` | XOR with sign bit mask | XOR with sign bit mask |

Float values are moved between general-purpose registers and XMM registers via `movq` (for 64-bit) or `movd` (for 32-bit).

### Spill and Reload

When a virtual register is spilled to the stack:

- **Spill**: `mov [rbp - offset], reg` (store register to stack slot)
- **Reload**: `mov reg, [rbp - offset]` (load from stack slot to scratch register)

The scratch registers r14 and r15 are used for reloading spilled values before instructions that need them.

### Instruction Mapping Examples

| IR Instruction | x86_64 Assembly |
|---------------|-----------------|
| `v1 = IR_IMM 42` | `mov rbx, 42` |
| `v2 = IR_ADD v0, v1` | `mov r14, rbx` / `add r14, r12` / `mov rbx, r14` |
| `v3 = IR_LOAD v1` | `mov rbx, [r12]` |
| `IR_STORE v0, v1` | `mov [rbx], r12` |
| `IR_JZ v0, L1` | `test rbx, rbx` / `jz .L1` |
| `v4 = IR_CALL "func"` | `call func` / `mov rbx, rax` |
| `IR_RET v0` | `mov rax, rbx` / `leave` / `ret` |

### Globals and String Literals

- **Global variables**: Emitted in `.data` section with `.quad` (8-byte), `.long` (4-byte), or `.zero` directives
- **String literals**: Emitted in `.rodata` as `.string` directives with labels `.str0`, `.str1`, etc.
- **Float constants**: Emitted in `.rodata` as `.quad` with the raw bit pattern of the f64/f32 value

### Labels

All functions are emitted with `.globl` directives (required for multi-object linking). Internal labels for control flow use the format `.L<n>` where `n` is the label number.
