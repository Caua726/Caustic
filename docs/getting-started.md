# Getting Started with Caustic

## What is Caustic?

Caustic is a self-hosted native programming language and compiler for Linux
x86_64 and AArch64. It compiles directly to assembly, assembles ELF object files,
and links executables — all without LLVM, libc, or external production-toolchain
dependencies. Programs use raw Linux syscalls, giving you full control over what
your code does with nothing hidden underneath.

## Requirements

- **Linux x86_64 host** — used to run the distributed bootstrap tools. The
  compiler targets `linux-x86_64`, `linux-aarch64`, `windows-x86_64`, and
  `caustic-x86_64`.
- **Optional AArch64 runner** — use native AArch64 Linux or `qemu-aarch64` to
  execute `linux-aarch64` output on an x86_64 development host.
- **No dependencies** — No C compiler, no linker, no libraries. The entire toolchain
  is self-contained: `caustic` (compiler), `caustic-as` (assembler), and `caustic-ld`
  (linker).

## Installation

### Option 1: Download from GitHub Releases

Grab the latest release, extract it, and install the binaries and standard library:

```bash
# Download the latest release
gh release download --repo Caua726/caustic -p 'caustic-x86_64-linux.tar.gz'

# Extract the archive
tar xzf caustic-x86_64-linux.tar.gz

# Install binaries
sudo cp caustic caustic-as caustic-ld /usr/local/bin/

# Install the standard library
sudo mkdir -p /usr/local/lib/caustic/
sudo cp -r std/ /usr/local/lib/caustic/std/
```

After this, `caustic`, `caustic-as`, and `caustic-ld` should be available from
anywhere on your system.

### Option 2: Build from Source

Since Caustic is self-hosted, you need an existing `caustic` binary to compile it.
Install from a release first (Option 1), then build from source:

```bash
# Clone the repository
git clone https://github.com/Caua726/caustic.git
cd caustic

# Build the compiler
./caustic-mk build caustic

# Build the assembler
./caustic-mk build caustic-as

# Build the linker
./caustic-mk build caustic-ld
```

This produces fresh `caustic`, `caustic-as`, and `caustic-ld` binaries in the
project root. You can copy them to `/usr/local/bin/` to install system-wide.

To verify everything works:

```bash
./caustic-mk test
```

## Your First Program

Create a file called `hello.cst`:

```cst
use "std/io.cst" as io;
use "std/linux.cst" as linux;

fn main() as i32 {
    io.println("Hello, world!");
    return 0;
}
```

Compile and run it for the default x86_64 Linux target:

```bash
caustic hello.cst -o hello
./hello
```

You should see `Hello, world!` printed to your terminal.

To cross-compile the same source to a static AArch64 Linux executable:

```bash
caustic --target=linux-aarch64 hello.cst -o hello-aarch64
qemu-aarch64 ./hello-aarch64
```

### What is happening here?

**`use` imports modules.** Caustic uses `use "path" as name;` to bring in other
source files as modules. Here we import `io` for printing and `linux` for syscall
wrappers. The standard library lives in `std/` and provides modules for I/O, memory
management, strings, networking, and more.

**`main` returns `i32`.** The return value of `main()` becomes the process exit code.
Returning `0` means success. You can check it with `echo $?` after running your
program. Exit codes are limited to 0-255 on Linux.

**`io.println` instead of raw syscalls.** You could write directly to stdout with
`syscall(1, 1, "Hello, world!\n", 14)`, but the standard library provides convenient
wrappers. `io.println` handles the write syscall and appends a newline for you.

## How Compilation Works

When you run `caustic hello.cst -o hello`, three things happen behind the scenes:

```
hello.cst  ──caustic──>  hello.cst.s  ──caustic-as──>  hello.cst.s.o  ──caustic-ld──>  hello
 (source)                (assembly)                     (ELF object)                    (executable)
```

1. **`caustic`** — The compiler reads your `.cst` source, runs it through lexing,
   parsing, semantic analysis, IR generation, and target-specific code generation
   to produce x86_64 or AArch64 assembly (`.s` file).

2. **`caustic-as`** — The assembler converts the assembly into an ELF relocatable
   object file (`.o` file).

3. **`caustic-ld`** — The linker resolves symbols and produces the final executable.

The `-o` flag tells the compiler to run all three steps automatically. If you want to
run them manually (useful for multi-file projects), you can:

```bash
# Compile to assembly
caustic hello.cst

# Assemble to object file
caustic-as hello.cst.s

# Link to executable
caustic-ld hello.cst.s.o -o hello
```

For the AArch64 manual pipeline, pass `--target=linux-aarch64` consistently to
all three tools.

No gcc, no ld, no external tools. The entire pipeline is written in Caustic itself.

### Multi-file Projects

For projects with multiple source files, compile each file separately with `-c`
(which skips the main function requirement), then link them together:

```bash
# Compile library and main separately
caustic -c lib.cst
caustic main.cst

# Assemble both
caustic-as lib.cst.s
caustic-as main.cst.s

# Link together
caustic-ld lib.cst.s.o main.cst.s.o -o program
```

Use `extern fn` declarations in your source files to reference functions defined in
other files:

```cst
// In main.cst — declare the function from lib.cst
extern fn add(a as i32, b as i32) as i32;

fn main() as i32 {
    return add(20, 22);
}
```

## A Few More Examples

### Variables and types

```cst
fn main() as i32 {
    let is i64 as x = 42;
    let is bool as flag = true;
    let is *u8 as message = "Caustic";
    let is i32 as counter with mut = 0;  // mutable variable

    counter = counter + 1;
    return counter;
}
```

Variables are immutable by default. Add `with mut` to allow reassignment.

### Structs

```cst
struct Point {
    x as i32;
    y as i32;
}

fn main() as i32 {
    let is Point as p = Point { x: 10, y: 20 };
    return p.x + p.y;
}
```

### Calling syscalls directly

```cst
fn main() as i32 {
    // write(stdout, message, length)
    syscall(1, 1, "Low level!\n", 11);
    return 0;
}
```

Every Linux syscall is available via `syscall()`. No wrappers needed — but the
standard library's `std/linux.cst` provides named functions for convenience.

## Next Steps

Now that you have Caustic installed and running, here is where to go next:

- **[Tutorial](tutorial.md)** — Learn Caustic step by step, from variables to generics.
- **[Language Reference](language.md)** — Full reference for Caustic syntax, types,
  and features.
- **[Standard Library](stdlib/)** — Documentation for all standard library modules
  (I/O, memory, strings, networking, and more).
