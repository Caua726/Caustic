# Changelog

All notable changes to Caustic, newest first. Format follows
[Keep a Changelog](https://keepachangelog.com/). Each version links to its GitHub
release; full notes for recent versions live under [`docs/releases/`](docs/releases/).

Versioning: **`v1.x` = stable · `v0.1.x` = beta · `v0.0.x` = alpha.**

## [v0.1.0](https://github.com/Caua726/Caustic/releases/tag/v0.1.0) — 2026-07-22

### Added
- WebAssembly backend (`--target=wasm32-wasi` / `wasm64-wasi`); emits `.wasm` directly, self-hosts on wasm. *Experimental.*
- Linux AArch64 backend (`--target=linux-aarch64`), static ELF. *Experimental.*
- `-O2` optimization tier: global value numbering, full loop unrolling, SIMD auto-vectorizer.
- LLVM-class loop vectorizer (vec2) with cpuid-dispatched AVX2/SSE2 and alias multiversioning.
- Atomics (`atomic_load`/`store`/`add`/`xchg`/`cas`, `fence`) and `with naked` functions.
- `std/thread.cst` — 1:1 OS threads (spawn/join/yield).
- `std/sync.cst` — Spinlock, Mutex, Cond, RwLock, Once, WaitGroup, MPSC Channel.
- Parallel-by-default compilation (per-module emission across all cores).
- Incremental compilation: `.csti` interfaces + `--module-objects` batch build.
- Dynamic stack growth — recursion no longer needs `--stack-size`.
- wasm variadics, function pointers, WASI filesystem/heap/argv, and custom host imports.
- `tools/precommit.sh` (self-host gate), `tools/prerelease.sh`, `tools/release-build.sh`.
- `CHANGELOG.md` and `docs/releases/`.

### Changed
- Vectorizer rewritten (vec2); the v1 pattern-matcher retired.
- README version badge now tracks the latest GitHub release automatically.

### Performance
- Self-host compile RAM 360 MB → ~92 MB (−74%).
- Compile I/O −78%.
- Register allocation O(n²) → O(degree).
- Clean incremental build 4.8s → 1.37s (~3.5×); parallel build ~2.1×.

### Fixed
- `-O2` GVN comparison-CSE miscompile (broke the `-O2` self-host).
- Octal escapes `\NNN` silently corrupted.
- `-O1` register clobbers across `SET_CTX` and struct copy.

### Removed
- Pure-JS WASI runner (`node:wasi` + Deno cover it).

## [v0.0.17](https://github.com/Caua726/Caustic/releases/tag/v0.0.17) — 2026-05-30

### Added
- Shared libraries (`--shared`): `.so` (ELF, System V ABI), `.dll` (PE), `.csl` (universal, own loader).
- `-lcaustic` — link the stdlib dynamically.
- compat `.csl` — one image runs on Linux + Windows + CausticOS.
- `install.sh` one-line installer (`curl … | sh`).
- Assembler `.incbin` directive.

### Changed
- Split `pe_debug.cst` / `link_dynamic.cst` out of the linker; test runner rewritten in Caustic.

### Fixed
- Array-of-struct globals sized 0 → aliased `.bss` and corrupted each other.
- Global const-init now folds `cast` / `<<` `>>` / `&` `|` `^` / `%`.
- Assembler `.byte` dropped all but the first comma-separated value.

## [v0.0.16](https://github.com/Caua726/Caustic/releases/tag/v0.0.16) — 2026-05-28

### Added
- Windows x86_64 native target (`--target=windows-x86_64` → PE/EXE); self-hosts on Win11.
- `with imut` on functions (compile-time evaluation of all-literal calls).
- Conditional imports: `if (cond) use "..." as alias;`.
- CodeView debug (`.debug$S`/`.debug$T`) + `.pdb` sidecar.
- `caustic-mk.exe` (Windows).

### Changed
- Stdlib reorganized into portable facades (`io`/`mem`/`net`/`time`/`env`/`random`/`term`/`process`) dispatching on `os.current`.

## [v0.0.15](https://github.com/Caua726/Caustic/releases/tag/v0.0.15) — 2026-03-31

### Added
- `caustic-lsp` — LSP server: diagnostics, hover, go-to-definition, completion, find-references, rename.
- `-debugparser` flag.
- 13 `docs/stdlib/*.md` documentation files.

## [v0.0.14](https://github.com/Caua726/Caustic/releases/tag/v0.0.14) — 2026-03-31

### Added
- `\xNN` hex escapes, `null` keyword, global string constants.
- `--version`, `--help`, `-q`/`--quiet`, unknown-flag detection.
- bins allocator (O(1) slab, double-free detection) and five submodule allocators.
- stdlib `path`, `process`, `term`; `printf` width/alignment and `sprintf`.

### Changed
- Full freelist elimination across compiler, assembler, and linker.

### Performance
- Semantic −61% (O(1) declare/find); codegen −12%; peak memory −87.5%.
- ~2× faster runtime vs v0.0.13.

### Fixed
- f64→f32 cast used integer truncation instead of `cvtsd2ss`.
- `only`-import type resolution and `asm()` escape sequences.

## [v0.0.13](https://github.com/Caua726/Caustic/releases/tag/v0.0.13) — 2026-03-23

### Added
- `call()` indirect calls with type checking; typed `fn_ptr()` (`TY_FN`).
- f32 literal narrowing.
- stdlib `math`, `sort`, `map`, `random`, `net`, `arena`, `env`, `time`.

### Performance
- O(1) DJB2 hash lookups in the semantic phase.
- First `-O1` self-compile that completes without a crash.

### Fixed
- SRET struct-return stack alignment and hidden-arg counting.
- 50+ stdlib bugs (INT64_MIN, bounds/null checks, double-free, heap coalescing).

## [v0.0.12](https://github.com/Caua726/Caustic/releases/tag/v0.0.12) — 2026-03-21

### Added
- Module system: `use "path" only … as alias`, submodule dot-notation.
- `caustic-mk depend` (git/system dependencies).
- Assembler `cvtsd2ss`, `cvtss2sd`, `movd`.

### Changed
- Float ABI for `extern fn` (f32/f64 in xmm0–7); internal f32 ABI.

### Fixed
- 1/2-byte extern returns (`movzx`); 16-byte stack alignment for extern calls; `SET_CTX` r10 corruption.

## [v0.0.11](https://github.com/Caua726/Caustic/releases/tag/v0.0.11) — 2026-03-20

### Added
- Per-pass optimizer profiling (`--profile`).

### Performance
- `-O1` compile 36% faster (7.7s → 4.9s).

## [v0.0.10](https://github.com/Caua726/Caustic/releases/tag/v0.0.10) — 2026-03-20

### Added
- `-O1` optimizer: constant/copy propagation, store-load forwarding, LICM, strength reduction, DCE, function inlining, peephole.
- Graph-coloring register allocator (iterative coalescing) with a register cache.
- `[base+index*scale]` SIB addressing.

### Performance
- 3.4× faster than `-O0`; competitive with GCC `-O0`.

### Fixed
- Multiple `-O1` correctness bugs (phi handling, branch invalidation, cache invalidation).

## [v0.0.9](https://github.com/Caua726/Caustic/releases/tag/v0.0.9) — 2026-03-18

### Added
- `r15` freed for allocation (10 allocatable registers); LICM for call-free loops.

### Changed
- SSA i32 promotion; `shl` for power-of-two strides; push/pop pruning.

### Performance
- Beats LuaJIT on the benchmark suite.

### Fixed
- 8 bugs (SSA heap overflows, caller-saved spill, OOB constant-fold guard).

## [v0.0.8](https://github.com/Caua726/Caustic/releases/tag/v0.0.8) — 2026-03-18

### Added
- SSA mem2reg (phi nodes, dominance frontiers); store-load forwarding; register coalescing hints.

### Performance
- ~20% faster benchmark; 1024-entry assembler hash table (1.7× faster tokenization).

### Fixed
- 12 bugs (heap overflows, DFS stack overflow, caller-saved spill across calls).

## [v0.0.7](https://github.com/Caua726/Caustic/releases/tag/v0.0.7) — 2026-02-26

### Added
- Strength reduction (div/mod by power-of-two → shift/and); ADDR+STORE fusion; MUL-immediate fold; peephole reload elision.

### Performance
- collatz 6700ms → 4100ms (−39%).

## [v0.0.6](https://github.com/Caua726/Caustic/releases/tag/v0.0.6) — 2026-02-26

### Performance
- Register allocator overhaul (active-set lookup, merge-sort intervals, binary-search `spans_call`, spill-cost eviction): self-hosting compile 3.01s → 0.49s (~6×); binary `.text` −25%.

## [v0.0.5](https://github.com/Caua726/Caustic/releases/tag/v0.0.5) — 2026-02-22

### Added
- `--profile` — pipeline, assembler sub-phase, and codegen timing + compilation stats.

## [v0.0.4](https://github.com/Caua726/Caustic/releases/tag/v0.0.4) — 2026-02-22

### Added
- Interface files (`.csti`, `--emit-interface`); binary pipeline caches (`--emit-tokens`/`-ast`/`-ir`); `--cache <dir>`.

## [v0.0.3](https://github.com/Caua726/Caustic/releases/tag/v0.0.3) — 2026-02-22

### Added
- Dynamic heap sizing; multi-file compilation (`caustic a.cst b.cst -o prog`); `--max-ram`; `mem.gheapreset`; `linux.rename`/`mkdir`.

## [v0.0.2](https://github.com/Caua726/Caustic/releases/tag/v0.0.2) — 2026-02-22

### Added
- Install scripts (`caustic-mk install/uninstall`, `install.sh`); `caustic-mk dist` tarball; DWARF debug info; generic slices + cross-module instantiation; type aliases; unused-variable warnings.

### Changed
- Stdlib resolved relative to the binary (`<bin>/../lib/caustic/`).

## [v0.0.1](https://github.com/Caua726/Caustic/releases/tag/v0.0.1) — 2026-02-21

### Added
- First stable self-hosted release: compiler, assembler, and linker written in Caustic; x86_64 Linux; no libc.
