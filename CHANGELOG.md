# Changelog

All notable changes to Caustic, newest first. Format follows
[Keep a Changelog](https://keepachangelog.com/). Each version links to its GitHub
release; full notes for recent versions live under [`docs/releases/`](docs/releases/).

Versioning: **`v1.x` = stable · `v0.1.x` = beta · `v0.0.x` = alpha.**

## [v0.1.0](https://github.com/Caua726/Caustic/releases/tag/v0.1.0) — 2026-07-22

### Added

- **WebAssembly backend** (`--target=wasm32-wasi` / `wasm64-wasi`): Emits complete `.wasm` binary; self-hosts to byte-identical compiler (gen1==gen2==gen3) under `node:wasi` and Deno.
- WebAssembly support for variadic functions (memory-packed varargs ABI).
- WebAssembly support for function pointers (`funcref` table + `call_indirect`).
- WebAssembly custom host imports (non-WASI `extern "<module>"` becomes wasm import).
- WebAssembly atomics support (single-threaded lowering for uncontended execution).
- WebAssembly `-O1`/`-O2` support (IR_LEA2/4/8 address folding + IR_CMOV).
- WebAssembly WASI OS stdlib: filesystem (path_*), heap (memory.grow), argv bootstrap, time, random, env.
- WebAssembly shared linear memory flag (prevents detach on memory.grow); 16 MiB chunk allocation.
- **Linux AArch64 backend** (`--target=linux-aarch64`): Static AArch64 ELF via scalar stack-home backend; supports atomics and threads.
- **-O2 optimization tier**: Global value numbering (cross-block CSE).
- **-O2 optimization tier**: Full loop unrolling for constant-trip loops.
- **-O2 optimization tier**: GVN extended to float ops.
- **SIMD auto-vectorizer (vec2)**: Reductions (i64/i32/f64).
- **SIMD auto-vectorizer (vec2)**: Elementwise maps (add/sub/mul/div).
- **SIMD auto-vectorizer (vec2)**: Broadcasts (`c[i]=a[i] OP k`).
- **SIMD auto-vectorizer (vec2)**: Runtime alias multiversioning.
- **SIMD auto-vectorizer (vec2)**: cpuid-dispatched AVX2 (256-bit) with SSE2 fallback + scalar remainder.
- **Language threading**: Atomic builtin `atomic_load`.
- **Language threading**: Atomic builtin `atomic_store`.
- **Language threading**: Atomic builtin `atomic_add`.
- **Language threading**: Atomic builtin `atomic_xchg`.
- **Language threading**: Atomic builtin `atomic_cas`.
- **Language threading**: Atomic `fence` builtin.
- **Language threading**: `with naked` functions.
- **Language threading**: `std/thread.cst` — 1:1 OS threads.
- **Language threading**: `std/sync.cst` primitive `Spinlock`.
- **Language threading**: `std/sync.cst` primitive `Mutex`.
- **Language threading**: `std/sync.cst` primitive `Cond`.
- **Language threading**: `std/sync.cst` primitive `RwLock`.
- **Language threading**: `std/sync.cst` primitive `Once`.
- **Language threading**: `std/sync.cst` primitive `WaitGroup`.
- **Language threading**: `std/sync.cst` primitive MPSC `Channel`.
- **Language threading**: thread-safe allocators.
- **Parallel-by-default compiler**: Fork per-module emission across all cores with dynamic work-stealing; ~2.1× speedup on self-host.
- **Dynamic stack growth**: Prologue stack probe eliminates `--stack-size` requirement for recursive programs.
- **Incremental compilation**: `--module-only` separate compilation.
- **Incremental compilation**: per-module `.csti` binary interfaces (types/signatures/imut-bodies/generics).
- **Incremental compilation**: `--module-objects` batch cold-build (4.8s → 1.37s).
- **Incremental compilation**: topological link-time DCE.
- **Incremental compilation**: `--emit-deps` module tracking.
- **Native self-host pre-commit gate** (`tools/precommit.sh`): Self-hosts 4-generation fixpoint at `-O0`, `-O1`, `-O2`; checks examples identical across levels; runs unit suite.
- **Release scripts**: `tools/prerelease.sh` — version bump enforcement.
- **Release scripts**: `tools/release-build.sh` — gen4 `-O2` fixpoint packaging.
- **LSP improvements**: Split `handlers.cst` into nav/complete/sig modules.
- **LSP improvements**: hover docs for atomics and target builtins.
- **LSP improvements**: completions for atomics and target builtins.
- CausticOS: framebuffer shell running as `/init`.
- CausticOS: `echo` userspace program.
- CausticOS: `cat` userspace program.
- CausticOS: `ls` userspace program.
- CausticOS: `uptime` userspace program.
- CausticOS: `sysinfo` userspace program.
- CausticOS: VGA 8×16 CP437 font.
- CausticOS: Tango color palette.
- CausticOS: ANSI/VT escape parser for in-terminal TUIs.
- CausticOS: terminal scrollback (grid ring).
- CausticOS: US Set-1 keyboard keymap with full punctuation.
- CausticOS syscall wrapper: `dev_open` / `present`.
- CausticOS syscall wrapper: `channel_create`.
- CausticOS syscall wrapper: `surface_info`.
- CausticOS syscall wrapper: `seg_create` / `seg_clone`.
- CausticOS syscall wrapper: `spawn_budget`.
- CausticOS syscall wrapper: `fd_send` / `fd_recv`.
- CausticOS syscall wrapper: `seg_read` / `seg_write`.
- CausticOS syscall wrapper: `time_now_ns` / `time_sleep_ns`.
- CausticOS syscall wrapper: `cpu_stat`.
- CausticOS syscall wrapper: `proc_list`.
- CausticOS syscall wrapper: `statfs`.

### Changed

- Replaced lossy IR cache with faithful AST cache (parser-layer serialization; generics opt-out).
- Register allocator changed to adjacency-list neighbor iteration (O(n²) → O(degree)).
- Struct copy (`rep movsb`) now recorded as clobber point in register allocation.
- SET_CTX instruction now recorded as clobber point in register allocation.
- Memory pool core refactored to reserve+commit-on-grow for sparse heap model.
- String global (.rodata) emission now filtered per module in separate compilation.
- Module emission deduped in `--module-objects` to prevent duplicate .o output.
- String `.000` emitter changed to prevent greedy-octal fusion of NUL with following octal digits.
- CLAUDE.md updated with WebAssembly backend docs — variadics.
- CLAUDE.md updated with WebAssembly backend docs — function pointers.
- CLAUDE.md updated with WebAssembly backend docs — custom imports.
- CLAUDE.md updated with WebAssembly backend docs — WASI filesystem.
- CLAUDE.md updated with WebAssembly backend docs — memory.
- CLAUDE.md updated with WebAssembly backend docs — argv.
- CLAUDE.md updated with WebAssembly backend docs — `-O1`/`-O2`.
- CLAUDE.md updated with WebAssembly backend docs — node:wasi verification.
- CausticOS stack-size computation now derives worst-case from call graph (user bounds via `--stack-size` only for recursion/indirect calls).
- Assembler/linker/maker submodules reorganized into internal folder structure (`core/lexer/parser`, etc).

### Performance

- Self-host peak RAM **−74%** (360 MB → 92 MB): Transient body-AST freed after IR-gen.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Generic-instance clones in transient arena.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Narrowed `Node` fields.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Narrowed `IRInst` fields.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Narrowed `Variable` fields.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Lazy side-struct for decl-only fields.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Free imported-module tokens after semantic.
- Self-host peak RAM **−74%** (360 MB → 92 MB): Arena on-demand growth.
- Register allocator per-generation build **−23%** (1.27s → 0.97s): Vreg compaction.
- Register allocator per-generation build **−23%** (1.27s → 0.97s): Adjacency-list neighbor iteration.
- Compile I/O **−78%** (32 MB → 7 MB): In-memory `.s` pipeline (codegen→peephole→assembler, no disk round-trip).
- Compile I/O **−78%** (32 MB → 7 MB): `--no-asm-cache` skips debug-cache write.
- Incremental clean build **−64%** (4.8s → 1.37s): `--module-objects` batch cold-build (whole-program frontend once, codegen+assemble per module).
- Incremental dev-loop **~247ms** per module (vs ~746ms full build).
- Vectorizer reduction/map/broadcast emission reduced via tighter LAA memory-dependence test.
- Vectorizer reduction/map/broadcast emission reduced via AVX2 dispatch (cpuid runtime selection).

### Fixed

- Octal escapes (`\NNN`) were silently corrupted (>= 64 unreachable); lexer now handles 1–3 digit octal.
- `.string` emitter fusion bug: NUL byte + following octal digit (`\0` + `'5'` → `\05`) dropped a byte.
- Register allocation clobber point: SET_CTX no longer loses live values in caller-saved registers.
- Register allocation clobber point: struct copy no longer loses live values in caller-saved registers.
- **-O2 GVN miscompile** (silent failures): GVN was CSEing comparison results across blocks; isel's cmp-into-branch fusion never materialized the boolean, so CSE'd consumers read garbage. Restricted GVN to arithmetic/address ops.
- Windows PE test import path (`tests/windows/exit42.cst`: pe_writer moved to `caustic-linker/pe/pe_writer.cst`).
- IR cache SIGSEGV on hit (pool uninitialized); cache disabled and replaced with AST cache.
- IR cache lossy serialization of cast_to/cast_from types; cache disabled and replaced with AST cache.
- IR cache lossy serialization of function metadata; cache disabled and replaced with AST cache.
- Node:wasi crash on memory.grow (unshared memory detached ArrayBuffer): Emit linear memory with shared flag + 2 GiB max.
- Unreachable blocks caused vec2 dom_blk segfault; CFG construction now gates dom queries on block reachability.
- CausticOS page allocator: `mem/pool` reserved full heap on creation; CausticOS (strict reserve/commit, no demand paging) OOM'd on null region. Now commits on-demand via `SYS_COMMIT`.
- CausticOS self-host startup: Register argv[0] via `io.set_self_path()` so stdlib discovery works (kernel has no /proc/self/exe).
- Incremental build segfault (duplicate module emission): Re-import of a module via plain + `only` created duplicate Module records; mo_count now dedupes by prefix.

### Removed

- **vec1 vectorizer** (opt_vec.cst) retired; vec2 is the only auto-vectorizer.
- Pure-JS WASI shim (tests/wasm/wasi_run.mjs) — node:wasi is stable; compiler compiles under standard `node:wasi` and Deno.
- Lossy IR cache (lossy serialization replaced with AST cache).

## [v0.0.17](https://github.com/Caua726/Caustic/releases/tag/v0.0.17) — 2026-05-30

### Added

- **Shared libraries** — `--shared` builds ELF `.so` (System V ABI, callable from C/Rust).
- **Shared libraries** — `--shared` builds PE `.dll`.
- **Shared libraries** — `--shared` builds `.csl` (universal cross-OS format, no runtime detection).
- **Dynamic stdlib** — `-lcaustic` links stdlib from libcaustic.so instead of bundling; functions externalized at codegen, resolved at runtime.
- **Cross-OS `.csl` format** — single 242KB libcaustic.csl runs on Linux/Windows/CausticOS; loader writes host OS once, dispatches via `os.current()` without polyglot bodies.
- **`.csl` loader** (`std/csl_loader.cst`) — mmap/VirtualAlloc segments RWX; proven under wine.
- **`.csl` loader** (`std/csl_loader.cst`) — resolves exports; proven under wine.
- **CSE target** — `--target=caustic-x86_64` generates Caustic Standard Executable, pure mode (folds `os.current()` to literal).
- **CSE target** — `--target=caustic-x86_64` generates Caustic Standard Executable, bundle mode (folds `os.current()` to literal).
- **CSE target** — `--target=caustic-x86_64` generates Caustic Standard Executable, compat mode (dispatches `os.current()` at runtime).
- **Tiny dynamic `.cse`** — CST_ v3 format (import table for Windows kernel32, no embedded loader), hello-world 451 bytes.
- **Installer script** (`install.sh`) — default `curl | sh` install, per-user into `~/.local` (no sudo).
- **Installer script** (`install.sh`) — `--custom` interactive mode.
- **Installer script** (`install.sh`) — `--system` mode, installs into /usr/local.
- **Installer script** (`install.sh`) — ships toolchain.
- **Installer script** (`install.sh`) — ships stdlib source.
- **Installer script** (`install.sh`) — ships `libcaustic.so`.
- **Installer script** (`install.sh`) — ships `libcaustic.csl`.
- **OS_CAUSTIC kernel target** — `__target_is_caustic` builtin.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `write`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `exit`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `getpid`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `yield`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `sleep_ns`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `time_ns`.
- **OS_CAUSTIC kernel target** — `std/os/causticos.cst` kernel syscall: `kern_info`.
- **OS_CAUSTIC kernel target** — `__compile_error` builtin for build-time assertions.
- **Compile-time error gate** — `__compile_error("msg")` aborts build if reachable (after DCE).
- **Compile-time error gate** — `__allow_unsupported` opt-out flag.
- **std/errors.cst** — compile-time helper macro `unsupported`.
- **std/errors.cst** — compile-time helper macro `no_vfs`.
- **std/errors.cst** — compile-time helper macro `todo`.
- **std/errors.cst** — runtime `panic`.
- **std/errors.cst** — runtime `assert`.
- **std/libcaustic.cst** — meta-module for building shared library distributions.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against GCC.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Clang.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against TCC.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Rust.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Zig.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Go.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against C++.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Java.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against C#.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Node.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Bun.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against Deno.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — self-hosted 13-language harness comparing against LuaJIT.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — cross-language comparison measures compile time.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — cross-language comparison measures binary size.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — cross-language comparison measures runtime perf.
- **Benchmark suite** (`benchmark.cst` + `benchmark.md`) — cross-language comparison measures RAM.
- **CLBG examples** — `_clbg_fannkuch.cst` (Computer Language Benchmarks Game suite).
- **CLBG examples** — `_clbg_nbody.cst` (Computer Language Benchmarks Game suite).
- **CLBG examples** — `_clbg_mandelbrot.cst` (Computer Language Benchmarks Game suite).
- **Local CSE pass** (`opt_cse.cst`) — eliminates redundant pure arithmetic within basic blocks, keyed on (op, src1, src2).
- **Loop-invariant address hoisting** (`pass_addr_hoist`) — pulls `IR_ADDR(IMM)` out of loop bodies.
- **Loop-invariant address hoisting** (`pass_addr_hoist`) — pulls `IR_ADDR_GLOBAL` out of loop bodies.
- **IR_LEA2 opcode** — fused base+idx*scale x86 LEA (replaces mov+shl+add chain).
- **IR_LEA4 opcode** — fused base+idx*scale x86 LEA (replaces mov+shl+add chain).
- **IR_LEA8 opcode** — fused base+idx*scale x86 LEA (replaces mov+shl+add chain).
- IR_LEA fusion — 6% faster on fannkuch.
- IR_LEA fusion — 3% faster on nbody.
- **F64 XMM regalloc** — dedicated VR_XMM class (14-color xmm2..xmm15 pool).
- **F64 XMM regalloc** — f64 LOADs classified as XMM.
- **F64 XMM regalloc** — load/store via movsd.
- **sqrt builtin** — `__builtin_sqrt(x)` → IR_FSQRT → sqrtsd.
- **sqrt builtin** — `std/math.cst` exposes `sqrt`.
- **sqrt builtin** — `std/math.cst` exposes `sqrtf`.
- **Magic divmod-by-const** — Granlund-Montgomery signed div/mod (30 cycles → 5 cycles, divmod microbench 126ms → 75ms vs GCC -O0 60ms).
- **IR strength-reduction Pattern 5** — arr[iv*K] indexing (arrays of structs) with non-zero IV init detection.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `ADD` by 0 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `SUB` by 0 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `OR` by 0 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `XOR` by 0 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `MUL` by 1 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `SHL` by 0 marked dead, routed through copy_of.
- **Algebraic identity stripping** (`pass_algebraic_identity`) — `SHR` by 0 marked dead, routed through copy_of.
- **IMUL 3-op form** — `imul reg, imm` (self-form) vs mov+imul bounce.
- **IMUL 3-op form** — power-of-2 multiplies collapse to shl.
- **NK_ASSIGN STORE[imm] optimization** — local assignments emit STORE [imm_offset] direct, enables loop-carried scalar mem2reg promotion.
- **CSE header app-version** — optional free-form app version string at 0x1C, set via `--app-version` / Causticfile.
- **Test suite in Caustic** (`tests/run_tests.cst`) — replaced bash runner; spawns toolchain via std/process, verifies bootstrap fixpoint (gen2==gen3 byte-identical).

### Changed

- **Compile RAM** — 163MB → 12MB (-93%, -O1 self-host) via lazy pool allocator (slots by bump index, not pre-linked freelist).
- **Array-of-struct sizing** — resolve_type_ref now recomputes array size after resolving element type; was leaving size 0 for named-reference elements.
- **os.current conversion** — from `let with imut` global to `fn current() as i32 with imut`; folds to literal in pure/bundle modes, runtime dispatch in compat.
- **108 os.current() call sites** — updated across 10 stdlib files to invoke as function.
- **Compat dual-backend imports** — conditional imports fire under `__target_is_X + __compat_mode` to pull both OS backends for runtime dispatch.
- **IR cache keyed by cse_mode** — separate caches for pure/compat/bundle modes; token cache mode-independent.
- **Linker submodule split** — `pe_debug.cst` extracted from monolith.
- **Linker submodule split** — `link_dynamic.cst` extracted from monolith.
- **Repository structure** — fold `caustic-shared/` into `src/debug_proto.cst`.
- **Repository structure** — move `tests_windows/` to `tests/windows/`.
- **Repository structure** — fold `tests/run_tests.sh` → `tests/run_tests.cst` (Caustic).
- **Build artifacts** — write to ./build (gitignored) instead of /tmp/caustic-* (fixes TOCTOU on multi-user hosts).
- **Bootstrap test** — inlined in Causticfile ('caustic-mk bootstrap', in-process + cmp), dropped test_bootstrap.sh.
- **Binaries gitignored** — toolchain binaries (caustic, caustic-as/ld/lsp/mk) untracked; releases ship via 'caustic-mk dist'.
- **README rewrite** — documents multi-target Linux ELF.
- **README rewrite** — documents multi-target Windows PE.
- **README rewrite** — documents multi-target CausticOS CSE.
- **README rewrite** — documents cross-compilation.
- **README rewrite** — documents portable stdlib facade `io`.
- **README rewrite** — documents portable stdlib facade `mem`.
- **README rewrite** — documents portable stdlib facade `string`.
- **README rewrite** — documents portable stdlib facade `os`.
- **README rewrite** — documents portable stdlib facade `net`.
- **README rewrite** — documents portable stdlib facade `process`.
- **README rewrite** — documents portable stdlib facade `term`.
- **README rewrite** — documents portable stdlib facade `env`.
- **README rewrite** — documents portable stdlib facade `errors`.
- **Examples cleanup** — drop direct `use os/linux.cst` imports in favor of portable facades.
- **Examples cleanup** — rich calculator: operator precedence.
- **Examples cleanup** — rich calculator: functions.
- **Examples cleanup** — rich calculator: ANSI colors.
- **Examples cleanup** — rich calculator: error carets.
- **Examples cleanup** — `math.cst` adds `isqrt`.

### Performance

- **Bootstrap** — 825ms → 568ms (-31%, -O1 self-host gen4).
- **Compile peak RAM** — 163MB → 12MB (-93%) via lazy pool (proportional to actual allocations, not pre-committed).
- **nbody** — 1.74s → 0.21s (-88%, -O1) via f64 XMM regalloc + IR optimizations; matches GCC -O2 within 1 ulp.
- **mandelbrot** — 67ms → 54ms (-19%, -O1).
- **fannkuch** — 4.27s → ~3.65s (-14% cumulative) via LEA fusion.
- **fannkuch** — 4.27s → ~3.65s (-14% cumulative) via addr hoist.
- **fannkuch** — 4.27s → ~3.65s (-14% cumulative) via CSE.
- **fannkuch** — 4.27s → ~3.65s (-14% cumulative) via store coalescing.
- **Regalloc** — O(n²) matrix → O(n) adjacency-list via vreg compaction, per-gen 1.27s → 0.97s.
- **Regalloc** — O(n²) matrix → O(n) adjacency-list via adjacency-list iteration, per-gen 1.27s → 0.97s.
- **Divmod-by-const** — Granlund-Montgomery magic (30 cycles → 5 cycles); divmod microbench 126ms → 75ms.
- **mark_reachable** — O(F+G) name scan → DJB2 hash on per-program index.
- **lookup_var** — lazy DJB2 hash on global scopes (local scopes stay linked-list).
- **register_alloc** — fused call-position + interval scan (3 pass-overs → 2).

### Fixed

- **Array-of-struct global aliasing** — resolve_type_ref recursed into element type but never recomputed array size; 0-byte arrays got same .bss address, writes clobbered each other (CausticOS ACPI MADT tables, compiler's own globals diverged from -O1 fixpoint).
- **Assembler `.byte` incomplete** — was emitting only first comma-separated value; `.byte 0x8F, 0x07` dropped 0x07, misaligned hand-written opcodes into invalid instructions (#UD → triple fault on freestanding).
- **Assembler `.incbin` missing** — directive was unparsed, emitted nothing; kernel driver-spec blobs registered zero drivers.
- **F64 regalloc bypass** — LOAD with f64 dest stayed VR_GPR, skipped XMM fast path, emitted mov gpr,[mem]; movq xmm,gpr (GPR round-trip per f64 load).
- **F64 comparison** — codegen emitted cmp gpr,gpr + signed jumps (incorrect for IEEE); now emits ucomisd xmm,xmm + CF-based jumps (jb/jae).
- **with imut f64 globals** — try_fold_const read n.val (integer half) not n.fval (float half); float literals refused to fold, PI/SOLAR_MASS propagated as zero.
- **Global const-init cast** — eval_const_expr rejected `cast` as non-constant (wrongly); re-added recursion on operands.
- **Global const-init shift** — eval_const_expr rejected `<<`/`>>` as non-constant (wrongly); re-added recursion on operands.
- **Global const-init bitwise** — eval_const_expr rejected `&`/`|`/`^` as non-constant (wrongly); re-added recursion on operands.
- **Global const-init mod** — eval_const_expr rejected `%` as non-constant (wrongly); re-added recursion on operands.
- **-O1 codegen miscompile** — `pass_cse_local` remapped CSE'd vregs only on src1/src2, not dest operand (garbage register for constant-0 array indices).
- **-O1 codegen miscompile** — IRInst -O1 pool 8x undersized (inline pass held all functions' IR, exceeded capacity, returned null).
- **-O1 codegen miscompile** — mov-dedup peephole reset only on `out_char('\n')`, not `out_str`, allowed dedup across jump targets.
- **Temporary file TOCTOU** — test/bootstrap scripts wrote to /tmp/caustic-* (symlink vector on multi-user hosts); now write to repo-local ./build.
- **Module-qualified imut call folding** — os.current() (NK_MEMBER lhs) never folded because walk_fncall only recovered callee from NK_IDENT; fixed via try_fold_imut_fn on resolved function (facade dispatch restored).
- **Cross-target --shared builds** — `os.linux` called syscall directly; under --shared all globals kept, syscalls reached codegen + errored on Windows/other targets; now gated on `__target_is_linux` so wrong-OS syscalls dead-strip.
- **Cross-target --shared builds** — `os.causticos` called syscall directly; under --shared all globals kept, syscalls reached codegen + errored on Windows/other targets; now gated on `__target_is_caustic` so wrong-OS syscalls dead-strip.
- **Linux-only dispatch pattern** — `if (os.current == os.OS_WINDOWS) { return -1; } linux.open(...)` never compiled cross-target (semantic DCE only on if (NK_NUM)); rewritten as `if (os.current == os.OS_LINUX) { linux.open(...); }` in io.cst's `list_dir`.
- **Linux-only dispatch pattern** — same rewrite applied to io.cst's `count_dir_entries`.
- **Linux-only dispatch pattern** — same rewrite applied to io.cst's `is_dir`.
- **Linux-only dispatch pattern** — same rewrite applied to io.cst's `get_cwd`.
- **Linux-only dispatch pattern** — same rewrite applied to io.cst's `process._spawn_win`.
- **Direct-from-register fast paths reverted** — three codegen optimizations (IR_LEA/load/store naming allocated registers in operands) were buggy in combination; matmul segfaulted, gen2 != gen3; reverted fast paths, kept IR-level wins (fusion, CSE, addr_hoist).

### Removed

- **Scratch documentation** — `docs/specs/` (planning notes, TODO lists) deleted.
- **Scratch documentation** — `docs/superpowers/` (planning notes, TODO lists) deleted.
- **Committed binaries** — `caustic` untracked + gitignored (releases ship via dist).
- **Committed binaries** — `caustic-as` untracked + gitignored (releases ship via dist).
- **Committed binaries** — `caustic-ld` untracked + gitignored (releases ship via dist).
- **Committed binaries** — `caustic-lsp` untracked + gitignored (releases ship via dist).
- **Committed binaries** — `caustic-mk` untracked + gitignored (releases ship via dist).
- **Committed binaries** — `caustic-x86_64-linux.tar.gz` untracked + gitignored (releases ship via dist).
- **test_bootstrap.sh** — bootstrap test moved inline to Causticfile ('caustic-mk bootstrap').
- **run_tests.sh** — 506-line bash test runner replaced by tests/run_tests.cst (Caustic).

## [v0.0.16](https://github.com/Caua726/Caustic/releases/tag/v0.0.16) — 2026-05-28

### Added

- Windows x86_64 native target (`--target=windows-x86_64`) produces PE/COFF executables.
- PE/COFF writer and object file infrastructure for Windows cross-compilation.
- `extern "dll.dll" fn` syntax for Windows DLL imports — `kernel32`.
- `extern "dll.dll" fn` syntax for Windows DLL imports — `ws2_32`.
- `extern "dll.dll" fn` syntax for Windows DLL imports — `bcrypt`.
- Windows binding (`std/os/windows.cst`): `accept`.
- Windows binding (`std/os/windows.cst`): `BCryptGenRandom`.
- Windows binding (`std/os/windows.cst`): `bind`.
- Windows binding (`std/os/windows.cst`): `CloseHandle`.
- Windows binding (`std/os/windows.cst`): `closesocket`.
- Windows binding (`std/os/windows.cst`): `connect`.
- Windows binding (`std/os/windows.cst`): `CreateDirectoryA`.
- Windows binding (`std/os/windows.cst`): `CreateFileA`.
- Windows binding (`std/os/windows.cst`): `CreatePipe`.
- Windows binding (`std/os/windows.cst`): `CreateProcessA`.
- Windows binding (`std/os/windows.cst`): `CreateThread`.
- Windows binding (`std/os/windows.cst`): `DeleteFileA`.
- Windows binding (`std/os/windows.cst`): `ExitProcess`.
- Windows binding (`std/os/windows.cst`): `ExitThread`.
- Windows binding (`std/os/windows.cst`): `FindClose`.
- Windows binding (`std/os/windows.cst`): `FindFirstFileA`.
- Windows binding (`std/os/windows.cst`): `FindNextFileA`.
- Windows binding (`std/os/windows.cst`): `GetCommandLineA`.
- Windows binding (`std/os/windows.cst`): `GetConsoleMode`.
- Windows binding (`std/os/windows.cst`): `GetConsoleScreenBufferInfo`.
- Windows binding (`std/os/windows.cst`): `GetCurrentProcessId`.
- Windows binding (`std/os/windows.cst`): `GetCurrentThreadId`.
- Windows binding (`std/os/windows.cst`): `GetEnvironmentVariableA`.
- Windows binding (`std/os/windows.cst`): `GetExitCodeProcess`.
- Windows binding (`std/os/windows.cst`): `GetExitCodeThread`.
- Windows binding (`std/os/windows.cst`): `GetFileAttributesExA`.
- Windows binding (`std/os/windows.cst`): `GetFileSizeEx`.
- Windows binding (`std/os/windows.cst`): `GetLastError`.
- Windows binding (`std/os/windows.cst`): `GetModuleFileNameA`.
- Windows binding (`std/os/windows.cst`): `getsockopt`.
- Windows binding (`std/os/windows.cst`): `GetStdHandle`.
- Windows binding (`std/os/windows.cst`): `GetSystemTimeAsFileTime`.
- Windows binding (`std/os/windows.cst`): `htonl`.
- Windows binding (`std/os/windows.cst`): `htons`.
- Windows binding (`std/os/windows.cst`): `ioctlsocket`.
- Windows binding (`std/os/windows.cst`): `listen`.
- Windows binding (`std/os/windows.cst`): `MoveFileA`.
- Windows binding (`std/os/windows.cst`): `MoveFileExA`.
- Windows binding (`std/os/windows.cst`): `ntohl`.
- Windows binding (`std/os/windows.cst`): `ntohs`.
- Windows binding (`std/os/windows.cst`): `QueryPerformanceCounter`.
- Windows binding (`std/os/windows.cst`): `QueryPerformanceFrequency`.
- Windows binding (`std/os/windows.cst`): `ReadFile`.
- Windows binding (`std/os/windows.cst`): `recv`.
- Windows binding (`std/os/windows.cst`): `RemoveDirectoryA`.
- Windows binding (`std/os/windows.cst`): `RtlWaitOnAddress`.
- Windows binding (`std/os/windows.cst`): `RtlWakeAddressAll`.
- Windows binding (`std/os/windows.cst`): `RtlWakeAddressSingle`.
- Windows binding (`std/os/windows.cst`): `send`.
- Windows binding (`std/os/windows.cst`): `SetConsoleMode`.
- Windows binding (`std/os/windows.cst`): `SetFilePointerEx`.
- Windows binding (`std/os/windows.cst`): `SetHandleInformation`.
- Windows binding (`std/os/windows.cst`): `setsockopt`.
- Windows binding (`std/os/windows.cst`): `Sleep`.
- Windows binding (`std/os/windows.cst`): `SwitchToThread`.
- Windows binding (`std/os/windows.cst`): `VirtualAlloc`.
- Windows binding (`std/os/windows.cst`): `VirtualFree`.
- Windows binding (`std/os/windows.cst`): `VirtualProtect`.
- Windows binding (`std/os/windows.cst`): `WaitForSingleObject`.
- Windows binding (`std/os/windows.cst`): `WriteFile`.
- Windows binding (`std/os/windows.cst`): `WSACleanup`.
- Windows binding (`std/os/windows.cst`): `WSAGetLastError`.
- Windows binding (`std/os/windows.cst`): `WSAPoll`.
- Windows binding (`std/os/windows.cst`): `WSASocketA`.
- Windows binding (`std/os/windows.cst`): `WSAStartup`.
- `<TARGET>` placeholder expansion in module paths for per-OS stdlib dispatch.
- Portable stdlib facade `io` — works cross-platform via `os.current` routing.
- Portable stdlib facade `mem` — works cross-platform via `os.current` routing.
- Portable stdlib facade `net` — works cross-platform via `os.current` routing.
- Portable stdlib facade `time` — works cross-platform via `os.current` routing.
- Portable stdlib facade `env` — works cross-platform via `os.current` routing.
- Portable stdlib facade `random` — works cross-platform via `os.current` routing.
- Portable stdlib facade `term` — works cross-platform via `os.current` routing.
- Portable stdlib facade `process` — works cross-platform via `os.current` routing.
- Per-OS stdlib implementation — `std/io/linux.cst` / `std/io/windows.cst`.
- Per-OS stdlib implementation — `std/time/linux.cst` / `std/time/windows.cst`.
- Per-OS stdlib implementation — `std/mem/linux.cst` / `std/mem/windows.cst`.
- Per-OS stdlib implementation — `std/net/linux.cst` / `std/net/windows.cst`.
- Per-OS stdlib implementation — `std/env/linux.cst` / `std/env/windows.cst`.
- Per-OS stdlib implementation — `std/random/linux.cst` / `std/random/windows.cst`.
- `with imut` on functions (Layer 2 comptime) — tree-walking interpreter folds calls with literal-only args to `NK_NUM` at semantic time.
- Conditional imports: `if (cond) use "..." as alias;` gated on parser builtins (`__target_is_linux`, `__target_is_windows`).
- IR_FFI_CALL opcode for foreign function interface with ABI translation.
- Microsoft x64 ABI code generation (rdi/rsi/rdx/rcx → rcx/rdx/r8/r9, 32-byte shadow space).
- CodeView debug info — `.debug$S` section for Windows binaries.
- CodeView debug info — `.debug$T` section for Windows binaries.
- `.pdb` sidecar for Windows binaries.
- Struct literal initialization syntax — `StructName { field = value, ... }` for global and local variables.
- Function pointers in global initializers via `fn_ptr()` with R_X86_64_64 relocations.
- Custom ELF section support — `with section(".name")` on globals and functions.
- `__start_X` / `__stop_X` boundary symbols for custom-section auto-discovery.
- caustic-mk ported to Windows via portable stdlib.
- LSP server (`caustic-lsp`) — hover.
- LSP server (`caustic-lsp`) — go-to-definition.
- LSP server (`caustic-lsp`) — completion.
- LSP server (`caustic-lsp`) — references.
- LSP server (`caustic-lsp`) — rename.
- LSP server (`caustic-lsp`) — diagnostics.
- Rich stdlib documentation (13 modules with signatures, params, return values, examples).
- Comprehensive language documentation — `getting-started.md`.
- Comprehensive language documentation — `tutorial.md`.
- Comprehensive language documentation — `language.md`.
- Comprehensive language documentation — 505+ lines combined across `getting-started.md`, `tutorial.md`, `language.md`.
- Full test suite (`tests/run_tests.sh`) — unit tests.
- Full test suite (`tests/run_tests.sh`) — 4-gen bootstrap.
- Full test suite (`tests/run_tests.sh`) — assembler validation.
- Full test suite (`tests/run_tests.sh`) — linker validation.
- CONTRIBUTING.md guide with development workflow.

### Changed

- Literals with unary minus now fold to signed `NK_NUM` at parse time (INT32_MIN / INT64_MIN expressible without cast).
- Semantic walk expanded with definite-assignment tracking — uninitialized scalar reads error ("uso de variavel nao inicializada").
- Implicit conversion unified via `try_implicit_convert()` — single entry point for all type compatibility checks.
- Target dispatch infrastructure — `--target` flag accepted by `caustic`.
- Target dispatch infrastructure — `--target` flag accepted by `caustic-as`.
- Target dispatch infrastructure — `--target` flag accepted by `caustic-ld`.
- Parser recognizes both `extern fn` (existing) and `extern "dll.dll" fn` (new) syntax.
- Module resolution attempts `<TARGET>` placeholder substitution before path lookup.
- Literal narrowing now refuses cross-domain (float↔int) conversions without explicit cast.
- Functions missing return in some code paths now error ("funcao nao retorna em todos os paths").
- Defer blocks inside `if` now execute at block exit (LIFO per iteration).
- Defer blocks inside `while` now execute at block exit (LIFO per iteration).
- Defer blocks inside `for` now execute at block exit (LIFO per iteration).
- Defer blocks inside `do-while` now execute at block exit (LIFO per iteration).
- Defer blocks inside `match` now execute at block exit (LIFO per iteration).
- Narrow integer operations on `u8` emit zero-extend after arithmetic to maintain wrap semantics.
- Narrow integer operations on `u16` emit zero-extend after arithmetic to maintain wrap semantics.
- Narrow integer operations on `u32` emit zero-extend after arithmetic to maintain wrap semantics.
- Narrow integer operations on `i8` emit sign-extend after arithmetic to maintain wrap semantics.
- Narrow integer operations on `i16` emit sign-extend after arithmetic to maintain wrap semantics.
- Narrow integer operations on `i32` emit sign-extend after arithmetic to maintain wrap semantics.
- Float cast chain (f32→f64→f32) now applies precision-loss roundtrip correctly.
- Constant out-of-bounds array indexing detected at compile time.
- u64 literal narrowing accepts full 2^64 bit patterns (including negatives reinterpreted as u64 max).
- Pointer-to-pointer implicit conversion requires matching base type or explicit cast (stricter type safety).
- All examples updated for new syntax and cross-platform features.
- Submodule version bumped: caustic-assembler — coff.cst stubs.
- Submodule version bumped: caustic-assembler — .cstimport support.
- Submodule version bumped: caustic-linker — PE writer.
- Submodule version bumped: caustic-linker — COFF reader.

### Performance

- Compiler self-binary size reduced 17% (1.22 MB → 1.01 MB) via improved function-level DCE for imported modules.

### Fixed

- Fixed DCE incorrectly marking all functions in imported modules as reachable.
- Fixed bootstrap regression where self-referential pointer types (`*Node`) rejected implicit conversion.
- Fixed silent float-as-int miscompile when float literals assigned to integer variables.
- Fixed struct literal initialization in local variable declarations (was only working for globals).
- Fixed negative literal narrowing (e.g., `let is i32 as x = -100;` now works).
- Fixed float cast convention violation — f32↔f64 roundtrips now preserve precision semantics.
- Fixed uninitialized local variables producing garbage instead of error.
- Fixed integer wrap semantics — operations on narrow widths now zero/sign-extend to canonical form.
- Fixed implicit pointer-to-pointer conversion accepting incompatible base types.
- Fixed constant array index bounds checking.

### Removed

- Removed stale documentation note claiming cross-module struct field embedding unsupported (feature already works).

## [v0.0.15](https://github.com/Caua726/Caustic/releases/tag/v0.0.15) — 2026-03-31

### Added
- **caustic-lsp** — a Language Server Protocol implementation for Caustic.
- LSP: real-time diagnostics (errors and warnings from the compiler).
- LSP: hover with full function signatures (parameters + doc comments).
- LSP: hover docs for every keyword (`use`, `fn`, `let`, `struct`, `defer`, `cast`, …).
- LSP: hover docs for stdlib symbols (from `docs/stdlib/`).
- LSP: go-to-definition for fn/struct/enum/type/variable.
- LSP: completion of symbols, keywords, and module members after `mod.`.
- LSP: find-references across the workspace.
- LSP: rename across all occurrences.
- `-debugparser` flag — dumps the full AST tree after parsing.
- Documentation `docs/stdlib/io.md` — buffered I/O, printf, file operations.
- Documentation `docs/stdlib/mem.md` — memory allocators.
- Documentation `docs/stdlib/string.md` — dynamic strings.
- Documentation `docs/stdlib/slice.md` — generic dynamic array.
- Documentation `docs/stdlib/map.md` — hash maps.
- Documentation `docs/stdlib/math.md` — math helpers.
- Documentation `docs/stdlib/sort.md` — sorting algorithms.
- Documentation `docs/stdlib/random.md` — PRNG.
- Documentation `docs/stdlib/net.md` — TCP/UDP sockets.
- Documentation `docs/stdlib/time.md` — clocks and sleep.
- Documentation `docs/stdlib/env.md` — argc/argv and getenv.
- Documentation `docs/stdlib/types.md` — Option and Result.
- Documentation `docs/stdlib/linux.md` — syscall wrappers.

### Changed
- Assembler submodule updated — performance.
- Assembler submodule updated — version generation.
- Linker submodule updated — performance.
- Linker submodule updated — version generation.
- Maker submodule updated — performance.
- Maker submodule updated — version generation.

## [v0.0.14](https://github.com/Caua726/Caustic/releases/tag/v0.0.14) — 2026-03-31

### Added
- `\xNN` hex escape sequences in string and char literals
- `null` keyword — typed null pointer literal for any `*T`
- Global string constants with `with imut` qualifier
- CLI `--version` flag (shows target, author, repo)
- CLI `--help` flag with full flag documentation
- CLI `-q` / `--quiet` flag to suppress status output
- CLI unknown flag detection (errors instead of silently accepting input)
- **bins allocator** — O(1) slab allocation with bitmap double-free detection
- **mem submodule** — `freelist` allocator (submodule architecture)
- **mem submodule** — `bins` allocator (submodule architecture)
- **mem submodule** — `arena` allocator (submodule architecture)
- **mem submodule** — `pool` allocator (submodule architecture)
- **mem submodule** — `lifo` allocator (submodule architecture)
- **std/path.cst** — `join`
- **std/path.cst** — `join3`
- **std/path.cst** — `dirname`
- **std/path.cst** — `basename`
- **std/path.cst** — `ext`
- **std/path.cst** — `expand_home`
- **std/process.cst** — `run`
- **std/process.cst** — `spawn`
- **std/process.cst** — `spawnp`
- **std/process.cst** — `wait_pid`
- **std/process.cst** — `capture`
- **std/term.cst** — raw mode
- **std/term.cst** — ANSI escapes
- **std/term.cst** — `get_size`
- **std/term.cst** — `read_key`
- **string.cst** — `streq`
- **string.cst** — `cstr_starts_with`
- **string.cst** — `cstr_len`
- **string.cst** — `StringBuilder`
- **io.cst** — `sprintf` for buffer formatting
- **io.cst** — `printf` width/alignment flags
- **linux.cst** — `PERM_755` constant
- **linux.cst** — `PERM_644` constant
- **linux.cst** — `PERM_700` constant
- **linux.cst** — `PERM_600` constant
- **linux.cst** — signal wrappers
- **linux.cst** — `WNOHANG`
- **net.cst** — `htons`
- **net.cst** — `ip_parse`
- **net.cst** — `dial(ip, port)`
- **env.cst** — `envp()`
- **env.cst** — `execvp(file, argv)`
- Assembler: octal escape `\NNN` in `.string` directives
- Assembler: support for 6 relocation types (was 3)

### Changed
- Compiler stdlib resolution fallback to `/usr/local/lib/caustic`
- `asm()` now processes escape sequences: `\n` works correctly
- `asm()` now processes escape sequences: `\t` works correctly
- `asm()` now processes escape sequences: `\x1b` works correctly
- Parser: support `mod.sub.Type` in type references (multi-level dot chains)
- Semantic: `resolve_type_ref` follows submodule chains (`mem.bins.Bins` resolves)
- **mem.cst API cleanup** — removed galloc/gfree wrappers; users call bins directly
- **Inliner** — leaf-only leaf functions
- **Inliner** — per-callee limit of 4 large functions
- **Inliner** — per-callee limit of 8 small functions
- **Inliner** — 60 inst max
- **Inliner** — 3-phase architecture (optimize → inline → cleanup)
- **IR codegen** — per-function pipeline with immediate instruction free after emit
- **IR allocation** — migrated from freelist to pool (262k slots)
- **IR allocation** — migrated to bins
- **Full stdlib bins migration** — all 15 modules use O(1) bins + auto-init
- **Full freelist elimination** — from compiler
- **Full freelist elimination** — from assembler
- **Full freelist elimination** — from linker

### Performance
- Semantic: O(1) `declare_fn` via hash lookup (-61% semantic time)
- Semantic: O(1) `declare_struct` via hash lookup (-61% semantic time)
- Semantic: O(1) `declare_enum` via hash lookup (-61% semantic time)
- Semantic: O(1) `find_module` via hash cache (was O(n) linked list)
- Codegen: O(1) regalloc worklist membership via tag array (was O(n) scan)
- Codegen: sorted sweep interference build with early exit (-12% codegen time)
- IR pool: 262k→32k slots per-function; peak memory -87.5% (30MB→3.7MB)
- stdlib: word-at-a-time `memcpy` (8 bytes per iteration)
- stdlib: word-at-a-time `memset` (8 bytes per iteration)
- stdlib: word-at-a-time `memzero` (8 bytes per iteration)
- stdlib: string dedup via 256-bucket hash table (was O(n) linked list)
- Assembler Token: 40→28 bytes per token (-30% memory on large files)
- Regalloc: live range fix for loops (spills reduced 73→2)
- Regalloc: worklist removal now O(1) via tag array
- Self-compile -O0: 960ms (-7% vs v0.0.13)
- Self-compile -O1: 3300ms (-5% vs v0.0.13)
- bench.cst -O0: 2x faster runtime (2.49s→1.15s) vs v0.0.13
- bench.cst -O1: 2.1x faster runtime (1.15s→0.54s) vs -O0

### Fixed
- f64→f32 cast now uses SSE `cvtsd2ss` (was integer truncation)
- only-import type resolution: `mem.Pool` resolves without cache corruption
- only-import type resolution: `mem.Bins` resolves without cache corruption
- asm() escape sequence fix: `\n` now works in inline assembly strings
- asm() escape sequence fix: `\t` now works in inline assembly strings
- asm() escape sequence fix: `\x1b` now works in inline assembly strings
- only-import cache corruption: moved `has_only` flag from Module to Variable struct
- Dual only-imports of same module: each creates separate Module entry per alias
- Inliner: `is_extern_call` now copied in function clone
- Mem2reg: single-block functions now use fast path (was skipping them)
- Optimizer buffer overflow fixed in `pass_dce`
- Optimizer buffer overflow fixed in `pass_addr_fusion`
- Optimizer buffer overflow fixed in `pass_const_copy_prop`
- Mangle generic 512-byte buffer: bounds check prevents stack overflow
- Codegen: use-count guard in try_fuse_inc_stack prevents stale load fuse

### Removed
- Removed `mem.gheapinit` wrapper (full bins migration)
- Removed `mem.galloc` wrapper (full bins migration)
- Removed `mem.gfree` wrapper (full bins migration)
- `stdlib-roadmap.md` documentation (consolidated into guides)
- Error messages guide (`docs/guide/error-messages.md`)

## [v0.0.13](https://github.com/Caua726/Caustic/releases/tag/v0.0.13) — 2026-03-23

### Added

- `call()` indirect function calls with type checking (`call(fn_ptr, arg1, arg2)`).
- Typed function pointers via `fn_ptr()` returning `TY_FN` with parameter/return types.
- f32 literal narrowing from f64 in `let`.
- f32 literal narrowing from f64 in assign.
- f32 literal narrowing from f64 in binop.
- f32 literal narrowing from f64 in compare.
- f32 literal narrowing from f64 in return.
- Call register encoding in assembler (FF /2).
- Jmp register encoding in assembler (FF /4).
- stdlib `math.cst`.
- stdlib `sort.cst`.
- stdlib `map.cst`.
- stdlib `random.cst`.
- stdlib `net.cst`.
- stdlib `arena.cst`.
- stdlib `env.cst`.
- stdlib `time.cst`.
- **math.cst**: `abs` (INT64_MIN fix).
- **math.cst**: `min` (INT64_MIN fix).
- **math.cst**: `max` (INT64_MIN fix).
- **math.cst**: `clamp` (INT64_MIN fix).
- **math.cst**: `pow` (INT64_MIN fix).
- **math.cst**: `gcd` (INT64_MIN fix).
- **math.cst**: `lcm` (INT64_MIN fix).
- **math.cst**: `align_up` (INT64_MIN fix).
- **math.cst**: `align_down` (INT64_MIN fix).
- **sort.cst**: insertion sort (fn_ptr comparators, 624 lines).
- **sort.cst**: bubble sort (fn_ptr comparators, 624 lines).
- **sort.cst**: quicksort (fn_ptr comparators, 624 lines).
- **sort.cst**: heapsort (fn_ptr comparators, 624 lines).
- **sort.cst**: mergesort (fn_ptr comparators, 624 lines).
- **map.cst**: `MapI64` (splitmix64) hash map with iterators and null checks.
- **map.cst**: `MapStr` (FNV-1a) hash map with iterators and null checks.
- **random.cst**: xoshiro256** PRNG.
- **random.cst**: rejection sampling.
- **random.cst**: `getrandom` (Linux/Windows dispatch).
- **net.cst**: TCP sockets.
- **net.cst**: UDP sockets.
- **net.cst**: `bind`.
- **net.cst**: `listen`.
- **net.cst**: `accept`.
- **net.cst**: `connect`.
- **net.cst**: `poll`.
- **net.cst**: address struct.
- **net.cst**: socket options.
- **arena.cst**: O(1) bump allocator with bulk free support.
- **env.cst**: argc/argv access.
- **env.cst**: `getenv` (Windows GetEnvironmentVariable dispatch).
- **time.cst**: monotonic/wall clock (clock_gettime/QueryPerformanceCounter dispatch).
- **time.cst**: `sleep` (clock_gettime/QueryPerformanceCounter dispatch).
- **time.cst**: `elapsed` (clock_gettime/QueryPerformanceCounter dispatch).
- **string.cst**: `substring`.
- **string.cst**: `trim`.
- **string.cst**: case conversion.
- **string.cst**: `repeat`.
- **string.cst**: `replace`.
- **string.cst**: `split`.
- **string.cst**: `join`.
- **string.cst** `_in` variants for explicit allocator control on all operations.
- **slice.cst**: `clear`.
- **slice.cst**: `reverse`.
- **slice.cst**: `insert`.
- **slice.cst**: `remove_at`.
- **slice.cst**: `swap`.
- **slice.cst**: `clone`.
- **slice.cst**: `reserve`.
- **slice.cst**: `truncate`.
- **slice.cst**: `first`.
- **slice.cst**: `last`.
- **io.cst**: stderr.
- **io.cst**: Write trait methods.
- **io.cst**: `copy_file`.
- **io.cst**: `list_dir`.
- **io.cst**: `count_dir_entries`.
- **io.cst**: `is_dir`.
- **io.cst**: `get_cwd`.
- **linux.cst**: 60+ syscall wrappers — process primitives.
- **linux.cst**: 60+ syscall wrappers — filesystem primitives.
- **linux.cst**: 60+ syscall wrappers — time primitives.
- **linux.cst**: 60+ syscall wrappers — memory primitives.
- **linux.cst**: 60+ syscall wrappers — network primitives.
- **compatcffi.cst**: C FFI struct passing helpers.
- **compatcffi.cst**: byte swapping.
- **compatcffi.cst**: cloning.
- **compatcffi.cst**: errno access.
- **compatcffi.cst**: debug printing.
- Codegen-level constant inlining for EQ/NE (cmp_try_inline_imm) to fix register corruption.
- DJB2 hash table lookup for semantic-phase functions.
- DJB2 hash table lookup for semantic-phase structs.
- DJB2 hash table lookup for semantic-phase enums.
- DJB2 hash table lookup for semantic-phase type aliases.
- Debugir improvements: globals section.
- Debugir improvements: strings section.
- Debugir improvements: function metadata.
- Debugir improvements: live/dead counts.
- Debugir `-O1` mode: show IR before and after optimization.
- Codegen bytecode inspection (before/after per function).

### Changed

- EQ/NE constant folding moved from optimizer to codegen instruction emission.
- Compilation determinism: ir_tl saved/restored per submodule to fix debug line number variance.
- Compiler refactored: `emit` split into 4 modules (all < 850 lines).
- Compiler refactored: `walk` split into 3 modules (all < 850 lines).
- Compiler refactored: `gen` split into 4 modules (all < 850 lines).
- Compiler refactored: `opt_passes` split into 4 modules (all < 850 lines).
- Compiler refactored: `cache` split into pairs (all < 850 lines).
- Sort.cst refactored from 902 to 624 lines using fn_ptr comparators.
- Float literal folding now reads fval for f64→f32 narrowing and global init.
- Hash table capacity check: `FN_HASH_CAP` (4096).
- Hash table capacity check: `TY_HASH_CAP` (2048).
- Parser builtin: `__target_is_linux`.
- Parser builtin: `__target_is_windows`.
- Parser builtin: `__target_is_x86_64`.
- Parser builtin: `__target_is_aarch64`.
- Zero warnings on self-compile (prefix unused vars with `_`).

### Performance

- **45% faster compile**: removing 15 unused imports reduced self-compile from 1793ms to 975ms.
- **Semantic phase 58% faster**: const_copy_prop/addr_fusion/etc optimizations drop from 1150ms to 480ms.
- **O1 self-compile fixed**: was crashing (SIGSEGV), now completes in 5346ms (first release with -O1 bootstrap).
- Bootstrap O0 improved 649ms → 601ms.

### Fixed

- EQ/NE constant register corruption via cmp_try_inline_imm (marks IMM dead during emission, not optimizer).
- SRET struct return: stack alignment before offset read.
- SRET struct return: hidden parameter in arg count.
- SRET struct return: struct copying with COPY (rep movsb).
- Struct parameter passing: read all GET_ARGs before COPY to avoid register clobber (rcx).
- asm() no longer drops code after inline assembly.
- Register cache invalidated after IR_CALL_INDIRECT.
- `clone_node_deep`/`substitute_in_node` now handle `while` body.
- `clone_node_deep`/`substitute_in_node` now handle `for` body.
- `clone_node_deep`/`substitute_in_node` now handle `NK_CALL_INDIRECT`.
- DCE marks multi-def IR_IMM dead when vreg has 0 uses.
- LICM hend decremented after hoist.
- gen_fncall/call_indirect bounds check 64 arguments.
- call() float return: movq rax,xmm0 for f32/f64.
- Assembler: ah register mapping removed (unsupported).
- Assembler: `reg_ext(REG_NONE=255)` returns 0.
- Assembler: movs 4-char match removed (movsb only).
- Assembler: `ht_lookup`/`ht_insert` probe limit aligned (1024).
- Assembler: undefined symbols get STT_NOTYPE.
- Assembler: Jcc to undefined label generates R_X86_64_PC32.
- Linker: `read_i64` for signed r_addend in RELA entries.
- Linker: `.rodata` moved to R+W segment.
- Linker: GOTPCREL null deref guard in static mode.
- Linker: duplicate symbol errors instead of silent warn.
- Build system: `remove_dir` recurses into subdirectories.
- Build system: `mkdir_parent` handles absolute paths.
- Build system: child processes inherit PATH.
- Build system: `depend` without url produces error.
- Parser: TK_EOF push-back null guard.
- 50+ stdlib bugfixes — INT64_MIN handling in math.
- 50+ stdlib bugfixes — INT64_MIN handling in string.
- 50+ stdlib bugfixes — null checks in string.
- 50+ stdlib bugfixes — null checks in slice.
- 50+ stdlib bugfixes — bounds checks in slice.
- 50+ stdlib bugfixes — bounds checks in io.
- 50+ stdlib bugfixes — double-free detection in mem.
- 50+ stdlib bugfixes — heap coalescing.
- 50+ stdlib bugfixes — bswap overflow.
- mem.cst: galloc null checks.
- mem.cst: replace underflow guard.
- mem.cst: split threshold tuning.
- string.cst: `cstr_len` with empty strings.
- string.cst: `trim_right` break workaround removed.
- string.cst: `trim` break workaround removed.
- io.cst: `print_hex` correction.
- io.cst: `write_int` correction.
- io.cst: `read_file` correction.
- io.cst: `printf` correction.
- slice.cst: pop abort on empty.
- slice.cst: push overflow protection.
- slice.cst: iterator bounds.
- random.cst: INT64_MIN in range.
- random.cst: getrandom loop checks.
- random.cst: rng_bool precedence fix.
- sort.cst: is_power_of_two optimization.
- sort.cst: fn_ptr comparator integration.
- Examples: leaks fixed.
- Examples: buffer overflow in calculator.
- Examples: i32 overflow.
- Examples: bounds checks in bench.

### Removed

- Removed unused import from `profiling` (slow module cascade); 15 unused imports removed in total.
- Removed unused import from `gen_expr` (slow module cascade); 15 unused imports removed in total.
- Removed unused import from `walk` (slow module cascade); 15 unused imports removed in total.
- Removed unused import from `emit` (slow module cascade); 15 unused imports removed in total.
- EQ/NE fold in fold_immediates (codegen-level inlining replaces it).
- Removed break workaround in `string.cst` `trim_right` (SRET bug now fixed).
- Removed break workaround in `string.cst` `trim` (SRET bug now fixed).
- Removed unused `c2p` variable.
- Removed unused `astcr` imports.
- OK print statement in IR codegen.
- Workaround comments (fold_immediates TODO, now self-contained).

## [v0.0.12](https://github.com/Caua726/Caustic/releases/tag/v0.0.12) — 2026-03-21

### Added

- **Module system** — `use "path" only name1, name2 as alias` selective imports, submodule resolution
- **Submodule dot notation** — chained access via `mod.submod.func()` and `mod.submod.VAR`
- **caustic-mk depend** — dependency resolution with git clone: `depend "name" in "url" tag "version"`
- **caustic-mk depend** — `system "lib"` native libs
- **caustic-mk depend** — `--path` flag
- **-debugir flag** — print IR instructions after generation for debugging
- **Assembler SSE instructions** — `cvtsd2ss` encoding support
- **Assembler SSE instructions** — `cvtss2sd` encoding support
- **Assembler SSE instructions** — `movd` encoding support
- **examples/generics.cst** — generics usage example and rebuild binaries

### Changed

- **Float ABI for extern fn** — f32/f64 args now passed in xmm0–xmm7 registers (x86_64 ABI compliance)
- **f32 internal ABI** — Caustic-to-Caustic f32 calls with automatic cvtsd2ss/cvtss2sd conversion
- **Float literal narrowing** — f64→f32 narrowing for assignments and extern fn args
- **Module cache system** — semantic analysis caches imported modules to avoid re-parsing
- **Codegen float return handling** — xmm0 only for extern calls, rax for internal calls
- **Submodule field access** — resolved submodule constant/variable nested access (mod.var.field)

### Performance

- Binary size reduction: `caustic` (−46.3 KB).
- Binary size reduction: `caustic-as` (−20.3 KB).
- Binary size reduction: `caustic-mk` (−11.6 KB).

### Fixed

- **Bool/small return ABI** — `movzx` on 1/2-byte return types from extern calls (SDL3 bool compat)
- **Stack alignment for extern fn** — 16-byte RSP alignment requirement for SSE instructions
- **SET_CTX scalar assign** — r10 register no longer corrupted on struct-return scalar assignment
- **Bootstrap module fallthrough** — submodule resolution no longer falls through incorrectly
- **Extern float literal conversion** — xmm0→rax conversion limited to extern calls only

### Removed

- **Legacy spec files** — `movsxd` spec removed (13 obsolete specs total).
- **Legacy spec files** — `codegen-v2` spec removed (13 obsolete specs total).
- **Legacy spec files** — `graph-coloring-v2` spec removed (13 obsolete specs total).
- **Legacy spec files** — `i32-ssa-audit` spec removed (13 obsolete specs total).
- **Legacy spec files** — `o1-bugfix-plan` spec removed (13 obsolete specs total).
- **Legacy spec files** — `optimization-flags` spec removed (13 obsolete specs total).
- **Legacy spec files** — peephole variant specs removed (13 obsolete specs total).
- **Legacy spec files** — `store-load-fwd` spec removed (13 obsolete specs total).
- **Legacy spec files** — `strength-reduction-v2` spec removed (13 obsolete specs total).
- **Legacy spec files** — `typed-phi-destruction` spec removed (13 obsolete specs total).

## [v0.0.11](https://github.com/Caua726/Caustic/releases/tag/v0.0.11) — 2026-03-20

### Added

- Per-pass optimizer profiling showing individual pass timings
- Pipeline breakdown view in single-file mode — lex phase timing.
- Pipeline breakdown view in single-file mode — parse phase timing.
- Pipeline breakdown view in single-file mode — semantic phase timing.
- Pipeline breakdown view in single-file mode — ir phase timing.
- Pipeline breakdown view in single-file mode — codegen phase timing.

### Changed

- Strength reduction now uses O(1) lookup table `def_inst`.
- Strength reduction now uses O(1) lookup table `inst_at`.
- Strength reduction now uses O(1) lookup table `call_prefix`.
- `sr_process_loop` extracted, reducing VREGs 4902 → 4061 for faster coloring
- mem2reg optimized with O(1) offset→slot map.
- mem2reg optimized with per-block phi index.

### Performance

- `-O1` compile 36% faster (7.7s → 4.9s)
- Optimizer phase 42% faster (626ms → 364ms)
- Strength reduction pass 70% faster (90ms → 27ms)
- SR codegen 65% faster (2651ms → 928ms)

## [v0.0.10](https://github.com/Caua726/Caustic/releases/tag/v0.0.10) — 2026-03-20

### Added

- `-O0` optimization flag (default) — uses linear scan with no IR optimization.
- `-O1` optimization flag.
- Constant propagation with single-def guard prevents stale constants in loops after SSA phi destruction.
- Copy propagation with single-def guard and vreg size boundary safety for i32/i64.
- Store-load forwarding now supports both i32 and i64 stores/loads; invalidates at branches.
- Fold immediates optimization with single-def guard folds constants into instruction operands.
- LICM (loop-invariant code motion) with multi-def guard prevents hoisting phi vregs.
- Strength reduction v2 detects MUL by loop-invariant variable; converts to pointer increments.
- Strength reduction v2 detects array indexing chains; converts to pointer increments.
- DCE (dead code elimination) with multi-def guard preserves phi back-edge copies.
- Function inlining inlines small (<20 IR instructions) non-recursive, non-variadic functions.
- Peephole optimizer pattern: MOV+movsxd fusion.
- Peephole optimizer pattern: MOV chain bypass.
- Peephole optimizer pattern: dead MOV elimination.
- Graph coloring v2 register allocator uses iterative register coalescing (George & Appel 1996).
- Register cache (16-entry) tracks spilled vregs with targeted invalidation; eliminates ~21% redundant loads.
- i32 SSA promotion via addr_fusion moved before mem2reg; i32 stack variables now promoted to SSA vregs.
- vreg_size tracking prevents copy propagation from crossing i32/i64 type boundaries.
- Typed phi destruction annotates phi-destruction MOVs with type information for const_copy_prop size safety.
- Instruction selection fuses IMM+STORE (direct memory immediate).
- Instruction selection fuses stack increment sequences.
- SIB byte encoding in assembler supports [base + index * scale] addressing mode syntax.
- Quicksort (Lomuto partition) replaces bubble sort; benchmark sorts 1M elements instead of 10K.
- Matmul benchmark uses cache-friendly ikj order instead of ijk.
- Interactive benchmark comparison (`bench/results.html`) with 52 language entries (Chart.js).

### Changed

- Register allocator completely rewritten: linear scan (now -O0 only) → graph coloring v2 (iterative coalescing).
- Graph coloring uses bit-matrix interference graph.
- Graph coloring uses George coalescing criterion.
- Graph coloring uses optimistic Briggs coloring.
- Loop-depth-weighted spill costs (pow10 table: 1/100/10000/100000).
- GET_ARG ABI register exclusion prevents parameter clobber.
- All codegen optimizations gated behind `-O1`; zero overhead when disabled.
- Assembler ParsedLine extended with index register and scale fields for SIB operands.
- Peephole optimizer (was standalone) now integrated into main compiler pipeline.

### Performance

- 3.4x runtime speedup: bench.cst 2592ms `-O0` → 768ms `-O1`.
- Matmul (64x64 x100): 233ms → 173ms.
- Sieve (10M): 86ms → 55ms (3.5x faster).
- Sort (1M Lomuto): 252ms → 199ms (3.8x faster).
- Collatz (1M): 201ms → 182ms (4.5x faster).
- Fib(38): 150ms → 157ms (1.4x faster).
- Caustic `-O1` now 1.08x faster than GCC `-O0` on identical algorithms.
- Binary size: 781KB → 1.1MB (caustic) due to new allocator and optimizer code.
- Bootstrap `-O1` deterministic: gen2 = gen3 = gen4 (1035KB binary, byte-identical).

### Fixed

- `pass_const_copy_prop` phi destruction: IR_MOV with non-constant source must clear `is_const` flag.
- `pass_store_load_fwd` invalidates forwarding table at JMP/JZ/JNZ branches (was forwarding across control flow).
- Remove second `mem2reg` pass (was corrupting IR for programs with heap allocations).
- `reg_name_to_idx` rdi/rdx disambiguation: rdi check moved before rdx to prevent shadowing.
- IR_COPY cache invalidation for `rdi` after rep movsb.
- IR_COPY cache invalidation for `rsi` after rep movsb.
- IR_COPY cache invalidation for `rcx` after rep movsb.
- IR_SET_CTX cache invalidation for r10 on IMM path.
- `find_promotable` collects address-taken vars in first pass before scanning LOAD/STORE.
- `destroy_ssa` updates `pbb.first` when inserting phi MOV before single-instruction block.
- Strength reduction loop processing order (bottom-to-top) for position stability.

### Removed

- Second `mem2reg` optimization pass (corrupted heap allocation IR).
- Blanket scratch register invalidation; now uses targeted invalidation per instruction.

## [v0.0.9](https://github.com/Caua726/Caustic/releases/tag/v0.0.9) — 2026-03-18

### Added

- SSA mem2reg pass with full CFG construction, Cooper-Harvey-Kennedy dominance, phi insertion, and rename.
- Store-load forwarding for i64 stack variables within basic blocks.
- Register coalescing hints to reduce phi-destruction MOV instructions.
- Loop-invariant code motion for call-free loops.
- Benchmark suite: `fib(38)`.
- Benchmark suite: `sieve(10M)`.
- Benchmark suite: `sort(10K)`.
- Benchmark suite: `matmul(64)`.
- Benchmark suite: `collatz(1M)`.
- Assembler hash table expanded to 1024 entries with direct pattern matching (len 2–9).
- Dynamic scratch register selection (replaces hardcoded r15 usage in codegen).
- Fast instruction-size oracle for RIP-relative addressing pass.

### Changed

- Freed `r15` for register allocation (10 allocatable registers total).
- Added `rdi` as caller-saved allocatable register (7→9 registers).
- Added `rsi` as caller-saved allocatable register (7→9 registers).
- Push/pop pruning now only saves callee-saved registers actually used in function.
- Power-of-2 array strides now use SHL instead of MUL.
- Zero checks now use `test r15,r15` instead of `cmp r15,0`.
- Encoder split into 3 submodules in assembler.
- SSA split into `cfg.cst`.
- SSA split into `dom.cst`.
- SSA split into `ssa.cst`.
- Major refactoring: `gen.cst` (680L→380L).
- Major refactoring: `emit.cst` (685L split).
- Major refactoring: `walk.cst`.
- Major refactoring: `main.cst`.
- Major refactoring: `ir/cache.cst`.
- All compiler functions now ≤100 lines (extracted 11 sub-functions).
- Binary sizes significantly reduced: `caustic` −23%.
- Binary sizes significantly reduced: `caustic-as` −16%.
- Binary sizes significantly reduced: `caustic-ld` −37%.

### Performance

- Beats LuaJIT on aggregate benchmark score (1060ms vs 1081ms).
- fib(38): −7% (191ms→177ms).
- matmul(64): −28% (309ms→222ms).
- Self-compile (gen4): ~500ms.

### Fixed

- SSA DFS stack overflow in compute_rpo by doubling buffer size.
- SSA has_phi heap allocation to support >16 basic blocks.
- SSA STORE rename guard against non-vreg operands.
- Allocator caller-saved spill pass now checks crosses_call flag.
- Allocator `used[]` allocation with headroom.
- Allocator `use_count[]` allocation with headroom.
- Allocator `addr_offset[]` allocation with headroom.
- Binary-op constant fold guard against non-vreg operands (OOB read).
- `sem_filename` pass 6 module traversal fix.
- Assembler `.ascii` support fix.
- `Free()` bounds check in mem allocator.
- 8-byte alignment fix in mem allocator.
- Slice `push()` guard for zero capacity.
- Slice `pop()` guard on empty.
- Bounds check ir_done_bodies before write.

## [v0.0.8](https://github.com/Caua726/Caustic/releases/tag/v0.0.8) — 2026-03-18

### Added
- SSA mem2reg promotion with phi nodes, dominance frontiers, and SSA destruction.
- Register coalescing hints to reduce MOV-connected vreg copy overhead.
- Assembler hash table (1024 entries) for instruction/register tokenization (1.7×).

### Changed
- Allocator alignment increased to 8-byte boundary in stdlib.

### Performance
- Store-load forwarding eliminates redundant loads from stack within basic blocks.
- Strength reduction in codegen pipeline.
- Address-store fusion in codegen pipeline.
- Peephole reload elision across instructions.
- Overall benchmark performance improved ~20% — collatz.
- Overall benchmark performance improved ~20% — fib.
- Overall benchmark performance improved ~20% — sort.

### Fixed
- Heap overflow fixed in SSA arrays.
- Heap overflow fixed in optimizer arrays.
- DFS stack overflow in optimization passes.
- Caller-saved register spillage across function calls.
- Stdlib slice `push` boundary check.
- Stdlib slice `pop` boundary check.
- Free function bounds validation.
- 12 total optimizer and runtime correctness fixes.

### Removed
- Redundant stack memory operations via store-load forwarding.

## [v0.0.7](https://github.com/Caua726/Caustic/releases/tag/v0.0.7) — 2026-02-26

### Added
- Strength reduction: `div`/`mod` by power-of-2 constant uses `shr`/`sar` instead of `idiv`.
- Signed `div`/`mod` fast path: correct rounding for negative divisors via `sar+shr+add` correction.
- Constant propagation in IR (`optimize_func` Pass 1).
- Copy propagation in IR (`optimize_func` Pass 1).
- ADDR+STORE fusion: eliminates address vregs for direct `mov [rbp-N], val` stack writes.
- MUL immediate folding: constants folded into multiply operand via `mov rax, imm; imul`.
- Peephole reload elision: skips redundant loads when register already holds the spilled value.

### Performance
- collatz (N=10M): 6700ms → 4100ms (39% faster).
- Spill count: 27 → 11.
- Compiler `.text` section: 776KB → 728KB (48KB redundant loads eliminated).

## [v0.0.6](https://github.com/Caua726/Caustic/releases/tag/v0.0.6) — 2026-02-26

### Added

- FFI example demonstrating extern function calls and Windows/Linux dispatch.

### Changed

- Register allocator: active set tracking (O(7) vs O(N) per allocation).
- Register allocator: merge sort for live intervals (O(N log N) vs O(N²) insertion).
- Register allocator: binary search in `spans_call()` (O(log N) vs linear scan).
- Register allocator: spill cost eviction with tie-break by longest remaining range.
- Register allocator: expire-before-allocate loop cleanup before allocation.
- Register allocator: added `r8` (caller-saved) with clobber tracking.
- Register allocator: added `r9` (caller-saved) with clobber tracking.
- Register allocator: added `r10` (caller-saved) with clobber tracking.
- Register allocator: r14 added to allocatable set.
- Codegen: direct `arg`/`mov` fusion optimization.
- Codegen: `gen_binop()` operand handling improvements.
- Codegen: cmp+branch fusion for tighter conditional sequences.
- IR optimizer enhancements.

### Performance

- Self-hosting compilation: 3.01s → 0.49s (~6× faster).
- Assembly output: 5.59MB → 4.42MB (21% smaller).
- Binary `.text` section: 1.05MB → 785KB (25% smaller).
- Register spills per function: 87 → 32 (63% reduction).
- Gen2/Gen3 produce byte-identical output (self-hosting fixpoint stable).

## [v0.0.5](https://github.com/Caua726/Caustic/releases/tag/v0.0.5) — 2026-02-22

### Added
- `--profile` flag: full pipeline breakdown, assembler sub-phases, codegen per-function timing.
- Pipeline profiling: 8-phase timing (lexer, parser, semantic, IR, codegen, assembler, linker, etc).
- Assembler profiling: tokenize sub-phase.
- Assembler profiling: parse sub-phase.
- Assembler profiling: pass1 (symbols) sub-phase.
- Assembler profiling: pass2 (encode) sub-phase.
- Assembler profiling: write ELF sub-phase.
- Per-function codegen profiling: name.
- Per-function codegen profiling: vreg count.
- Per-function codegen profiling: execution time in microseconds.
- Compilation stats: source bytes.
- Compilation stats: token count.
- Compilation stats: function count.
- Compilation stats: IR instruction count.
- Compilation stats: assembly bytes.
- Compilation stats: object size.
- Top codegen functions ranking sorted by time (sorted descending bubble sort display).
- `examples/ffi.cst` target in Causticfile for testing extern function calls.

## [v0.0.4](https://github.com/Caua726/Caustic/releases/tag/v0.0.4) — 2026-02-22

### Added
- Interface files (`.csti`).
- `--emit-interface` flag.
- Auto `.csti` loading.
- Binary cache: `--emit-tokens` flag for pipeline serialization.
- Binary cache: `--emit-ast` flag for pipeline serialization.
- Binary cache: `--emit-ir` flag for pipeline serialization.
- `--cache <dir>` flag for automatic lex/parse/semantic/IR caching.
- `--max-ram` flag for dynamic heap configuration and source complexity sizing.
- Multi-file compilation with per-file heap reset.
- caustic-mk `--cache <dir>` integration for single-step builds.

### Fixed
- Hash sign extension bug in IR cache serialization.
- Hash sign extension bug in AST cache serialization.

## [v0.0.3](https://github.com/Caua726/Caustic/releases/tag/v0.0.3) — 2026-02-22

### Added
- Multi-file compilation (`caustic a.cst b.cst -o prog`).
- `--max-ram <MB>` flag to limit memory usage.
- `mem.gheapreset(size)` for heap reset/recreation.
- `ir_reset()` to clear IR state between compilation units.
- `linux.rename()` syscall wrapper.
- `linux.mkdir()` syscall wrapper.

### Changed
- Dynamic heap sizing based on source/imports (previously fixed 128MB).

### Fixed
- `-c -o` now outputs to specified path correctly.

## [v0.0.2](https://github.com/Caua726/Caustic/releases/tag/v0.0.2) — 2026-02-22

### Added

- `caustic-mk install` command.
- `caustic-mk uninstall` command.
- Curl-based installer.
- Stdlib resolution from binary path (`<binary>/../lib/caustic/`)
- Distribution tarball creation via `caustic-mk dist`
- `caustic-mk` build system (replaces Makefile for projects)
- DWARF debug info: `.file` directive in assembly output.
- DWARF debug info: `.loc` directive in assembly output.
- Generic slices stdlib (`std/slice.cst`) with cross-module generic instantiation
- Unused variable warnings (prefix with `_` to suppress)
- Type aliases (`type Name = ExistingType;`)
- Semantic error reporting with source position information
- `__builtin_va_start` support for variadic function declarations
- Language documentation: getting-started.
- Language documentation: types.
- Language documentation: variables.
- Language documentation: functions.
- Language documentation: operators.
- Language documentation: control flow.
- Language documentation: structs.
- Language documentation: enums.
- Language documentation: impl blocks.
- Language documentation: generics.
- Language documentation: modules.
- Language documentation: memory.
- Language documentation: advanced topics.
- Reference documentation: lexer.
- Reference documentation: parser.
- Reference documentation: semantic analysis.
- Reference documentation: IR.
- Reference documentation: codegen.
- Reference documentation: assembler.
- Reference documentation: linker.
- Reference documentation: stdlib.
- Example program: hello world.
- Example program: fibonacci.
- Example program: strings.
- Example program: structs.
- Example program: files.
- Example program: calculator.
- Example program: linked list.
- Example program: enums.
- Examples README with cross-references
- Causticfile build manifest format

### Changed

- Renamed `caustic-compiler/` to `src/` (standardized project structure)
- Migrated `src/io.cst` to `std/io.cst` (moved I/O to standard library)
- README updated with self-hosted compiler status and new installation method
- Updated CLAUDE.md with build commands and language features documentation

### Removed

- Makefile (replaced by `caustic-mk`)
- Tracked `.s` assembly files from repository


## [v0.0.1](https://github.com/Caua726/Caustic/releases/tag/v0.0.1) — 2026-02-21

### Added

- Self-hosted compiler, entirely written in Caustic itself.
- Self-hosted assembler, entirely written in Caustic itself.
- Self-hosted linker, entirely written in Caustic itself.
- Stable self-compilation — compiler successfully compiles itself to bit-identical binaries.
- x86_64 Linux ELF executable generation with no libc dependency (direct syscalls).
- Compiler pipeline phase: lexer.
- Compiler pipeline phase: parser.
- Compiler pipeline phase: semantic analysis.
- Compiler pipeline phase: IR.
- Compiler pipeline phase: codegen.
- Compiler pipeline phase: assembly.
- Comprehensive type system with primitives.
- Comprehensive type system with pointers.
- Comprehensive type system with arrays.
- Comprehensive type system with structs.
- Comprehensive type system with enums.
- Comprehensive type system with generics.
- Monomorphic generics instantiation with full type safety across instantiations.
- Pattern matching on tagged unions and enums with exhaustiveness checking.
- Function pointers via `fn_ptr()` builtin enabling indirect function calls.
- Foreign function interface (FFI) with `extern` declarations for C library calls.
- `defer` statement with LIFO execution order for resource cleanup.
- Variadic functions with type-safe argument access.
- System V AMD64 ABI compliance for function calls.
- System V AMD64 ABI compliance for struct passing.
- Standard library: memory management (malloc/free).
- Standard library: I/O.
- Standard library: strings.
- Standard library: math.
- Standard library: collections.
- Module system with source imports (`use "path/to/module.cst" as name`).
- Module system with selective imports.
- Optimization pass: constant folding.
- Optimization pass: dead code elimination.
- Optimization pass: leaf function inlining.
- DWARF debug symbol generation for GDB source-level debugging.
- Syscall abstraction layer (`std/os.cst`) for portable Linux operations.
- Dynamic linking support with symbol resolution.
- Dynamic linking support with relocation.
