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
./caustic <source.cst>                       # Generates <source.cst>.s assembly
./caustic <source.cst> -o prog               # Full pipeline: compile + assemble + link
./caustic -c <source.cst>                    # Compile only (no main required, for libraries)
./caustic -O1 <source.cst> -o p              # Optimized build
./caustic --target=linux-x86_64 ... (default)
./caustic --target=windows-x86_64 src.cst -o p.exe   # Cross-compile PE/COFF for Windows
./caustic -debuglexer <file>                 # Debug tokenization
./caustic -debugparser <file>                # Debug AST
./caustic -debugir <file>                    # Debug IR generation
./caustic --profile <file> -o p              # Show per-phase timing

# Shared libraries (opt-in; static is the default):
./caustic --shared lib.cst -o libfoo.so                         # ELF .so (Linux) — callable from C/gcc/Rust (System V ABI)
./caustic --target=windows-x86_64 --shared lib.cst -o foo.dll   # PE .dll (export directory)
./caustic --target=caustic-x86_64 --shared --mode=pure lib.cst -o lib.csl   # Universal .csl (our own loader)
./caustic main.cst -lcaustic -o main                            # Dynamically link the Caustic stdlib (libcaustic.so)

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

Caustic is a self-hosted native x86_64 compiler targeting **Linux (ELF) and Windows (PE)** with no LLVM or runtime dependencies. Linux builds use direct syscalls; Windows builds use kernel32/ws2_32/bcrypt DLL imports via the MS x64 ABI (System V internally, trampolined at FFI boundary).

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
syscall(1, 1, "hello", 5);  // direct syscall (Linux only — rejected at codegen on Windows builds)
asm("mov rax, 1\n");        // inline asm
cast(*u8, address);         // type cast

// Extern FFI (DLL imports on Windows builds)
extern "kernel32.dll" fn ExitProcess(code as u32) as void;
extern "ws2_32.dll" fn WSAStartup(version as u32, data as *u8) as i32;
// Call sites lower to IR_FFI_CALL: shadow space + SysV→MS-x64 reorder
// + `call qword ptr [rip+__imp_<name>]` (FF 15) via IAT.

// Compile-time target detection
let is i32 as os_id with imut = os.current;             // folded at parse: 1 = Linux, 2 = Windows
if (os.current == os.OS_LINUX) { syscall(1, ...); }
else if (os.current == os.OS_WINDOWS) { os.windows.WriteFile(...); }
```

## Cross-target programming

Code that needs to do OS-specific work uses `std/os.cst` as the single
entry point. The `os.current` identifier folds to a literal `1` (Linux)
or `2` (Windows) at parse time, and the IR generator dead-strips
branches whose condition evaluates to a constant — so the wrong-target
code path never reaches codegen and the syscall gate never fires:

```cst
use "std/os.cst" as os;

