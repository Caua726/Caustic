# Incremental compilation — staged plan

## Why

Profiling a compiler self-build (`caustic src/main.cst -o out`, ~580 ms total):

| phase | time | % |
|---|---|---|
| lex + parse (top file) | 3 ms | ~1% |
| **semantic** (loads + re-parses + analyzes *all* imports each build) | 148 ms | 44% |
| ir | 61 ms | 18% |
| codegen | 118 ms | 35% |
| — assemble (`caustic-as`, separate process) | 234 ms | — |
| — link (`caustic-ld`) | 15 ms | — |

Parse/token/AST caching is worthless for speed (parse is ~0.6%). The cost is
**semantic + codegen + assemble**, and today every build redoes all of it for
the whole program. The only real win is **not recompiling unchanged modules** —
i.e. real separate compilation with per-module object caching.

## What blocks it today

1. **Whole-program model.** `caustic src/main.cst` parses main, then semantic
   recursively reads + parses + analyzes *every transitive import* (full source),
   IR-gens everything, codegens everything into one 5.7 MB `.s` → one `.o`.
2. **`-c` is not separate compilation.** `caustic -c src/ir/opt_mem.cst`
   produces a 183 KB `.s` with **699 functions** — it includes the whole import
   closure, not just the module's own functions. Caching/linking such objects
   would duplicate every shared dependency.
3. **No interface consumption.** `interface.cst` only *emits* `.csti`
   (`emit_csti`); nothing reads it. `semantic/modules.cst` always `read_file`s
   the full source of each import and re-parses it. So compiling N modules
   separately re-analyzes shared imports N times (first/clean build would get
   *slower*, not faster).

Useful hook: `IRFunction.asm_name_ptr` is the mangled name (e.g.
`_src_ir_opt_mem_cst_opt_alloc`) — the module is derivable from its prefix.
Codegen emits every function with `is_reachable == 1 && _is_dyn_extern == 0`
(`emit_driver.cst`). Cross-module calls already use the mangled symbol; the
linker resolves them.

## Stages

### Stage 1 — module-only emission (compiler) — ✅ DONE (commit 2e66ee0)
Add a mode (`--module-only=<src-path>` or derive from the lone input) so codegen
emits **only functions belonging to the target module** and leaves calls to
other modules as undefined symbols (resolved at link). Concretely:
- Compute the target module's mangled prefix from its path.
- In `emit_driver.cst`, gate function *emission* on "asm_name starts with target
  prefix" (still IR-gen/analyze the rest for type info, just don't emit bodies).
- Globals likewise: emit only the module's own globals.
- Result per module: a small `.s` with its own functions + extern references.
**Verify:** linking all per-module `.o`s yields a working binary; ideally
byte-comparable to (or at least behaviorally identical to) the whole-program
build. Bootstrap fixpoint (gen2 == gen3) must still hold with the incremental
toolchain.
**Risk:** cross-module DCE. Whole-program builds drop unreachable functions;
per-module objects can't. The **linker** must DCE at link time (it already keeps
"only the functions DCE keeps" for static ELF — confirm/extend `caustic-ld`).

#### Stage 1 — implementation notes (mapped)
- **Mangling.** A function's symbol is `<module_prefix>_<name>`, built in
  `semantic/walk_module.cst:370` from `sc.current_mod_prefix_*`. The prefix is
  computed by `scope.cst:588 get_mod_prefix(path)` — `_` + path with `/ . \ -`
  replaced by `_` (e.g. `src/ir/opt_mem.cst` → `_src_ir_opt_mem_cst`). So the
  target module's prefix is just `get_mod_prefix(<input path>)`.
- **Blocker — root vs import prefix (non-obvious).** The *root/input* file is
  compiled UNPREFIXED (entry convention): `caustic -c src/ir/opt_mem.cst` emits
  `opt_alloc`, `.globl opt_alloc`. But when `main.cst` imports it, the symbol is
  `_src_ir_opt_mem_cst_opt_alloc`. So a separately-compiled module's symbols
  WON'T match its importers' references → link error. **Fix:** in `--module-only`
  mode, assign the root module its canonical import prefix
  (`get_mod_prefix(input)`) instead of the empty root prefix — a small change in
  the semantic entry / `walk_module` where the root's `current_mod_prefix` is set.
- **No-main.** `--module-only` implies a library compile (no `main` required) —
  reuse the existing `--no-main` / `-c` path.
- **Reachability/DCE.** Compiling a library has no `main` to root reachability,
  so emit ALL of the module's own functions (don't rely on `is_reachable`). All
  cross-references resolve (every function is emitted in some `.o`); the cost is
  bloat (unreachable functions kept) until the linker GCs them — hence the
  linker DCE below is required to make incremental binaries non-bloated.
