# Changelog

All notable changes to Caustic, newest first. Each version links to its GitHub
release; full notes for recent versions also live under [`docs/releases/`](docs/releases/).

Versioning scheme: **`v1.x` = stable · `v0.1.x` = beta · `v0.0.x` = alpha.** Every
generation of the toolchain self-hosts to a byte-identical fixpoint (gen2 == gen3 ==
gen4) before it ships — the shipped binary is what the compiler converges to when it
compiles itself with itself.

---

## [v0.1.0](https://github.com/Caua726/Caustic/releases/tag/v0.1.0) — 2026-07-22

First release on the beta (`0.1.x`) line — 137 commits since v0.0.17. The compiler now
emits **five** targets (Linux x86_64, Windows x86_64, Linux AArch64, WebAssembly, and
Caustic's own CSE) and self-hosts to a byte-identical fixpoint at `-O0`/`-O1`/`-O2` on
x86 *and on wasm*. Full notes: [`docs/releases/v0.1.0.md`](docs/releases/v0.1.0.md).

### New backends

- **WebAssembly** (`--target=wasm32-wasi` / `wasm64-wasi`) — emits a complete `.wasm`
  binary module directly, bypassing the assembler and linker. Every vreg is an i64
  local (float ops reinterpret i64↔f64 around the op); address-taken locals and spills
  live on a shadow stack in linear memory (`$sp` global + per-function `$fp`);
  unstructured jumps become structured control flow via a dispatch loop (`br_table` on
  a `$state` local, built from the CFG); the calling convention is positional. One
  backend serves **two memory models** keyed on `target.arch` — wasm32 wraps addresses
  to i32, wasm64 uses i64 addresses + the memory64 flag. Supports variadics
  (memory-packed varargs ABI), function pointers (funcref table + `call_indirect`),
  atomics (lowered single-threaded), the WASI filesystem/heap/argv, and custom host
  imports (any non-WASI `extern "<module>"` becomes a host import for wasm↔JS interop).
  **The compiler compiles ITSELF to a byte-identical wasm compiler** (gen1==gen2==gen3,
  2,526,829 bytes) that compiles itself again, under the standard `node:wasi` and Deno.
  *Experimental.*
  - Key fix: the linear memory is emitted **`shared`** (flag 0x03/0x07, 2 GiB cap). A
    plain wasm memory grows by *detaching* its backing `ArrayBuffer`; a host caching a
    view over it (node:wasi does) then holds a dangling reference and SIGSEGVs in V8's
    GC the next time the heap grows. A shared memory is `SharedArrayBuffer`-backed and
    grows in place, so cached views stay valid.
  - `page_alloc` bump-allocates in 16 MiB chunks over `memory.grow` (append-only), to
    cut the number of grows — and thus host-side reallocations — by orders of magnitude.
- **Linux AArch64** (`--target=linux-aarch64`) — static AArch64 ELF via a scalar
  stack-home backend (no register allocator; every vreg spills), the same all-spill
  model the wasm backend mirrors. Atomics and threads supported. *Experimental.*

### Optimization: a real `-O2` tier

- **Global value numbering** (cross-block CSE) over the existing CFG + dominator tree
  (until now used only by mem2reg). Safe for Caustic's multi-def IR: only CSEs
  single-def expressions and resolves operands through single-def MOV-copy chains to
  their ultimate source. Plus full loop unrolling for small constant-trip loops (run at
  codegen time, after value numbering, so reused vregs stay correct) and GVN extended
  to float ops.
- **SIMD auto-vectorizer** — reductions (i64/i32/f64), elementwise maps
  (`c[i]=a[i] OP b[i]`, `+ − × ÷`) and broadcasts (`c[i]=a[i] OP k`), over constant
  *and* runtime lengths, with runtime **alias multiversioning** (a disjoint-ranges check
  branches to the packed loop, else the scalar loop stays as both fallback and
  remainder) and a cpuid-dispatched **AVX2** (256-bit) path that falls back to SSE2 then
  a scalar remainder. AVX2 is emitted as raw VEX bytes — no assembler change.
- **vec2** — the vectorizer was rewritten from a pattern-matcher into an LLVM-class
  Legal→Plan→Execute loop vectorizer: natural-loop discovery on the CFG/dom
  infrastructure, a mini-SCEV affine-address analysis (`{base,+,step}`), IV/recurrence
  descriptors, and an LAA-shaped memory-dependence test feeding a legality gate. v1 (the
  pattern-matcher) was retired.
- **`-O2` GVN miscompile fix** — GVN was CSEing *comparison* results across blocks,
  which is unsound against the instruction selector's compare-into-branch fusion: when a
  comparison feeds a conditional branch, isel emits `cmp; jcc` and never materialises the
  boolean into its slot, so a CSE'd second consumer read a location the fused branch
  never wrote → garbage. This silently broke the `-O2` self-host (a compiler built at
  `-O2` produced wrong code) while `-O0`/`-O1` stayed fine — which is exactly why the new
  pre-commit gate self-hosts at all three optimization levels. Restricting GVN to
  arithmetic/address ops fixes it (recomputing a comparison is a single `cmp`).

The x86 auto-vectorizer stays x86-only — its runtime AVX2 dispatch and 256-bit ops don't
map to wasm's 128-bit v128 — so `-O2` wasm is correct scalar code the engine vectorizes.

### Threading and a parallel compiler

- **Language threading** — atomic builtins (`atomic_load/store/add/xchg/cas` + `fence()`,
  seq_cst; wired into every optimizer side-effect list so `-O1` never reorders across
  them), `with naked` functions (asm-only body, no prologue — enables the thread
  trampolines), `std/thread.cst` (1:1 OS threads via raw `clone(2)` + `CLONE_SETTLS` on
  Linux / `CreateThread` on Windows), `std/sync.cst` (Spinlock, Mutex — Drepper 3-state
  futex + spin, Cond, RwLock, Once, WaitGroup, bounded MPSC Channel), and thread-safe
  allocators (a lazy futex lock around bins+freelist, no-op single-thread fast path).
- **Parallel-by-default builds** — `caustic x.cst -o out` forks per-module emission
  across all cores. Dynamic **work-stealing** over a `MAP_SHARED` page holding an atomic
  task counter (a COW page would give each process its own copy) drains the module list;
  the whole-program inline pass runs per-worker off a pristine IR snapshot (cross-module
  inlining preserved, output byte-identical no matter which worker emits a module) and
  the frontend `optimize_func` — the real ~300 ms cost, previously done twice — is
  deferred to the workers. Byte-identical across `-j`; ~2.1× on the self-host, per-gen
  build ~2.2s → ~1.0s over the branch.
- **Dynamic stack growth** — a recursive / indirect-call program (whose worst-case stack
  is statically undecidable) no longer errors asking for `--stack-size`: a prologue stack
  probe commits more stack on demand via `__caustic_stkgrow`.

### Incremental compilation

Separate compilation backed by per-module binary interfaces (`.csti` v4: types,
signatures, `imut` bodies, generic templates; regular bodies stripped), topological
link-time DCE, and a `--module-objects` batch cold-build that runs the whole-program
frontend **once** then loops codegen+assemble per module (killing the per-module
process-spawn floor and the redundant re-analysis). Clean build of the compiler
**4.8s → 1.37s (~3.5×)**, dev-loop ~247 ms, `.csti` 305 MB → 2 MB.

### Compile-time RAM −74%, I/O −78%

- Self-host `-O1` peak RSS **360 MB → ~92 MB**. The body-AST (~184K nodes) moved from
  the bins slab allocator (which rounded a 312 B node up to the 384 B class) to a **bump
  allocator** at exact size, freed wholesale right after IR-gen; generic-instance clones
  route through the same transient arena (−18 MB); low-cardinality `Node`/`IRInst`/
  `Variable` fields narrowed i32→i8/i16; rare decl-only `Node` fields moved behind a lazy
  side-struct so body nodes carry 8 B not 32 B (−3.9 MB); imported-module token arrays
  freed after semantic (−11.7 MB); the body-node arena grows on demand instead of
  reserving 256 MB up front (VmPeak 430 → 201 MB).
- **Regalloc O(n²) → O(degree)** — codegen was 86% of build time; the `-O1`
  graph-coloring allocator scanned all n vregs to enumerate a node's neighbors. An
  adjacency-list index (with the bit matrix kept as the O(1) pair-query oracle) plus
  dense vreg compaction cut per-gen build 1.27s → 0.97s.
- **In-memory `.s` pipeline** — codegen captures assembly into a buffer; the peephole
  and the in-process assembler read straight from it, removing four disk round-trips of
  the ~6.6 MB assembly. With `--no-asm-cache` (skip the `.s` debug-cache write), self-host
  compile I/O drops ~32 MB → ~7 MB (−78%); a 1 MB/s 4-gen bootstrap goes ~145s → ~46s.

### Correctness fixes (root cause)

- **Octal escapes `\NNN`** were silently corrupted — the string decoder only handled
  `\0`, so `\012`/`\377`/`\200` dropped the backslash (every byte ≥ 128 unreachable via
  octal). Both string and char paths now decode C-style 1–3 digit octal. A follow-up
  fixed the `.string` emitter fusing a NUL with a following octal-digit byte (`\0`+`'5'`
  → `\05`), which dropped a byte and misaligned the blob.
- **Register-allocation clobber points** — `SET_CTX` emits `xor r10,r10` and struct copy
  emits `rep movsb` (clobbers rdi/rsi/rcx), but those registers are in the allocatable
  pool; a value live across one came out wrong. Both are now recorded as clobber points
  so the crosses-call spill logic evicts the affected value — fixes `-O1` miscompiles in
  closure/method-call and struct-field-copy code.

### Tooling

- **Native self-host pre-commit gate** (`tools/precommit.sh`) — builds the toolchain
  from source and self-hosts to a 4-generation fixpoint at `-O0`, `-O1` **and** `-O2`
  (gen2==gen3==gen4 byte-identical + gen4 must compile correctly), checks every example
  is identical across levels, and runs the unit suite. Cross checks (aarch64/qemu,
  windows/wine, C-interop) live in `tools/check-cross.sh` and never gate a commit.
- **Release scripts** — `tools/prerelease.sh` refuses to release without a version bump
  past the latest tag; `tools/release-build.sh` builds the shipped toolchain as the
  settled gen4 `-O2` fixpoint (compiler→gen4, toolchain→gen4, compiler→gen4 again).
- **LSP** — `handlers.cst` split into nav/complete/sig modules; hover docs + completions
  for the atomics and target builtins.

---

## [v0.0.17](https://github.com/Caua726/Caustic/releases/tag/v0.0.17) — 2026-05-30

Shared libraries, a dynamic stdlib, a cross-OS `.csl`, and a one-line installer.

- **Shared libraries** (opt-in `--shared`) in three flavours: `.so` (ELF `ET_DYN`, real
  `.hash`/`.dynsym`/`.dynamic`, System V ABI — callable from C/gcc/Rust), `.dll` (PE
  export directory, sorted names + ordinals), and `.csl` (a universal `CSL_` image loaded
  by Caustic's **own** loader — one file runs on Linux + Windows + CausticOS because the
  toolchain owns the format, loader, and consumers, so there are no system-ABI
  constraints). Static linking stays the default.
- **`-lcaustic`** — link the stdlib dynamically from `libcaustic.so` instead of bundling
  it: codegen externalizes every `_std_*` function so calls become dynamic imports.
- **compat `.csl`** — a single image carrying both OS code paths; the per-OS loader writes
  its compile-time-known OS into the lib's `_runtime_os_current` (one store, not runtime
  probing) so `os.current()` dispatches correctly. Verified: the same 242 KB
  `libcaustic.csl` runs under both Linux (mmap) and Windows (VirtualAlloc, under Wine).
