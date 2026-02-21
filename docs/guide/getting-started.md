# Getting Started

## Prerequisites

- Linux x86_64 (kernel 3.2+)
- git
- make

No other dependencies are needed. Caustic does not use LLVM, GCC, libc, or any runtime. The compiler, assembler, and linker are all self-contained.

## Build from Source

```bash
git clone https://github.com/your-repo/Caustic.git
cd Caustic

make              # Build the compiler (./caustic)
make assembler    # Build the assembler (./caustic-as)
make linker       # Build the linker (./caustic-ld)
```

After building, you should have three binaries in the project root: `caustic`, `caustic-as`, and `caustic-ld`.

## The Compilation Pipeline

Caustic uses a three-stage pipeline to go from source code to a native executable:

```
source.cst  -->  caustic  -->  .s  -->  caustic-as  -->  .o  -->  caustic-ld  -->  executable
              (compiler)        (assembler)                (linker)
```

1. **caustic** compiles `.cst` source into x86_64 assembly (`.s`)
2. **caustic-as** assembles the `.s` file into an ELF object (`.o`)
3. **caustic-ld** links one or more `.o` files into a final executable

The full command for a single file:

```bash
./caustic program.cst && ./caustic-as program.cst.s && ./caustic-ld program.cst.s.o -o program
./program
```

## Quick Test

Create a file called `hello.cst`:

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, Caustic!\n", 16);
    return 0;
}
```

Compile and run:

```bash
./caustic hello.cst && ./caustic-as hello.cst.s && ./caustic-ld hello.cst.s.o -o hello
./hello
```

You should see `Hello, Caustic!` printed to stdout.

## No libc

Caustic programs do not link against libc. All system interaction happens through raw Linux syscalls. The `syscall()` builtin maps directly to the `syscall` instruction with x86_64 Linux syscall numbers (e.g., write=1, read=0, exit=60, mmap=9).

## Program Entry Point

Every Caustic program needs a `main` function that returns `i32`:

```cst
fn main() as i32 {
    return 0;
}
```

The return value becomes the process exit code (0-255). Use the `-c` flag when compiling library files that do not have a main function.

## Standard Library

Caustic ships with a small standard library in the `std/` directory:

| Module | Purpose |
|--------|---------|
| `std/linux.cst` | Syscall wrappers (read, write, open, close, mmap, exit, etc.) |
| `std/mem.cst` | Heap allocator with free-list coalescing (`galloc`, `gfree`) |
| `std/string.cst` | Dynamic strings (`String` struct with ptr/len/cap) |
| `std/io.cst` | Buffered I/O, line reading, `printf` |

Import modules with `use`:

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;

fn main() as i32 {
    mem.gheapinit(1048576);
    io.printf("Hello from io!\n");
    return 0;
}
```

The heap must be initialized with `mem.gheapinit(size)` before any allocation functions are called.
