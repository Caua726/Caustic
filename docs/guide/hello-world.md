# Hello World

## The Simplest Program

The minimal Caustic program returns an exit code:

```cst
fn main() as i32 {
    return 42;
}
```

Compile and run:

```bash
./caustic hello.cst && ./caustic-as hello.cst.s && ./caustic-ld hello.cst.s.o -o hello
./hello; echo $?
```

Output: `42`

The return value of `main` becomes the process exit code. Valid range is 0-255.

## Hello World with syscall

Since Caustic has no libc, you write to stdout using the `write` syscall directly:

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

The arguments to `syscall` are:
- `1` -- syscall number (write)
- `1` -- file descriptor (stdout)
- `"Hello, World!\n"` -- pointer to the string
- `14` -- number of bytes to write

## Hello World with io.printf

For formatted output, use the standard library:

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;

fn main() as i32 {
    mem.gheapinit(1048576);
    io.printf("Hello, %s!\n", "World");
    return 0;
}
```

`mem.gheapinit(1048576)` initializes a 1 MB heap. This is required before using any standard library function that allocates memory, including `io.printf`.

## Compile and Run (Full Pipeline)

Every Caustic program goes through three steps:

```bash
# 1. Compile .cst to assembly
./caustic hello.cst

# 2. Assemble to object file
./caustic-as hello.cst.s

# 3. Link to executable
./caustic-ld hello.cst.s.o -o hello

# Run
./hello
```

Or as a one-liner:

```bash
./caustic hello.cst && ./caustic-as hello.cst.s && ./caustic-ld hello.cst.s.o -o hello && ./hello
```

## Exit Codes

The return value of `main` is the process exit code. Linux truncates it to 0-255:

```cst
fn main() as i32 {
    return 0;    // success
}
```

```cst
fn main() as i32 {
    return 1;    // error
}
```

Check the exit code with `echo $?` after running the program.

## See Also

- [examples/hello.cst](../../examples/hello.cst) — runnable hello world example