- **Linker has no DCE today** (`caustic-ld` resolves symbols but no
  reachability GC). Needed: from `_start`/entry, mark functions reachable via
  relocation edges, drop the rest. Independently testable on a normal
  whole-program `.o` (must not change behavior; suite + fixpoint hold).

### Stage 2 — maker orchestration (incremental driver) — ✅ DONE
`caustic-mk build <target> --incremental` (commit caustic-maker a634f98 + compiler
463bc95):
- **Module discovery via `--emit-deps`** (compiler-side): `modules.cst` records
  every loaded module's normalized path; emitted after semantic. 101 modules for
  the compiler. The maker caches this list (it only changes on a use-graph edit;
  `clean` to refresh) so body edits skip the ~400 ms emit pass.
- **Per-module object cache**: `hash_file(M)` (streamed djb2) → `.caustic/obj/<h>.o`;
  reuse if present, else `caustic --module-only M && caustic-as M.s` → cache the `.o`.
- **Link**: all `.o` + `--entry=_<mangled-main>_main`.
- **Measured (compiler self-build)**: dev-loop (1 module changed) **~164 ms** vs
  **~945 ms** whole-program (**~5.8×**); the incremental compiler is correct
  (compiles tiny→42, fibonacci, …), ~22% larger (no linker DCE yet).
- **Known cost**: first/clean build ~5.7 s (6× slower) — each module re-runs
  semantic on its import closure (the redundant-semantic problem → Stage 3). So
  `--incremental` is a dev-loop accelerator, not for clean/CI builds.
- **Caveats**: object cache is keyed on source hash only (not the compiler
  binary) — `clean` after rebuilding the compiler itself. Per-module flags
  (`-O`, `--target`) aren't threaded yet (default ELF/-O0). Bloat awaits linker DCE.

### Stage 3 attempt 1 — "skip import bodies in --module-only" (reverted)
A lighter idea than full `.csti`: in `--module-only`, the target only needs
imports' SIGNATURES (passes 1-5), not their bodies — so skip pass 6
(`analyze_bodies`) + the `gen_ir` NK_USE recursion for imports. **It got the
first/clean incremental build from 5.7 s → ~0.73 s.** But it cascaded:
1. Segfault in IR-gen (gen_ir was generating IR for unanalyzed import bodies) —
   fixed by also gating the NK_USE recursion in `gen_program.cst` on
   `tgt.module_only_get() == 0`.
2. Then `std/mem/core.cst:110 nao e modulo 'mem_windows'` — **conditional
   imports** (`if (__target_is_windows) use ... as mem_windows`) stop resolving
   when the import isn't fully processed. And generic instances (appended to
   `ast_tail`, generics.cst:534) may be attributed to a module that no longer
   gets IR-gen'd → undefined symbols.
**Reverted** — the compiler's whole-program assumptions (conditional imports,
generic instantiation site, cross-module reachability) are baked deep enough
that a partial skip miscompiles. A correct version must treat imports as truly
external *consistently* across semantic + IR-gen + reachability, OR go the full
`.csti` route below. This is a dedicated effort, not an end-of-session change.

### Stage 3 — interface (`.csti`) consumption (compiler) — removes redundant semantic
Make `semantic/modules.cst` import via a module's **interface** instead of its
full source: read a cached `.csti` (exported signatures + types) and populate the
symbol table, without parsing/analyzing the module's bodies.
- Define a stable `.csti` format (extend `emit_csti`): exported fn signatures,
  `struct`/`enum`/`type`/generic decls, with cross-module type references.
- Implement a `.csti` reader + a "load interface into module symtab" path.
- Cache `.csti` by source hash; regenerate on change.
- **Reuse the now-faithful AST serializer** (`parser/cache_read.cst` /
  `cache_write.cst`, `ast.canon_type`) to (de)serialize interface ASTs/types.
- Result: compiling module M reads interfaces (fast) instead of re-parsing the
  whole import closure → first/clean incremental build is fast too.
**Risk:** cross-module types + generics in interfaces are the hard part (like C++
modules / Rust rlib metadata). Generic instantiation across module boundaries
needs care.

## Verification strategy (every stage)
- `rm -rf .caustic` before each measured build (stale-cache gotcha).
- Behavioral: `./caustic-mk test` = 5/5, incl. the bootstrap **fixpoint**
  (gen2 == gen3) — proves the incremental toolchain self-hosts deterministically.
- Per-program: incremental-built binary runs correctly; compare against the
  whole-program build (byte-identical if DCE/ordering match, else behaviorally).
- Measure dev-loop: touch one module, time the rebuild vs a full build.

## Foundation already in place
- Faithful AST/Type serializer with primitive canonicalization
  (`ast.canon_type`) and a generics guard — committed (`ea9a294`). Reusable for
  Stage 3 interface (de)serialization.
- `--cache` is safe again (lossy IR cache disabled; pool-init crash fixed).