- **`install.sh`** — `curl -fsSL … | sh`, per-user into `~/.local` with no sudo by
  default; `--custom` picks prefix/tools/stdlib pieces, `--system` targets `/usr/local`.
- **Fix — array-of-struct globals were aliasing `.bss`.** A struct/enum array element
  referenced by name still had size 0 at parse time, so the array was sized 0; a 0-byte
  `.bss` global reserves no space, so several uninitialized array-of-struct globals got
  the **same address** and writes through one clobbered the others. This corrupted the
  CausticOS kernel's ACPI MADT tables (→ garbage IOAPIC address → #PF) *and* the
  compiler's own such globals (a generation built by the buggy compiler diverged from the
  `-O1` fixpoint). Fixed by recomputing the array size once the element type resolves.
- **Fix — assembler `.byte` dropped comma-separated values.** It emitted only the first,
  so `.byte 0x8F, 0x07` lost `0x07`; hand-written raw-opcode stubs went byte-short,
  misaligning the stream into an invalid opcode → `#UD` → triple fault on freestanding
  kernels. Also implemented `.incbin` (was missing entirely, so kernels splicing driver
  blobs registered zero drivers).
- Fold `cast` / `<<` `>>` / `&` `|` `^` / `%` in global const-init (were wrongly rejected
  as non-constant). Linker/codegen split into `pe_debug.cst` + `link_dynamic.cst`.

