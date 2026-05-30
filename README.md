# Caustic

**A self-hosted, native x86_64 compiler — no LLVM, no libc, no runtime.**

![version](https://img.shields.io/badge/version-0.0.14-blue)
![self-hosted](https://img.shields.io/badge/self--hosted-yes-success)
![targets](https://img.shields.io/badge/targets-Linux%20%C2%B7%20Windows%20%C2%B7%20CausticOS-orange)
![dependencies](https://img.shields.io/badge/dependencies-none-brightgreen)
![arch](https://img.shields.io/badge/arch-x86__64-lightgrey)

Caustic is a systems programming language whose **entire toolchain — compiler,
assembler, linker, and build system — is written in Caustic itself.** It
compiles straight to machine code and links its own executables, with no
external dependencies: no LLVM backend, no libc, no language runtime. Programs
talk to the OS directly (Linux syscalls, Windows DLL imports, or the CausticOS
kernel ABI).

```cst
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("Hello, World!\n");   // portable: same source builds for every target
    return 0;
}
```

```sh
./caustic hello.cst -o hello && ./hello                       # Linux  (ELF)
./caustic --target=windows-x86_64 hello.cst -o hello.exe      # Windows (PE32+)
./caustic --target=caustic-x86_64  hello.cst -o hello.cse     # Universal CSE: one binary for Linux + Windows + CausticOS
```

---

## Contents

- [Why Caustic](#why-caustic)
- [Compilation targets](#compilation-targets)
- [Quick start](#quick-start)
- [Language at a glance](#language-at-a-glance)
- [Standard library](#standard-library)
- [Toolchain](#toolchain)
- [Optimizer](#optimizer)
- [Performance](#performance)
- [Project layout](#project-layout)
- [Build from source](#build-from-source)
- [Documentation](#documentation)
- [Contributing](#contributing)

---

## Why Caustic

- **Self-hosted.** The compiler compiles itself; the assembler assembles itself;
  the linker links itself. Stable bootstrap since v0.0.1.
- **Zero dependencies.** No LLVM, no libc, no runtime — just the operating
  system's raw interface. Statically linked binaries with nothing underneath.
- **Cross-target from one source.** The same `.cst` file builds for Linux,
  Windows, and CausticOS. OS-specific work goes through portable standard-library
  facades that pick the right backend *at compile time* — the wrong target's code
  is dead-stripped before it ever reaches codegen.
- **Optimizing.** `-O1` runs SSA construction, constant propagation, store/load
  forwarding, LICM, strength reduction, function inlining, DCE, a peephole pass,
  and graph-coloring register allocation.
- **Honest and explicit.** No hidden allocations, no implicit conversions, no
  runtime surprises. Manual memory management with five allocator strategies.
- **Real size.** ~39K lines of self-hosted toolchain (compiler + assembler +
  linker + build system) backed by a ~9K-line standard library.

## Compilation targets

A single source tree builds three different executable formats. Select one with
`--target=` (Linux is the default):

| Target | Flag | Output | Runs on | Status |
|--------|------|--------|---------|--------|
| **Linux** | `--target=linux-x86_64` *(default)* | static **ELF64**, raw `syscall` | Linux | Stable |
| **Windows** | `--target=windows-x86_64` | **PE32+** `.exe` (+ `.pdb`), DLL imports via IAT | Windows | Stable — **cross-compiles from Linux** |
| **Universal** | `--target=caustic-x86_64` | **CSE** (Caustic Standard Executable) | **Linux · Windows · CausticOS** | Experimental |

**Cross-compilation works today.** A Windows `.exe` built on Linux runs under
Wine (and on Windows) with no changes to the source — the FFI layer reorders
arguments into the MS x64 ABI, reserves shadow space, and emits
`call qword ptr [rip+__imp_<fn>]` through the import address table. The compiler,
assembler, and linker all ship as native Windows binaries too (`caustic.exe`,
`caustic-as.exe`, `caustic-ld.exe`).

**CSE is a *portable* executable, not a CausticOS-only one.** The Caustic
Standard Executable is the project's universal binary format: in `--mode=compat`
(or `--mode=bundle`) the driver emits a **PE + ELF + CSE polyglot** — *one* file
that is at the same time a valid Windows PE, a valid Linux ELF, **and** a valid
CausticOS executable. The exact same binary runs natively on all three operating
systems; OS detection happens at startup and the program dispatches to the right
backend. `--mode=pure` instead targets CausticOS alone — a from-scratch OS whose
kernel exposes a minimal 7-call ABI (serial write, time, sleep, exit, getpid,
yield). CSE is the newest target and parts of its linker path are still landing,
so treat it as a preview.

> Behind the scenes, target dispatch is driven by `os.current`. On single-OS
> builds it folds to a compile-time literal (`1` Linux, `2` Windows, `3`
> CausticOS) and branches guarded by it are dead-stripped — so a Linux build
> never contains a `kernel32` import and a Windows build never emits a `syscall`
> instruction. In CSE compat mode the same `os.current` becomes a runtime check,
> which is what lets a single polyglot binary serve every OS. See
> [`docs/`](docs/README.md) for the full mechanism.

## Quick start

### Install a release

```sh
tar xzf caustic-x86_64-linux.tar.gz
sudo cp caustic-x86_64-linux/bin/* /usr/local/bin/
sudo mkdir -p /usr/local/lib/caustic/std
sudo cp -r caustic-x86_64-linux/lib/caustic/std/* /usr/local/lib/caustic/std/
```

An [`install.sh`](install.sh) helper is included as well.

### Compile and run

```sh
./caustic program.cst -o program    # compile + assemble + link in one step
./program

# Cross-target:
./caustic --target=windows-x86_64 program.cst -o program.exe

# Step-by-step pipeline:
./caustic    program.cst             # -> program.cst.s   (assembly)
./caustic-as program.cst.s           # -> program.cst.s.o (object)
./caustic-ld program.cst.s.o -o program
```

### Useful flags

| Flag | Purpose |
|------|---------|
| `-o <file>` | Output executable |
| `-c` | Compile only — no `main` required (for libraries) |
| `-O0` / `-O1` | Optimization level |
| `--target=<triple>` | `linux-x86_64`, `windows-x86_64`, `caustic-x86_64` |
| `-l<name>` | Link a dynamic library |
| `--profile` | Per-phase timing |
| `--cache <dir>` | Incremental build cache |
| `--path <dir>` | Extra module search path |
| `-debuglexer` / `-debugparser` / `-debugir` | Inspect a phase |

### With the build system

```sh
./caustic-mk build caustic    # build a target from the Causticfile
./caustic-mk test             # run the test suite
./caustic-mk clean            # remove .caustic/ and build/
```

## Language at a glance

- **Types** — `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `bool`, `char`, `string`,
  `void`, pointers (`*T`), arrays (`[N]T`), structs, and tagged-union enums.
- **Generics** — monomorphized: `fn max gen T`, `struct Pair gen T, U`,
  `enum Option gen T`.
- **Pattern matching** — `match` / `case` / `else` over enums, with destructuring.
- **Impl blocks** — methods and associated functions: `impl Point { fn sum(self) ... }`.
- **Defer** — LIFO cleanup that runs before every `return`: `defer free(ptr);`.
- **Modules** — `use "path" as alias`, `only` selective imports, submodule
  dot-notation, module caching.
- **Function pointers** — typed `fn_ptr(...)` plus type-checked indirect
  `call(ptr, args)`.
- **FFI** — `extern "kernel32.dll" fn ...` declarations and System V struct
  passing for C interop.
- **Low-level escape hatches** — `syscall(...)`, inline `asm("...")`,
  `cast(T, x)`, `sizeof`, custom ELF/PE sections via `with section(".name")`.
- **`with imut`** — "immutable *and* compile-time-known": the initializer folds to
  a literal at semantic time and propagates at every use site. This is what makes
  zero-cost compile-time target dispatch fall out for free.

```cst
use "std/mem.cst" as mem;
use "std/io.cst"  as io;

struct Point { x as i32; y as i32; }

impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x; p.y = y;
        return p;
    }
    fn sum(self as *Point) as i32 { return self.x + self.y; }
}

fn main() as i32 {
    mem.gheapinit(1048576);
    let is Point as p = Point.new(3, 4);
    io.printf("sum = %d\n", cast(i64, p.sum()));
    return 0;
}
```

## Standard library

The stdlib is built as **portable facades** (the top-level modules you import)
backed by **per-OS implementations** in subdirectories (`std/io/linux.cst`,
`std/io/windows.cst`, …). User code imports the facade; the facade dispatches to
the right backend at compile time. Pure modules have no OS dependency at all.

| Module | What it provides | Backend |
|--------|------------------|---------|
| `os.cst` | OS-identity hub (`os.current`, `OS_LINUX/WINDOWS/CAUSTIC`); re-exports raw `os/{linux,windows,causticos}` bindings | dispatched |
| `io.cst` | Buffered I/O, `printf`/`sprintf`, files, directories | per-OS |
| `mem.cst` | Memory hub: 5 allocators — freelist, bins (O(1) slab), arena, pool, lifo | per-OS pages |
| `net.cst` | IPv4 TCP/UDP sockets, bind/listen/accept, poll (auto `WSAStartup` on Windows) | per-OS |
| `time.cst` | Monotonic & wall clock, sleep, elapsed | per-OS |
| `env.cst` | `argc`/`argv`, environment variables | per-OS |
| `process.cst` | Spawn / exec / wait; `capture()` (Linux) | per-OS |
| `term.cst` | Raw mode, ANSI escapes, terminal size, key input | per-OS |
| `random.cst` | xoshiro256** PRNG, OS-seeded entropy | per-OS seed |
| `string.cst` | Dynamic strings: concat, find, substring, split, trim, replace | pure |
| `slice.cst` | Generic dynamic array (`Slice gen T`) | pure |
| `map.cst` | Hash maps (`MapI64`, `MapStr`) with open addressing | pure |
| `types.cst` | `Option gen T`, `Result gen T, E` | pure |
| `math.cst` | abs, min/max, clamp, gcd/lcm, sqrt, isqrt, bit ops, alignment | pure |
| `sort.cst` | quicksort, heapsort, mergesort with `fn_ptr` comparators | pure |
| `path.cst` | Path join, dirname, basename, ext, `~` expansion | pure |
| `errors.cst` | `__compile_error` aborts + runtime panic/assert | pure |
| `arena.cst` | Standalone bump allocator | pure |
| `compatcffi.cst` | C-ABI struct/union layout & errno helpers | pure |

*New since the last release:* `os`, `errors`, `path`, `process`, `term`, and the
portable-facade architecture across `io`, `net`, `time`, `env`, `random`, and
`mem`.

## Toolchain

| Tool | Role |
|------|------|
| **`caustic`** | The compiler — 6-phase pipeline: lex → parse → semantic → IR → optimize → codegen (~23K lines) |
| **`caustic-as`** | Two-pass x86_64 assembler emitting **ELF** *and* **COFF/PE** objects (~6.6K lines) |
| **`caustic-ld`** | Linker — static & dynamic ELF (PLT/GOT), PE import tables + IAT + CodeView `.pdb`, multi-object, custom sections (~8K lines) |
| **`caustic-mk`** | `Causticfile`-driven build system: targets, scripts, git/system dependencies (~1.2K lines) |

```
Source (.cst)
   │  caustic
   ▼
Lexer → Parser → Semantic → IR → Optimize → Codegen → Assembly (.s)
                                                          │  caustic-as
                                                          ▼
                                                   Object (.o / .obj)
                                                          │  caustic-ld
                                                          ▼
                                              Executable (ELF / PE / CSE)
```

A `Causticfile` configures a project:

```
name "myproject"
version "0.1.0"

target "myapp" {
    src "src/main.cst"
    out "myapp"
}

script "test" {
    "bash run_tests.sh"
}
```

## Optimizer

`-O1` runs a real optimization pipeline over the virtual-register IR:

SSA `mem2reg` · constant propagation & folding · store/load forwarding · common
subexpression elimination · loop-invariant code motion · strength reduction
(including magic-number division and induction-variable rewriting) · function
inlining · dead-code elimination · a peephole pass · and **graph-coloring
register allocation** (George & Appel, 1996) over 10 allocatable registers.
Roughly **3.4× faster** than `-O0` output.

## Performance

Five integer workloads (`fib(38)`, `sieve(10M)`, `quicksort(1M)`, `matmul(64×100)`,
`collatz(1M)`), same source logic in every language, all results checksum-identical.
Compute time in ms (best of 5), Ryzen 5 5600X, core-pinned. **Caustic `-O1`
beats `gcc -O0`, TCC, LuaJIT, the JS runtimes and every interpreter** — while
emitting a 28 KB static binary on ~17 MB of RAM, with no runtime and no libc.

| Toolchain | fib | sieve | sort | matmul | collatz | **Total** | Compile | Binary | RAM |
|-----------|----:|------:|-----:|-------:|--------:|----------:|--------:|-------:|----:|
| GCC -O2 | 43 | 12 | 51 | 7 | 106 | **218** | 120 ms | 16 KB | 17 MB |
| **Caustic -O1** | 172 | 47 | 121 | 36 | 173 | **550** | 138 ms | 28 KB | 17 MB |
| GCC -O0 | 171 | 50 | 120 | 65 | 274 | **680** | 46 ms | 16 KB | 17 MB |
| TCC | 187 | 54 | 126 | 49 | 377 | **792** | 6 ms | 7 KB | 17 MB |
| **Caustic -O0** | 197 | 133 | 207 | 91 | 390 | **1017** | 89 ms | 24 KB | 16 MB |
| LuaJIT | 303 | 188 | 236 | 25 | 417 | **1169** | — | — | 139 MB |
| Python 3.14 | 3888 | 564 | 3405 | 1962 | 5784 | **15604** | — | — | 52 MB |

Full matrix — 28 build configs across **GCC, Clang, TCC, Rust, Zig, Go, C++,
Java, C#, Node, Bun, Deno, LuaJIT, Lua, PHP, Python** with compile time, binary
size, peak RAM and `hyperfine` wall-clock — in **[`benchmark.md`](benchmark.md)**.
Reproduce it all with one self-contained Caustic program (it embeds every
language's source and skips toolchains you don't have):

```sh
./caustic examples/benchmark.cst -o /tmp/benchmark && /tmp/benchmark
```

## Project layout

```
src/                compiler — lexer, parser, semantic, ir, codegen   (~23K)
caustic-assembler/  caustic-as — x86_64 → ELF / COFF-PE objects        (~6.6K)   [submodule]
caustic-linker/     caustic-ld — ELF / PE linking, .pdb, multi-object  (~8K)     [submodule]
caustic-maker/      caustic-mk — Causticfile build system              (~1.2K)   [submodule]
std/                standard library — facades + per-OS backends       (~9K)
examples/           runnable sample programs                            (~2.4K)
docs/               language guide + compiler-internals reference
tests/              section / driver tests
```

## Build from source

Bootstrap the build system with an existing `caustic`, then build the rest:

```sh
# 1. Build the build system
./caustic    caustic-maker/main.cst
./caustic-as caustic-maker/main.cst.s
./caustic-ld caustic-maker/main.cst.s.o -o caustic-mk

# 2. Build everything else
./caustic-mk build caustic       # the compiler
./caustic-mk build caustic-as    # the assembler
./caustic-mk build caustic-ld    # the linker
./caustic-mk build caustic-mk    # the build system itself
```

The submodules live in their own repos; clone with `--recurse-submodules` (or run
`git submodule update --init`).

## Documentation

Full documentation lives in [`docs/`](docs/README.md):

- **[Guide](docs/README.md#guide)** — learn the language from scratch: variables,
  types, functions, control flow, structs, enums, generics, memory, modules,
  impl blocks, the build system.
- **[Reference](docs/README.md#reference)** — compiler internals: the full
  pipeline, lexer, parser, semantic analysis, type system, IR, codegen,
  assembler, linker, and per-module stdlib docs.
- **[Examples](examples/README.md)** — runnable programs for every major feature.

## Contributing

Contributions are welcome — see [`CONTRIBUTING.md`](CONTRIBUTING.md). In short:
bug fixes come with a test, performance changes come with a benchmark, and new
stdlib functionality should be generally useful. Caustic values explicitness,
speed, and zero dependencies.

---

*Caustic does not yet ship a formal license file. Until one is added, please
contact the author before redistributing.*