fn write_byte(b as u8) as void {
    if (os.current == os.OS_LINUX) {
        syscall(1, 1, &b, 1);                // Linux: raw write(stdout, &b, 1)
    } else if (os.current == os.OS_WINDOWS) {
        let is u32 as w with mut = 0;
        os.windows.WriteFile(
            os.windows.GetStdHandle(os.windows.STD_OUTPUT_HANDLE),
            &b, 1, &w, cast(*u8, 0));
    }
}
```

How the dispatch lowers:
1. `os.current` is a `let with imut` global whose initialiser folds to a
   literal at semantic time (`__target_is_linux * 1 + __target_is_windows * 2`).
2. AST-level const-prop rewrites `os.current` references to `NK_NUM`.
3. AST-level const-fold rewrites the comparison to `NK_NUM(0|1)`.
4. IR-gen dead-branch elim drops the inactive arm of `if (NK_NUM)`.
5. Codegen only sees the active arm — no kernel32 in Linux binaries,
   no `syscall` instruction in Windows binaries.

In practice, most user code just imports the **portable facades**
(`std/io.cst`, `std/mem.cst`, `std/net.cst`, etc) which already use the
`os.current == os.OS_X` pattern internally. Direct OS calls via
`os.linux.X` / `os.windows.X` are for code that genuinely needs raw
access; everything else delegates through the facades.

### `with imut` semantics

Caustic's `imut` qualifier means **both** "doesn't change after init"
**and** "compile-time-known":

- `let is i32 as X with imut = <expr>;` — init must fold to a literal at
  semantic time. The compiler propagates the literal at every use site.
  Foldable inits: literals, arithmetic on literals, references to other
  imut globals with literal-foldable inits.
- `let is i32 as X = some_runtime_call();` — variable is still immutable
  (Caustic default), but the value is decided at runtime; the compiler
  emits a normal load.

This unification (`imut` = "no runtime dependency") is what makes
`os.current == os.OS_LINUX` dead-strip cleanly without a `comptime`
keyword.

### Parser builtins (low-level)

The compiler injects two integer literals at parse time:

- `__target_is_linux` — `1` on `--target=linux-x86_64`, `0` otherwise
- `__target_is_windows` — `1` on `--target=windows-x86_64`, `0` otherwise

User code should generally **not** touch these directly — they exist as
implementation primitives for `os.cst`'s `current` computation. Same
mechanism as `__builtin_va_start` (parser intercepts the identifier and
emits a literal NK_NUM in its place).

## Standard Library (`std/`)

The stdlib is organised as **portable facades** (top-level) backed by
**per-OS impls** in subdirectories. All facades work on both targets via
the `os.current == os.OS_X` dispatch pattern; user code imports the
facade only.

**Raw OS bindings:**
- `os.cst` — Hub re-exporting `os.linux` + `os.windows` submodules.
  Exposes `os.current` (i32, 1=Linux, 2=Windows) and `os.OS_LINUX` /
  `os.OS_WINDOWS` constants — used by portable code for target dispatch.
- `os/linux.cst` — 40+ syscall wrappers (file, process, memory, network, time)
- `os/windows.cst` — kernel32, ws2_32, bcrypt extern declarations
  (CreateFileA, WriteFile, VirtualAlloc, WSASocketA, BCryptGenRandom, etc)

**Portable facades** (dispatch internally via `os.current`):
- `mem.cst` — Memory management hub with 5 allocators:
  - `mem/freelist.cst` — O(n) alloc/free, address-ordered coalescing
  - `mem/bins.cst` — O(1) alloc/free, slab-based, bitmap double-free detection
  - `mem/arena.cst` — O(1) bump allocator, bulk free
  - `mem/pool.cst` — O(1) fixed-size slots, optional debug
  - `mem/lifo.cst` — O(1) stack-like allocator
  - `mem/linux.cst` / `mem/windows.cst` — page_alloc/page_free per OS (mmap vs VirtualAlloc)
- `io.cst` — Buffered I/O, printf, file/directory operations
  - `io/linux.cst` / `io/windows.cst` — write/read/open/lseek per OS
- `string.cst` — Dynamic strings (concat, find, substring, split, trim, replace) — pure, no OS deps
- `slice.cst` — Generic dynamic array (`Slice gen T`)
- `types.cst` — `Option gen T` and `Result gen T, E`
- `map.cst` — Hash maps (MapI64 splitmix64, MapStr FNV-1a)
- `math.cst` — abs, min, max, pow, gcd, lcm, align_up/down — pure
- `sort.cst` — quicksort, heapsort, mergesort with fn_ptr comparators — pure
- `random.cst` — xoshiro256** PRNG, rejection sampling range. Dispatches
  entropy seed to `getrandom` (Linux) or `BCryptGenRandom` (Windows).
- `net.cst` — TCP/UDP sockets, bind/listen/accept, poll. Auto-calls
  `WSAStartup` on Windows; expose `net.init()` / `net.cleanup()` for
  explicit lifecycle control.
- `time.cst` — Monotonic/wall clock, sleep, elapsed
  - `time/linux.cst` / `time/windows.cst` — clock_gettime vs QueryPerformanceCounter / GetSystemTimeAsFileTime
- `env.cst` — argc/argv, getenv. On Windows, getenv routes through
  `GetEnvironmentVariableA`; on Linux, walks the inherited envp[].
  argc/argv come from the Windows entry stub's `__caustic_parse_argv`
  (CommandLineToArgvA-equivalent, hand-rolled in `caustic-ld`).
- `term.cst` — Terminal: ANSI escapes (portable bytes), raw mode
  (termios on Linux, GetConsoleMode + ENABLE_VIRTUAL_TERMINAL_INPUT on Windows),
  size query (TIOCGWINSZ vs GetConsoleScreenBufferInfo)
- `process.cst` — fork/execve/wait on Linux; CreateProcessA +
  WaitForSingleObject + GetExitCodeProcess on Windows. `capture()` is
  Linux-only for now (Windows needs CreatePipe wiring).
- `arena.cst` — Standalone bump allocator (delegates to `mem/core.cst`)
- `compatcffi.cst` — C FFI struct passing helpers — pure, debug prints
  route through `io.cst`

## Shared Libraries (.so / .dll / .csl)

Static linking is the **default** — every binary is a self-contained, zero-dependency
static ELF/PE (raw syscalls, no libc, only the functions DCE keeps). Shared libraries
are **opt-in** via `--shared`, in three flavours — two for external interop, one
universal for the Caustic toolchain itself:

| Output | Flag | Loaded by | Purpose |
|--------|------|-----------|---------|
| **`.so`** (ELF `ET_DYN`) | `--shared` (linux target) | system `ld-linux` | interop — callable from C/gcc/Rust (System V ABI). Real `.hash`/`.dynsym`/`.dynamic`; flat `file_off==vaddr` layout. |
| **`.dll`** (PE) | `--target=windows-x86_64 --shared` | Windows loader | PE export directory (sorted names + ordinals, appended to `.rdata`). Export RVAs add `STUB_SIZE` to skip the prepended entry stub. |
| **`.csl`** (`CSL_` image) | `--target=caustic-x86_64 --shared --mode=pure\|compat` | **our own** loader (`std/csl_loader.cst`) | the toolchain's own universal shared runtime — one file runs on Linux + Windows + CausticOS. |

### `-lcaustic` — dynamic stdlib
Makes a program import the stdlib from `libcaustic.so` instead of statically bundling
it: codegen externalizes every `_std_*` function (skips its body) so the calls become
dynamic imports the linker resolves at load. The self-host bootstrap passes a clean
gen1==gen4 fixpoint built with `-lcaustic`. For a single program the static build
still wins on RAM (no loader); the dynamic stdlib pays off only when many programs
share one resident `libcaustic.so`. The libcaustic.so is built from a wrapper that
imports every std facade, compiled `-c --allow-unsupported` (so the per-OS-unsupported
`__compile_error` stubs become no-ops and all globals are kept).

### `.csl` + the custom loader (closed ecosystem)
The `.csl` is consumed **only** by the Caustic toolchain via **our own loader** —
never by `ld-linux`/the Windows loader. That is *why* one `.csl` is cross-OS: we own
the format + loader + consumers, so there are no system-ABI constraints.

- **Format** (`CSL_`, see `caustic-linker/cse_writer.cst:cse_emit_csl_image`): a
  CSE-family segment table (`vaddr, file_off, file_size, mem_size, perms`) + an
  **export table** (`name_off, vaddr` + string blob). `flags` bit 0: `0` = pure
  (CausticOS), `1` = compat (both OS code paths present, dispatched at run time).
- **compat mode** relies on `os.current()` being a **runtime** value: in `--mode=compat`
  the `__compat_mode` builtin is 1, so `os.current()` returns the `mut` global
  `_runtime_os_current` (set by the loader) instead of folding to a literal — both the
  Linux and Windows branches survive into the image.
- **Loader** (`std/csl_loader.cst`): `csl_open()` reads the image and mmaps its
  segments RWX (`mmap` syscall on Linux / `VirtualAlloc` on Windows, selected by
  `os.current()` folding per target); `csl_resolve(name)` returns the loaded address.
  Code is PC-relative so it runs at any base with no relocation.
- **Verified**: the same `.csl` loads + runs under **both** Linux (`mmap`) and Windows
  (`VirtualAlloc` under wine) — `cst_add(40,2)` → 42 on both.

**Still TODO** for transparent use: the linker must auto-embed the loader as `_start`
plus a GOT (so stdlib calls go through `call [GOT+slot]` and `-lcsl` is transparent),
and the Windows compat path needs kernel32 emission on the caustic target + a PEB-walk
in the loader to resolve `kernel32` (`.csl` has no import table yet).

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
- **PE debug info is Caustic-linker-only**: on `--target=windows-x86_64`, codegen emits `.cstdebug` / `.cstsigs` / `.cststructs` / `.cstlocals` custom sections per .obj. `caustic-ld` synthesizes the EXE's CodeView `.debug$S`/`.debug$T` content and the matching `.pdb` from those sections at link time. External linkers (lld-link, MSVC link.exe) do not understand these — linking Caustic objects with them silently drops all debug info. Same applies to `.cstimport` (PE import directives). Pure ELF builds never see these sections (codegen gates on `target.object_format == 2`).
- **`syscall(...)` is Linux-only**: the codegen-time gate rejects emission of the `syscall` instruction when `target.syscall_supported == 0` (Windows). The check fires only for IR that survived DCE — so `syscall` calls inside `if (os.current == os.OS_LINUX)` branches don't break Windows builds (the branch is dead-stripped before codegen).
- **PE section RVAs are dynamic**: `caustic-ld` sizes each PE section to its actual content (`.text` size may span multiple 4 KiB pages); `.idata`/`.rdata`/`.data`/`.pdata`/`.xdata`/`.reloc` RVAs follow at the next page-aligned slot. Earlier hardcoded RVAs caused section-overlap page faults once `.text` exceeded one page.
- **WSAStartup is auto-init**: `std/net.cst` calls `WSAStartup(MAKEWORD(2,2), ...)` on first use of any socket dispatcher. Programs that never touch sockets don't pay for it (function-level DCE drops the loader-side `__imp_WSAStartup` entry).

## Future work (not implemented)

- **`with imut` on functions** — extend the `imut` qualifier from variables to functions, meaning "pure (no syscall, no extern fn, no mut globals, only calls to other imut) and parse-time evaluable when args are literals". When the compiler sees a call to a `with imut` fn with all-literal args, run the body in a tree-walking interpreter at semantic time and substitute the result as a literal. This is the "Layer 2" of the comptime design — variables and globals already have this in the form of init-expression const-prop; functions don't yet. Plan: ~500 lines (parser annotation + semantic purity check + AST interpreter). Defer until a concrete use case appears (lookup-table generation, compile-time string hashing, etc). See `/home/caua/.claude/projects/-home-caua-Documentos-Projetos-Pessoais-Caustic/memory/comptime_design.md` for the full design discussion.
