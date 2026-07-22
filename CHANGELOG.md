# Changelog

All notable changes to Caustic, newest first. Each entry links to its GitHub
release; full notes for recent versions also live under [`docs/releases/`](docs/releases/).

Versioning scheme: **`v1.x` = stable · `v0.1.x` = beta · `v0.0.x` = alpha.**
Every generation of the toolchain self-hosts to a byte-identical fixpoint before release.

## [v0.1.0](https://github.com/Caua726/Caustic/releases/tag/v0.1.0) — 2026-07-22

First release on the beta (`0.1.x`) line — 137 commits since v0.0.17. Full notes:
[`docs/releases/v0.1.0.md`](docs/releases/v0.1.0.md).

- **WebAssembly backend** (`--target=wasm32-wasi` / `wasm64-wasi`) — emits a `.wasm`
  module directly (no assembler/linker step); variadics, function pointers, atomics,
  WASI filesystem/heap/argv, custom host imports, `-O0/-O1/-O2`. The compiler
  **self-hosts on wasm** to a byte-identical fixpoint under `node:wasi` and Deno. *Experimental.*
- **Linux AArch64 backend** (`--target=linux-aarch64`) — static AArch64 ELF, scalar
  stack-home codegen. *Experimental.*
- **`-O2` optimization tier** — global value numbering (cross-block CSE), full loop
  unrolling, and an LLVM-class SIMD auto-vectorizer (**vec2**: mini-SCEV, dependence
  test, legality gate) with cpuid-dispatched AVX2 → SSE2 → scalar.
- **Language threading** — atomics (`atomic_load/store/add/xchg/cas`, `fence`),
  `with naked` functions, `std/thread.cst` (1:1 OS threads) and `std/sync.cst`
  (mutex, cond, rwlock, waitgroup, channel), thread-safe allocators.
- **Parallel-by-default compiler** — per-module emission across all cores
  (work-stealing), byte-identical across `-j`; ~2.1× on the self-host.
- **Incremental compilation** — per-module `.csti` interfaces + `--module-objects`
  batch build; clean build 4.8s → 1.37s (~3.5×).
- **Compile-time RAM −74%** (360 MB → ~92 MB) and **I/O −78%**; regalloc O(n²) → O(degree).
- **Dynamic stack growth** — recursion no longer needs `--stack-size`.
- Fixes: octal escapes `\NNN` silently corrupted; `-O2` GVN comparison-CSE miscompile;
  register-allocation clobber points (`SET_CTX`, struct copy).
- Tooling: native self-host pre-commit gate (`-O0/-O1/-O2` fixpoint); release scripts
  (`tools/prerelease.sh`, `tools/release-build.sh`).

## [v0.0.17](https://github.com/Caua726/Caustic/releases/tag/v0.0.17) — 2026-05-30

