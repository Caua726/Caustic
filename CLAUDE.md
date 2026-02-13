# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make              # Build the compiler (creates ./caustic binary)
make assembler    # Build the assembler (creates ./caustic-as binary)
make linker       # Build the linker (creates ./caustic-ld binary)
make test-linker  # Run full linker test suite (36 tests)
make run          # Build, compile main.cst, assemble with GCC, and run
make clean        # Remove build artifacts
```

**Compiler usage:**
```bash
./caustic <source.cst>           # Generates <source.cst>.s assembly
./caustic -c <source.cst>        # Compile only (no main required, for libraries)
./caustic -debuglexer <file>     # Debug tokenization
./caustic -debugparser <file>    # Debug AST
./caustic -debugir <file>        # Debug IR generation

# Full self-hosted pipeline (no gcc needed):
./caustic program.cst && ./caustic-as program.cst.s && ./caustic-ld program.cst.s.o -o program && ./program

# Legacy pipeline with gcc:
./caustic program.cst && gcc program.cst.s -o program && ./program

# Multi-object linking:
./caustic -c lib.cst && ./caustic main.cst
./caustic-as lib.cst.s && ./caustic-as main.cst.s
./caustic-ld lib.cst.s.o main.cst.s.o -o program

# Quick one-liner test (prints exit code):
./caustic test.cst && ./caustic-as test.cst.s && ./caustic-ld test.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
```

## Architecture

Caustic is a native x86_64 Linux compiler written in C23, with no LLVM or runtime dependencies. Direct syscall-based execution.

### Compiler Pipeline (6 phases)

```
Source (.cst) → Lexer → Parser → Semantic → IR → Codegen → Assembly (.s)
                                                              ↓ (gcc)
                                                          Executable
```

| Phase | Files | Purpose |
|-------|-------|---------|
| Lexer | `lexer.c/h` | Tokenization (60+ token types) |
| Parser | `parser.c/h` | Recursive descent → AST (30+ node kinds) |
| Semantic | `semantic.c/h` | Type checking, symbol tables, module resolution |
| IR | `ir.c/h` | Virtual register IR (35+ operations, unlimited vregs) |
| Codegen | `codegen.c/h` | Register allocation + x86_64 assembly output |

### Type System

- **Primitives**: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `char`, `string`, `void`
- **Composite**: Pointers (`*T`), Arrays (`[N]T`), Structs
- **Floats**: Use SSE instructions (addsd, subsd, mulsd, divsd)

### Key Conventions

- System V AMD64 ABI for function calls (args: rdi, rsi, rdx, rcx, r8, r9)
- Linear scan register allocation with 12 GPRs
- Module caching to avoid re-parsing imports
- Manual memory management via `std/mem.cst` heap allocator

## Language Syntax

```cst
// Variables
let is i64 as x = 10;
let is *u8 as ptr;
let is [64]u8 as buffer;
let is i64 as global with mut = 0;  // mutable global

// Functions
fn name(a as i64, b as *u8) as i32 {
    return 0;
}

// Structs
struct Point { x as i32; y as i32; }

// Modules
use "std/mem.cst" as mem;
mem.galloc(1024);

// Defer — executa chamada antes de cada return (LIFO)
defer free(ptr);
defer close(fd);

// Function pointers
let is *u8 as cb = fn_ptr(my_func);          // local function
let is *u8 as cb = fn_ptr(mod.func);         // module function
let is *u8 as cb = fn_ptr(max gen i32);      // generic function
let is *u8 as cb = fn_ptr(Point_sum);        // impl method

// Low-level
syscall(1, 1, "hello", 5);  // direct syscall
asm("mov rax, 1\n");        // inline asm
cast(*u8, address);         // type cast
```

## Standard Library (`std/`)

- `linux.cst` - Syscall wrappers (read, write, mmap, exit, etc.)
- `mem.cst` - Heap allocator with free-list coalescing
- `string.cst` - Dynamic strings (String struct with ptr/len/cap)
- `io.cst` - Buffered I/O, line reading, printf

## Adding New Operators/Features

File change sequence:
1. `lexer.h/c` - Add token type, lexer switch case
2. `parser.h/c` - Add NODE_KIND, parsing function
3. `semantic.c` - Type checking in `walk()` switch
4. `ir.h/c` - IR operation, emit in `gen_expr()`
5. `codegen.c` - Assembly generation case

## Defer

`defer <fncall>;` schedules a function call to execute before every `return` in the current scope. Multiple defers run in LIFO order (last registered, first executed). The return value is evaluated **before** deferred calls run.

```cst
fn work() as i32 {
    let is *u8 as ptr = mem.galloc(1024);
    defer mem.gfree(ptr);    // runs before any return
    // ... work with ptr ...
    return 0;                // gfree(ptr) called automatically
}
```

**Rules:**
- Only accepts function calls (`defer expr;` where expr is a FNCALL)
- LIFO order: last `defer` executes first
- Return expression evaluated before defers execute
- Defers inside `if`/`else`/`while`/`for`/`match` are scoped to that block
- Does NOT support `defer syscall(...)` — wrap in a function instead

## Gotchas

- **No libc**: Programs use raw Linux syscalls, not C library functions
- **Syscall numbers**: x86_64 Linux (e.g., write=1, exit=60, mmap=9)
- **Float literals**: Must match variable type (`10.0` for f64, not `10`)
- **Char literals**: Now properly typed, no cast needed for `let is char as c = 'A'`
- **Return via exit code**: `return N` from main becomes process exit code (0-255)