Binary is the `-O1` self-host gen4 (gen2 == gen3 == gen4, byte-identical).

---

## [v0.0.16](https://github.com/Caua726/Caustic/releases/tag/v0.0.16) — 2026-05-28

The Windows port — a second native target, cross-compiled from Linux.

- **Windows x86_64 native target** (`--target=windows-x86_64`) — produces PE/EXE via the
  MS x64 ABI (SysV→MS argument reorder, 32-byte shadow space, `call qword ptr
  [rip+__imp_<fn>]` through the IAT). Full self-host on Win11 (stage2==stage3==stage4
  byte-identical); the compiler, assembler, and linker all ship as native `.exe`s.
- **`with imut` on functions** — Layer 2 comptime: a pure (no syscall/extern/mut-global)
  function called with all-literal args is evaluated by a tree-walking interpreter at
  semantic time and substituted as `NK_NUM`.
- **Conditional imports** — `if (cond) use "..." as alias;`, gated on parser builtins
  (`__target_is_linux`/`__target_is_windows`), so the wrong-target backend is never
  loaded.
- **Portable stdlib facades** — `io`, `mem`, `net`, `time`, `env`, `random`, `term`,
  `process` all cross-platform via `os.current` dispatch (which folds to a compile-time
  literal, dead-stripping the inactive OS path before codegen).