- **Shared libraries** (opt-in `--shared`): `.so` (ELF `ET_DYN`, System V ABI —
  callable from C/gcc/Rust), `.dll` (PE export directory), and `.csl` (a universal
  image loaded by Caustic's own loader).
- **`-lcaustic`** links the stdlib dynamically instead of bundling it.
- **compat `.csl`** — one file runs on Linux + Windows + CausticOS.
- **`install.sh`** one-line installer (`curl … | sh`), per-user by default.
- Fixes: array-of-struct globals now sized correctly (were aliasing `.bss`); fold
  `cast`/shift/bitwise/`%` in global const-init; assembler `.byte` multi-value +
  `.incbin` (kernel driver-spec blobs).

## [v0.0.16](https://github.com/Caua726/Caustic/releases/tag/v0.0.16) — 2026-05-28

- **Windows x86_64 native target** (`--target=windows-x86_64` → PE/EXE); full
  self-host on Win11 (byte-identical).
- **`with imut` on functions** — Layer 2 comptime; a tree-walking interpreter folds
  all-literal calls at semantic time.
- **Conditional imports** — `if (cond) use "..." as alias;`, gated on target builtins.
- **Portable stdlib facades** — `io`, `mem`, `net`, `time`, `env`, `random`, `term`,
  `process` all cross-platform via `os.current` dispatch.
- CodeView debug (`.debug$S`/`.debug$T` + `.pdb`); `caustic-mk` on Windows.

## [v0.0.15](https://github.com/Caua726/Caustic/releases/tag/v0.0.15) — 2026-03-31

- **caustic-lsp** — Language Server Protocol: diagnostics, hover (signatures +
  keyword/stdlib docs), go-to-definition, completion, find-references, rename.
- `-debugparser` flag; 13 `docs/stdlib/*.md` documentation files.

## [v0.0.14](https://github.com/Caua726/Caustic/releases/tag/v0.0.14) — 2026-03-31

- Language: `\xNN` hex escapes, `null` keyword, global string constants.
- CLI: `--version`, `--help`, `-q`/`--quiet`, unknown-flag detection.
- Performance: O(1) `declare_fn`/`find_module` (semantic −61%), per-function IR
  pipeline (−87.5% peak memory), word-at-a-time memops.
- **bins allocator** — O(1) slab alloc/free with bitmap double-free detection; five
  submodule allocators (freelist, bins, arena, pool, lifo).
- New stdlib: `path`, `process`, `term`.
- ~2× faster runtime vs v0.0.13; `-O1` within 2–4× of GCC `-O2`.

## [v0.0.13](https://github.com/Caua726/Caustic/releases/tag/v0.0.13) — 2026-03-23

- **`call()` indirect calls** through function pointers with full type checking;
  typed function signatures (`TY_FN`).
- f32 literal narrowing; O(1) DJB2 hash lookups in the semantic phase.
- First release where `-O1` self-compile completes without crashing.
- 8 new stdlib modules: `math`, `sort`, `map`, `random`, `net`, `arena`, `env`, `time`.

## [v0.0.12](https://github.com/Caua726/Caustic/releases/tag/v0.0.12) — 2026-03-21

- **Module system** — `use "path" only name1, name2 as alias`, submodule dot-notation
  (`mod.submod.func()`).
- `caustic-mk depend` — git/system dependency resolution.
- Float ABI for `extern fn` (f32/f64 in xmm0–7); f32 internal calling convention;
  16-byte stack alignment for extern calls.

## [v0.0.11](https://github.com/Caua726/Caustic/releases/tag/v0.0.11) — 2026-03-20

- **36% faster `-O1` compile** (7.7s → 4.9s): O(1) lookup tables in strength
  reduction, function splitting, faster mem2reg.
- Per-pass optimizer profiling (`--profile`).

## [v0.0.10](https://github.com/Caua726/Caustic/releases/tag/v0.0.10) — 2026-03-20

- **Complete `-O1` optimizer pipeline** — constant/copy propagation, store-load
  forwarding, LICM, strength reduction v2, DCE, function inlining, peephole.
- **Graph-coloring register allocator** — iterative register coalescing (George &
  Appel), optimistic Briggs coloring, loop-depth-weighted spill costs.
- i32 SSA promotion, typed phi destruction; `[base+index*scale]` SIB addressing.
- **3.4× faster than `-O0`**, competitive with GCC `-O0`.

## [v0.0.9](https://github.com/Caua726/Caustic/releases/tag/v0.0.9) — 2026-03-18

- Codegen `r15` freed for allocation (10 allocatable registers); SSA i32 promotion;
  LICM for call-free loops; push/pop pruning; `shl` for power-of-two strides.
- **Beats LuaJIT** on the benchmark suite.

## [v0.0.8](https://github.com/Caua726/Caustic/releases/tag/v0.0.8) — 2026-03-18

- **SSA mem2reg** — promotes stack variables to virtual registers (phi nodes,
  dominance frontiers, SSA destruction).
- Store-load forwarding; register coalescing hints; 1024-entry assembler hash table.
- ~20% faster benchmark; hardened stdlib bounds checks.

## [v0.0.7](https://github.com/Caua726/Caustic/releases/tag/v0.0.7) — 2026-02-26

- Strength reduction (div/mod by power-of-two → shift/and), ADDR+STORE fusion, MUL
  immediate fold, peephole reload elision, signed div/mod fast path.
- collatz N=10M: 39% faster (6.7s → 4.1s).

## [v0.0.6](https://github.com/Caua726/Caustic/releases/tag/v0.0.6) — 2026-02-26

- Register allocator: active-set O(7) lookup, merge-sort intervals (O(N log N)),
  binary search in `spans_call`, spill-cost eviction.
- **Self-hosting compile 3.01s → 0.49s (~6×)**; binary `.text` 25% smaller.

## [v0.0.5](https://github.com/Caua726/Caustic/releases/tag/v0.0.5) — 2026-02-22

- `--profile` flag: pipeline breakdown, assembler sub-phases, top codegen functions
  by time; compilation stats (bytes, tokens, functions, IR insts, output sizes).

## [v0.0.4](https://github.com/Caua726/Caustic/releases/tag/v0.0.4) — 2026-02-22

- Interface files (`.csti`, `--emit-interface`); binary pipeline caches
  (`--emit-tokens`/`--emit-ast`/`--emit-ir`); `--cache <dir>` skips lex/parse/
  semantic/IR when source is unchanged.

## [v0.0.3](https://github.com/Caua726/Caustic/releases/tag/v0.0.3) — 2026-02-22

- Dynamic heap sized from source + imports (not a fixed 128 MB); multi-file
  compilation (`caustic a.cst b.cst -o prog`); `--max-ram <MB>`.

## [v0.0.2](https://github.com/Caua726/Caustic/releases/tag/v0.0.2) — 2026-02-22

- Install scripts (`caustic-mk install/uninstall`, `install.sh`); stdlib resolution
  relative to the binary; `caustic-mk dist` tarball; `.file`/`.loc` DWARF debug info;
  generic slices + cross-module generic instantiation; type aliases; unused-variable
  warnings.

## [v0.0.1](https://github.com/Caua726/Caustic/releases/tag/v0.0.1) — 2026-02-21

- **First stable self-hosted release.** Compiler, assembler, and linker fully written
  in Caustic; stable self-compilation; x86_64 Linux, no libc dependency.
