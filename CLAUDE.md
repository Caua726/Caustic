# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
./caustic-mk build caustic       # Build the compiler
./caustic-mk build caustic-as    # Build the assembler
./caustic-mk build caustic-ld    # Build the linker
./caustic-mk build caustic-mk    # Build the build system itself
./caustic-mk test                # Run test suite
./caustic-mk clean               # Remove .caustic/ and build/
```

**Compiler usage:**
```bash
./caustic <source.cst>           # Generates <source.cst>.s assembly
./caustic <source.cst> -o prog   # Full pipeline: compile + assemble + link
./caustic -c <source.cst>        # Compile only (no main required, for libraries)
./caustic -O1 <source.cst> -o p  # Optimized build
./caustic -debuglexer <file>     # Debug tokenization
./caustic -debugparser <file>    # Debug AST
./caustic -debugir <file>        # Debug IR generation
./caustic --profile <file> -o p  # Show per-phase timing

# Full manual pipeline:
./caustic program.cst && ./caustic-as program.cst.s && ./caustic-ld program.cst.s.o -o program && ./program

# Multi-object linking:
./caustic -c lib.cst && ./caustic main.cst
./caustic-as lib.cst.s && ./caustic-as main.cst.s
./caustic-ld lib.cst.s.o main.cst.s.o -o program

# Quick one-liner test (prints exit code):
./caustic test.cst -o /tmp/t && /tmp/t; echo "Exit: $?"
```

## Architecture

Caustic is a self-hosted native x86_64 Linux compiler, with no LLVM or runtime dependencies. Direct syscall-based execution.

### Compiler Pipeline (6 phases)

```
Source (.cst) → Lexer → Parser → Semantic → IR → Codegen → Assembly (.s)
                                                              ↓ (caustic-as + caustic-ld)
                                                          Executable
```

| Phase | Files | Purpose |
|-------|-------|---------|
| Lexer | `src/lexer/` | Tokenization (80 token types) |
| Parser | `src/parser/` | Recursive descent → AST (53 node kinds) |
| Semantic | `src/semantic/` | Type checking, symbol tables, module resolution, generic instantiation |
| IR | `src/ir/` | Virtual register IR (48 opcodes, unlimited vregs), optimization passes |
| Codegen | `src/codegen/` | Register allocation (linear scan -O0, graph coloring -O1) + x86_64 assembly output |

### Type System

- **Primitives**: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `char`, `string`, `void`
- **Composite**: Pointers (`*T`), Arrays (`[N]T`), Structs, Enums (tagged unions)
- **Type Aliases**: `type Name = ExistingType;`
- **Function Types**: `TY_FN` — typed function pointers via `fn_ptr()`
- **Floats**: Use SSE instructions (addsd, subsd, mulsd, divsd), f32 literal narrowing

### Key Conventions

- System V AMD64 ABI for function calls (args: rdi, rsi, rdx, rcx, r8, r9)
- 10 allocatable registers: rbx, r12, r13, r14, r15 (callee-saved) + rsi, rdi, r8, r9, r10 (caller-saved between calls)
- Module caching to avoid re-parsing imports
- Manual memory management via `std/mem.cst` heap allocator (5 submodule allocators)

## Language Syntax

```cst
// Variables
let is i64 as x = 10;
let is *u8 as ptr;
let is [64]u8 as buffer;
let is i64 as global with mut = 0;  // mutable global
let is i64 as entry with section(".drivers_init") = 99;  // custom ELF section
let is i64 as x with mut with section(".mydata") = 0;    // mut + section

// Functions
fn name(a as i64, b as *u8) as i32 {
    return 0;
}
fn init() as void with section(".init") { }  // function in custom section

// Structs
struct Point { x as i32; y as i32; }

// Type aliases
type Byte = u8;
type Size = i64;

// Modules
use "std/mem.cst" as mem;
use "std/mem.cst" only freelist, bins as mem;  // selective import
mem.galloc(1024);
mem.bins.bins_new(4096);  // submodule access

// Defer — executes call before each return (LIFO)
defer free(ptr);
defer close(fd);

// Function pointers + indirect calls
let is *u8 as cb = fn_ptr(my_func);          // local function
let is *u8 as cb = fn_ptr(mod.func);         // module function
let is *u8 as cb = fn_ptr(max gen i32);      // generic function
let is *u8 as cb = fn_ptr(Point_sum);        // impl method
let is i64 as result = call(cb, arg1, arg2); // indirect call (type-checked)

// Low-level
syscall(1, 1, "hello", 5);  // direct syscall
asm("mov rax, 1\n");        // inline asm
cast(*u8, address);         // type cast
```

## Standard Library (`std/`)

- `linux.cst` — 40+ syscall wrappers (file, process, memory, network, time)
- `mem.cst` — Memory management hub with 5 allocators:
  - `mem/freelist.cst` — O(n) alloc/free, address-ordered coalescing
  - `mem/bins.cst` — O(1) alloc/free, slab-based, bitmap double-free detection
  - `mem/arena.cst` — O(1) bump allocator, bulk free
  - `mem/pool.cst` — O(1) fixed-size slots, optional debug
  - `mem/lifo.cst` — O(1) stack-like allocator
- `string.cst` — Dynamic strings (concat, find, substring, split, trim, replace)
- `io.cst` — Buffered I/O, printf, file/directory operations
- `slice.cst` — Generic dynamic array (`Slice gen T`)
- `types.cst` — `Option gen T` and `Result gen T, E`
- `map.cst` — Hash maps (MapI64 splitmix64, MapStr FNV-1a)
- `math.cst` — abs, min, max, pow, gcd, lcm, align_up/down
- `sort.cst` — quicksort, heapsort, mergesort with fn_ptr comparators
- `random.cst` — xoshiro256** PRNG, rejection sampling range
- `net.cst` — TCP/UDP sockets, bind/listen/accept, poll
- `time.cst` — Monotonic/wall clock, sleep, elapsed
- `env.cst` — argc/argv, getenv
- `arena.cst` — Standalone bump allocator (same as mem/arena.cst)
- `compatcffi.cst` — C FFI struct passing helpers

## Adding New Operators/Features

File change sequence:
1. `src/lexer/` - Add token type, lexer switch case
2. `src/parser/` - Add NODE_KIND, parsing function
3. `src/semantic/` - Type checking in `walk()` switch
4. `src/ir/` - IR operation, emit in `gen_expr()`
5. `src/codegen/` - Assembly generation case

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

- **Stdlib resolution**: The compiler tries paths relative to the source file first, then falls back to `<binary_dir>/../lib/caustic/`, then `/usr/local/lib/caustic/`
- **No libc**: Programs use raw Linux syscalls, not C library functions
- **Syscall numbers**: x86_64 Linux (e.g., write=1, exit=60, mmap=9)
- **Float literals**: Must match variable type (`10.0` for f64, not `10`). f32 literal narrowing is automatic.
- **Char literals**: Now properly typed, no cast needed for `let is char as c = 'A'`
- **Return via exit code**: `return N` from main becomes process exit code (0-255)
- **Unused variable warnings**: Variables declared but never used produce a warning (prefix with `_` to suppress)
- **Debug info**: Generated assembly includes `.file` and `.loc` DWARF directives for source-level debugging with GDB
- **Packed structs**: No alignment padding between fields. `sizeof({i64,i32,i64})` = 20, not 24.
- **Custom sections**: `with section(".name")` places globals/functions in named ELF sections. Linker merges by name across .o files. Only works on top-level declarations.