- **CodeView debug** — `.debug$S`/`.debug$T` + a `.pdb` sidecar with a path-independent
  GUID (basename only). `caustic-mk` ported to Windows via the portable facades.
- Bootstrap: Linux ELF `-O0` gen2==gen3==gen4 = `943474f7…`; Win11 PE `-O0`
  stage2==stage3==stage4 = `38f358ee…`.

---

## [v0.0.15](https://github.com/Caua726/Caustic/releases/tag/v0.0.15) — 2026-03-31

Editor tooling.

- **caustic-lsp** — a Language Server Protocol implementation: real-time diagnostics from
  the compiler; hover with full function signatures + doc comments, hover docs for every
  keyword and every stdlib symbol (from `docs/stdlib/`); go-to-definition for
  fn/struct/enum/type/variable; completion of symbols, keywords, and module members after
  `mod.`; find-references; and rename across all occurrences.
- `-debugparser` flag dumps the full AST; 13 `docs/stdlib/*.md` files with signatures,
  descriptions, parameters, and examples. VS Code extension available separately.

---

## [v0.0.14](https://github.com/Caua726/Caustic/releases/tag/v0.0.14) — 2026-03-31

56 commits; +4098 −980 lines. Language polish, a CLI, a new allocator, and a 2× runtime.

