# Codegen Overview

The codegen phase is the final stage of the Caustic compiler pipeline. It takes an `IRProgram`
(containing functions, globals, and string constants) and produces x86_64 assembly text in Intel
syntax, written to an output file descriptor.

## Position in the Pipeline

```
Source (.cst)
  -> Lexer
  -> Parser
  -> Semantic
  -> IR (virtual register IR)
  -> Codegen         <- this phase
  -> Assembly (.s)
```

## Input and Output

- **Input**: A pointer to an `IRProgram` struct and a file descriptor for output.
- **Output**: x86_64 assembly text (Intel syntax with `noprefix`) suitable for assembly by
  `caustic-as`.

The assembly file begins with `.intel_syntax noprefix` and contains four sections:

1. `.rodata` -- string literal constants (`.LC0:`, `.LC1:`, etc.)
2. `.data` -- initialized global variables
3. `.bss` -- uninitialized global variables
4. `.text` -- executable code for all reachable functions

## Source Layout

Codegen lives under `src/codegen/`:

| File | Responsibility |
|------|----------------|
| `alloc.cst` | Live interval computation, linear scan register allocation, spill logic |
| `emit.cst` | IR opcode -> x86_64 instruction emission, prologue/epilogue, data sections |

## Code Generation Flow

The entry point is `codegen(prog, fd)`:

1. **Initialize output buffer**: A 4 MB heap-allocated buffer for batched writes.
2. **Emit `.intel_syntax noprefix`**: Assembler mode directive at the top of the file.
3. **Emit `.rodata`**: String literals registered during IR generation.
4. **Emit `.data` and `.bss`**: Global variables with appropriate size directives.
5. **Emit `.text`**: Section header followed by `.globl` for every reachable function.
6. **Emit function code**: For each reachable function, call `gen_func` which runs register
   allocation and instruction emission.

## Per-Function Code Generation

`gen_func(func)` handles one function at a time:

1. **Create `AllocCtx`**: Allocate and zero-initialize the allocation context.
2. **Build live intervals**: Scan all instructions to determine each vreg's first definition
   and last use.
3. **Linear scan allocation**: Assign physical registers to vregs, or spill to stack slots.
4. **Compute stack layout**: Sum local variables, spill slots, and alloc space. Align to 16 bytes.
5. **Emit function label**: The assembly-visible function name.
6. **Emit prologue**: `push rbp`, `mov rbp, rsp`, push callee-saved registers, `sub rsp`.
7. **Emit instructions**: Walk the instruction linked list and call `gen_inst` for each one.

## Assembly Syntax

The output uses Intel syntax without register prefixes (no `%` or `$`):

```asm
.intel_syntax noprefix
.text
.globl main
main:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 8
  mov rax, 42
  add rsp, 8
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
```

## Scratch Registers

The codegen reserves `r14` and `r15` as dedicated scratch registers. Every IR instruction loads
its source operands into `r15`/`r14`, performs the operation, and stores the result back to the
destination's physical location. This avoids tracking transient values across instruction
boundaries and keeps the emission code simple.

Example for `IR_ADD`:

```asm
  mov r15, <src1_location>
  mov r14, <src2_location>
  add r15, r14
  mov <dest_location>, r15
```

## Output Buffering

Assembly text is buffered in a 4 MB heap buffer to minimize `write` syscall overhead. The
`out_char`, `out_str`, `out_cstr`, `out_int`, and `out_line` family of functions append to this
buffer and flush via `linux.write` when the buffer is nearly full or when codegen completes.

## Reachability

Only functions and globals marked `is_reachable == 1` are included in the output. Reachability
is computed in the IR phase via `mark_reachable`, which transitively marks everything called or
referenced from `main` (or from all functions when compiling with `-c`).

## Assembly File Naming

The compiler appends `.s` to the source filename:

```
program.cst  ->  program.cst.s
```

The assembler then produces `program.cst.s.o`, and the linker produces the final executable.
