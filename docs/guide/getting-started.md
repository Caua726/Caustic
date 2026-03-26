# Getting Started

## Install

Download the latest release tarball and copy the binaries:

```sh
tar xzf caustic-x86_64-linux.tar.gz
sudo cp caustic-x86_64-linux/bin/* /usr/local/bin/
sudo mkdir -p /usr/local/lib/caustic/std
sudo cp caustic-x86_64-linux/lib/caustic/std/*.cst /usr/local/lib/caustic/std/
```

## Hello World

Create a file `hello.cst`:

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

Compile and run:

```sh
./caustic hello.cst -o hello
./hello
```

Output: `Hello, World!`

The `-o` flag runs the full pipeline: compile to assembly, assemble to object file, link into executable.

## Using caustic-mk

For projects with multiple files, use the build system. Create a `Causticfile`:

```
name "myproject"
version "0.1.0"

target "hello" {
    src "hello.cst"
    out "build/hello"
}
```

Then build and run:

```sh
./caustic-mk build hello
./build/hello
```

Other commands:

```sh
./caustic-mk run hello       # build and run
./caustic-mk test            # run the "test" script
./caustic-mk clean           # remove build artifacts
./caustic-mk init            # create a new Causticfile interactively
```

## Using the Standard Library

Most programs need the heap allocator and I/O:

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;

fn main() as i32 {
    mem.gheapinit(1048576);  // 1MB heap

    io.printf("Hello %s! %d + %d = %d\n", "World", 2, 3, 5);

    return 0;
}
```

The stdlib is resolved automatically from `<binary_dir>/../lib/caustic/` or `/usr/local/lib/caustic/`.

## Manual Pipeline

Without `-o`, you can run each stage separately:

```sh
./caustic program.cst          # compile -> program.cst.s
./caustic-as program.cst.s     # assemble -> program.cst.s.o
./caustic-ld program.cst.s.o -o program  # link -> executable
./program
```

## Optimization

Use `-O1` for optimized builds (3.4x faster code, slower compilation):

```sh
./caustic -O1 program.cst -o program
```

## Next Steps

- [Variables](variables.md) — declaration, mutability, globals
- [Types](types.md) — integers, floats, pointers, arrays
- [Functions](functions.md) — parameters, return values
- [Control Flow](control-flow.md) — if, while, for, match
- [Structs](structs.md) — composite types
- [Modules](modules.md) — imports, multi-file projects
- [Compiler Flags](compiler-flags.md) — all 16 flags