- **Language** — `\xNN` hex escapes in string/char literals, a typed `null` keyword
  (works with any `*T`), and global string constants (`let is *u8 as ESC with imut =
  "\x1b[0m";`).
- **CLI** — `--version`, `--help`, `-q`/`--quiet` (was 110+ lines per build), and
  unknown-flag detection (`--typo` errors instead of silently becoming input). Fixed
  f64→f32 cast (was integer truncation instead of `cvtsd2ss`), `only`-import type
  resolution, and `asm()` escape sequences.
- **Performance** — O(1) `declare_fn`/`struct`/`enum` via hash lookup (semantic **−61%**),
  O(1) `find_module`, sorted interference sweep with early exit (codegen **−12%**), a
  per-function IR pipeline that frees IR after codegen and regenerates on demand
  (**−87.5% peak memory**), word-at-a-time memcpy/memset, string dedup for `.rodata`.
- **bins allocator** — O(1) slab alloc/free with bitmap double-free detection; five
  submodule allocators (freelist, bins, arena, pool, lifo); freelist eliminated across
  the compiler + assembler + linker.
- **New stdlib** — `path` (join/dirname/basename/ext/`~`), `process` (run/spawn/wait/
  capture), `term` (raw mode, ANSI escapes, size, read-key). `printf` gains
  width/alignment; `net` gains `htons`/`ip_parse`/`dial`.
- Benchmarks: **2× faster runtime** vs v0.0.13 at `-O0`, another **2.1×** at `-O1`; `-O1`
  within 2–4× of GCC `-O2` (sieve 45ms vs GCC-O2 13ms, matmul 33ms vs 11ms).

---

## [v0.0.13](https://github.com/Caua726/Caustic/releases/tag/v0.0.13) — 2026-03-23

Function pointers become real, and the stdlib doubles.

- **`call()` indirect calls** — call through a function pointer with full type checking
  (`call(fn_ptr, arg1, arg2)`); `fn_ptr()` now returns a typed `TY_FN` carrying
  parameter/return types, so `fn(i32, i64) as bool` is a usable type.
- f32 literal narrowing (float literals like `10.0` auto-narrow f64→f32 in let/assign/
  binop/compare/return); O(1) DJB2 hash tables for fn/struct/enum/alias lookup in
  semantic; SRET struct-return fix (stack alignment + hidden-arg counting). First release
  where `-O1` self-compile completes without a SIGSEGV. 50+ stdlib bugfixes (INT64_MIN,
  null/bounds checks, double-free detection, heap coalescing).
- **8 new stdlib modules** (15 total): `math`, `sort` (quicksort/heapsort/mergesort with
  fn-ptr comparators), `map` (splitmix64 / FNV-1a), `random` (xoshiro256\*\*), `net`
  (TCP/UDP), `arena`, `env`, `time`. All source files split to < 850 lines; zero warnings
  on self-compile.

---

## [v0.0.12](https://github.com/Caua726/Caustic/releases/tag/v0.0.12) — 2026-03-21

Modules and the C float ABI.

- **Module system** — `use "path" only name1, name2 as alias` selective imports and
  submodule dot-notation (`mod.submod.func()`, `mod.submod.VAR`). `caustic-mk depend`
  resolves git/system dependencies (`depend "name" in "url" tag "version"`, `system
  "lib"`, git clone).
