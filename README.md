# Caustic

Self-hosted x86_64 Linux compiler — no LLVM, no libc, no runtime.

The entire toolchain — compiler, assembler, linker, and build system — is written in Caustic itself. Programs execute via direct Linux syscalls with zero external dependencies.

## Overview

- **Self-hosted**: the compiler compiles itself (stable bootstrap since v0.0.1)
- **Zero dependencies**: no LLVM, no libc, no runtime — only Linux syscalls
- **Full toolchain**: compiler (`caustic`), assembler (`caustic-as`), linker (`caustic-ld`), build system (`caustic-mk`)
- **Optimizing**: `-O1` with SSA, constant propagation, strength reduction, LICM, DCE, graph coloring register allocation
- **~18K lines** of self-hosted Caustic code + 6.6K lines of standard library

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

```sh
./caustic hello.cst -o hello && ./hello
```

## Features

### Language
- **Types**: i8-i64, u8-u64, f32, f64, bool, char, string, void, pointers, arrays
- **Generics**: monomorphization (`fn max gen T`, `struct Pair gen T, U`, `enum Option gen T`)
- **Enums**: simple enums and tagged unions with pattern matching (`match`/`case`/`else`)
- **Impl blocks**: methods and associated functions (`impl Point { fn sum(self) ... }`)
- **Defer**: LIFO cleanup before return (`defer free(ptr);`)
- **FFI**: `extern fn` for C interop, System V ABI struct passing
- **Indirect calls**: `call(fn_ptr, args)` with compile-time type checking
- **Modules**: `use "path" as alias`, `only` selective imports, submodule dot-notation
- **Type aliases**: `type Size = i64;`

### Standard Library

| Module | Description |
|--------|-------------|
| `linux.cst` | 40+ syscall wrappers (file, process, memory, network, time) |
| `mem.cst` | 5 allocators: freelist, bins (O(1)), arena, pool, lifo |
| `string.cst` | Dynamic strings (concat, find, substring, split, trim, replace) |
| `io.cst` | Buffered I/O, printf, file/directory operations |
| `slice.cst` | Generic dynamic array (`Slice gen T` with push/pop/get/set) |
| `types.cst` | `Option gen T` and `Result gen T, E` |
| `map.cst` | Hash maps (MapI64, MapStr) with open addressing |
| `math.cst` | abs, min, max, pow, gcd, lcm, align |
| `sort.cst` | quicksort, heapsort, mergesort with fn_ptr comparators |
| `random.cst` | xoshiro256** PRNG with rejection sampling |
| `net.cst` | TCP/UDP sockets, bind/listen/accept, poll |
| `time.cst` | Monotonic/wall clock, sleep, elapsed |
| `env.cst` | argc/argv, getenv |
| `arena.cst` | Standalone bump allocator |
| `compatcffi.cst` | C FFI struct passing helpers |

### Toolchain
- **Compiler** (`caustic`): 6-phase pipeline (lex → parse → semantic → IR → optimize → codegen)
- **Assembler** (`caustic-as`): two-pass x86_64 assembler, 65 instructions, ELF .o output
- **Linker** (`caustic-ld`): static and dynamic linking, PLT/GOT, multi-object
- **Build system** (`caustic-mk`): Causticfile-based, git dependencies, system libraries

### Optimization (-O1)
SSA mem2reg, constant propagation, store-load forwarding, LICM, strength reduction, DCE, function inlining, graph coloring register allocation (George & Appel 1996), peephole optimizer. **3.4x faster** than -O0.

## Performance

Benchmark totals (v0.0.13, same machine, CPU-pinned):

| Compiler | fib(38) | sieve(10M) | sort(1M) | matmul(64) | collatz(1M) | **Total** |
|----------|---------|------------|----------|------------|-------------|-----------|
| GCC -O2 | — | — | — | — | — | **242ms** |
| GCC -O0 | — | — | — | — | — | **709ms** |
| **Caustic -O1** | 175ms | 61ms | 261ms | 70ms | 185ms | **757ms** |
| LuaJIT | — | — | — | — | — | **1081ms** |

## Quick Start

### Install

```sh
tar xzf caustic-x86_64-linux.tar.gz
sudo cp caustic-x86_64-linux/bin/* /usr/local/bin/
sudo mkdir -p /usr/local/lib/caustic/std
sudo cp caustic-x86_64-linux/lib/caustic/std/*.cst /usr/local/lib/caustic/std/
```

### Using caustic-mk

```sh
./caustic-mk build hello    # compile examples/hello.cst
./build/hello                # run it
./caustic-mk test            # run test suite
```

### Manual pipeline

```sh
./caustic program.cst -o program    # compile + assemble + link
./program                            # run

# Or step by step:
./caustic program.cst                # -> program.cst.s
./caustic-as program.cst.s           # -> program.cst.s.o
./caustic-ld program.cst.s.o -o program
```

## Build from Source

Bootstrap the build system:

```sh
./caustic caustic-maker/main.cst
./caustic-as caustic-maker/main.cst.s
./caustic-ld caustic-maker/main.cst.s.o -o caustic-mk
```

Then build everything:

```sh
./caustic-mk build caustic       # compiler
./caustic-mk build caustic-as    # assembler
./caustic-mk build caustic-ld    # linker
./caustic-mk build caustic-mk    # build system itself
```

### Causticfile

Projects are configured via a `Causticfile`:

```
name "myproject"
version "0.1.0"
author "Name"

target "myapp" {
    src "src/main.cst"
    out "myapp"
}

script "test" {
    "bash run_tests.sh"
}
```

## Syntax

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;

struct Point { x as i32; y as i32; }

impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x; p.y = y;
        return p;
    }

    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

fn main() as i32 {
    mem.gheapinit(1048576);
    let is Point as p = Point.new(3, 4);
    io.printf("sum = %d\n", cast(i64, p.sum()));
    return 0;
}
```

## Documentation

Full documentation is available in [`docs/`](docs/README.md):

- **[Guide](docs/README.md#guide)** — Learn the language from scratch
- **[Reference](docs/README.md#reference)** — Compiler internals, IR, codegen, assembler, linker