- **Float ABI for `extern fn`** — f32/f64 args passed in xmm0–7, `cvtsd2ss` for f32
  params, `cvtss2sd` for f32 returns; the internal Caustic-to-Caustic f32 ABI works too
  (movd at load/store boundaries). `movzx` on 1/2-byte extern returns (fixes the SDL3
  `bool` ABI); 16-byte RSP alignment before extern calls (SSE requirement); `SET_CTX` no
  longer corrupts r10 on scalar assign from a call. Assembler gains `cvtsd2ss`/`cvtss2sd`/
  `movd`.

---

## [v0.0.11](https://github.com/Caua726/Caustic/releases/tag/v0.0.11) — 2026-03-20

Optimizer compile-time.

- **36% faster `-O1` compile** (self-compile 7.7s → 4.9s). O(1) lookup tables
  (`def_inst[vreg]`, `inst_at[pos]`, `call_prefix[pos]`) replace O(n) linked-list scans in
  strength reduction; `sr_process_loop` extracted from the main pass, dropping the vreg
  count 4902 → 4061 so graph coloring runs 2.9× faster on it. Faster mem2reg (O(1)
  offset→slot map + per-block phi index). Per-pass optimizer profiling under `--profile`.

---

## [v0.0.10](https://github.com/Caua726/Caustic/releases/tag/v0.0.10) — 2026-03-20

The biggest release of the alpha line — +6,266 lines. A complete `-O1` pipeline.

- **`-O1` optimizer** — constant + copy propagation (single-def guarded so loops don't get
  stale constants after phi destruction), store-load forwarding (invalidated at branches),
  immediate folding, LICM (multi-def guarded), **strength reduction v2** (detects
  `base + iv*stride*8` indexing chains and converts them to pointer increments, constant
  or variable strides), DCE (preserves phi back-edge copies), function inlining
  (< 20 IR insts, non-recursive), and a peephole pass integrated into the pipeline.
- **Graph-coloring register allocator v2** — a full rewrite using Iterative Register
  Coalescing (George & Appel): bit-matrix interference graph, George coalescing criterion,
  optimistic Briggs coloring, loop-depth-weighted spill costs. A 16-entry register cache
  tracks which physical register holds which spilled vreg (eliminates ~21% of redundant
  loads). Linear scan is preserved for `-O0` (zero overhead).
- i32 SSA promotion (addr-fusion moved before mem2reg), vreg-size tracking to stop copy
  propagation crossing i32/i64 boundaries, `[base+index*scale]` SIB addressing.
- **3.4× faster than `-O0`** (768ms vs 2592ms on the benchmark suite); competitive with
  GCC `-O0` (709ms), ahead of TinyCC (832ms). All optimizations gated behind `-O1`.

---

## [v0.0.9](https://github.com/Caua726/Caustic/releases/tag/v0.0.9) — 2026-03-18

- Codegen `r15` rewrite frees it for allocation (10 allocatable registers, dynamic scratch
  selection); SSA i32 promotion (all stack variables, including i32 loop counters,
  promoted after ADDR fusion); LICM for call-free loops; push/pop pruning (only save
  callee-saved registers actually used); `shl` for power-of-two array strides; `test
  r,r` instead of `cmp r,0`. 14 files refactored (no function over 100 lines); 8 bug fixes
  (SSA heap overflows, allocator caller-saved spill, OOB constant-fold guard).
- **Beats LuaJIT** on the suite (1060ms vs 1081ms); self-compile ~500ms.

---

## [v0.0.8](https://github.com/Caua726/Caustic/releases/tag/v0.0.8) — 2026-03-18

The SSA foundation.

- **SSA mem2reg** — promotes i64/pointer stack variables to virtual registers with phi
  nodes, dominance frontiers, and SSA destruction. Store-load forwarding eliminates
  redundant loads from stack slots within a basic block (i64, with correct i32
  invalidation). Register coalescing hints make the allocator prefer the same register for
  MOV-connected vregs, cutting phi-copy overhead.
- Assembler hash table expanded to 1024 entries covering all registers/instructions of
  length 2–9 (1.7× faster tokenization). 12 bug fixes (heap overflows in SSA/optimizer
  arrays, DFS stack overflow, caller-saved spill across calls, allocator alignment).
- Benchmark ~20% faster (1567ms → 1225ms).

---

## [v0.0.7](https://github.com/Caua726/Caustic/releases/tag/v0.0.7) — 2026-02-26

- Strength reduction — div/mod by a power of two uses shift/and instead of `idiv`;
  ADDR+STORE fusion eliminates the address vreg for direct `mov [rbp-N], val` stack writes;
  MUL-immediate fold; peephole reload elision (skip a redundant load when the register
  already holds the spilled value); a signed div/mod fast path with correct negative
  rounding (`sar` + correction).
- collatz N=10M: 6700ms → 4100ms (**39% faster**); spills 27 → 11; compiler `.text` −48 KB.

---

## [v0.0.6](https://github.com/Caua726/Caustic/releases/tag/v0.0.6) — 2026-02-26

The register allocator gets serious.

- Active-set tracking (O(7) lookup per allocation instead of an O(N) full scan); merge sort
  for live intervals (O(N log N) instead of O(N²) insertion sort); binary search in
  `spans_call` (O(log N)); spill-cost eviction (evict the lowest-cost interval, tie-broken
  by longest remaining range) instead of farthest-end; expire-before-allocate cleanup.
- **Self-hosting compile 3.01s → 0.49s (~6×)**; assembly output 5.59 MB → 4.42 MB (−21%);
  binary `.text` 1.05 MB → 785 KB (−25%); spills 87 → 32. gen2/gen3 identical (stable
  self-hosting).

---

## [v0.0.5](https://github.com/Caua726/Caustic/releases/tag/v0.0.5) — 2026-02-22

- `--profile` — pipeline breakdown, assembler sub-phase timing (tokenize/parse/pass1/pass2/
  elf), per-function codegen profiling with vreg count, and top codegen functions by time;
  plus compilation stats (source bytes, tokens, functions, IR insts, asm/object sizes).

---

## [v0.0.4](https://github.com/Caua726/Caustic/releases/tag/v0.0.4) — 2026-02-22

- **Interface files** (`.csti`) — `--emit-interface` generates minimal declaration files,
  auto-loaded when newer than the `.cst`. Binary pipeline caches (`--emit-tokens`/
  `--emit-ast`/`--emit-ir` serialize each stage); `--cache <dir>` skips lex/parse/
  semantic/IR on rebuild when the source is unchanged, integrated into `caustic-mk`.

---

## [v0.0.3](https://github.com/Caua726/Caustic/releases/tag/v0.0.3) — 2026-02-22

- **Dynamic heap** sized from source + imports (a pre-heap import scanner estimates the
  size via mmap, no allocation needed) instead of a fixed 128 MB; **multi-file
  compilation** (`caustic a.cst b.cst -o prog`) compiles each file with its own heap and
  links at the end; `--max-ram <MB>`; `mem.gheapreset(size)`; `linux.rename`/`mkdir`.

---

## [v0.0.2](https://github.com/Caua726/Caustic/releases/tag/v0.0.2) — 2026-02-22

- Install scripts (`caustic-mk install/uninstall`, `install.sh` curl script); the compiler
  resolves `std/` relative to the binary (`<bin>/../lib/caustic/`); `caustic-mk dist`
  builds a release tarball; `.file`/`.loc` DWARF directives for GDB source-level debugging;
  generic slices + cross-module generic instantiation; type aliases; semantic error
  reporting with position info; unused-variable warnings.

---

## [v0.0.1](https://github.com/Caua726/Caustic/releases/tag/v0.0.1) — 2026-02-21

- **First stable self-hosted release.** The compiler, assembler, and linker are fully
  written in Caustic and compile themselves stably; x86_64 Linux, straight to machine code,
  no libc dependency.
