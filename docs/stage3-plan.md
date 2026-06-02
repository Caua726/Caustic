# Stage 3 — eliminate redundant cross-module semantic (plan for a dedicated agent)

> Hand this to a fresh agent. It is self-contained: goal, the core insight, a
> verification harness to build FIRST, the recommended approach broken into
> ordered tasks, the dead-ends already hit (don't repeat them), and a file/line
> index. Read `docs/incremental-compilation-plan.md` for the full context of
> Stages 1–2 (both DONE and committed).

## 0. Objective

`caustic-mk build <target> --incremental` (Stage 2) already gives a ~5.8× dev-loop
win, BUT the first/clean incremental build of the compiler is ~5.7 s — **6× slower**
than the 0.95 s whole-program build. Cause: each of the 101 modules is compiled
separately with `--module-only`, and **every one re-runs full semantic on its
entire import closure** (re-analyzing the same shared modules' function bodies
N times).

**Goal:** make a `--module-only` compile analyze ONLY the target module's own
function bodies, treating imports as external — so the first/clean incremental
build drops from ~5.7 s to <1 s, while every per-module `.o` stays byte-for-byte
what the whole-program build would emit for that module.

A prior attempt got the first build **5.7 s → 0.73 s** but miscompiled (details in
§6). This plan is how to do it correctly.

## 1. The core insight (read this twice)

Separate compilation works iff, when compiling module M, each import I exposes
exactly what M needs to type-check + codegen M, and hides the rest:

| Import I must EXPOSE (M depends on it) | I may HIDE (pure redundant cost) |
|---|---|
| struct/enum/type definitions | regular (non-imut, non-generic) function **bodies** |
| function **signatures** (name, params, return, asm_name) | |
| global declarations (name, type, section) | |
| **`with imut` function/global bodies+values** — they fold at M's call sites | |
| **generic templates** (the template AST, for M to instantiate) | |

The redundant cost is analyzing the *hidden* column. The trap is that the naïve
"skip all import bodies" ALSO hides the things in the EXPOSE column — specifically
`with imut` folding and generic templates — and that silently miscompiles.

### Why hiding `imut` bodies miscompiles (the conditional-import trap)
- Cross-OS code is written `if (os.current() == os.OS_WINDOWS) { win.foo(); }`.
- `os.current()` is a **`with imut` function** (std/os.cst); the compiler's
  Layer-2 comptime AST interpreter (`src/semantic/walk.cst:100+`) evaluates its
  body at semantic time and folds the call to a literal (`OS_LINUX` on Linux).
- That makes the comparison `1 == 2` → the `if` body is dead, so semantic never
  needs `win` / `mem_windows`.
- Conditional imports themselves are dead-stripped **at parse** when their guard
  folds false (`src/parser/top.cst:299`: `if (__target_is_windows) use ... as
  mem_windows` → null on Linux). So `mem_windows` is never registered.
- **If you skip analyzing std/os.cst's body, `os.current()` stops folding**, the
  branch stays live, semantic tries to resolve `mem_windows` → `nao e modulo
  'mem_windows'`. This is the exact failure the prior attempt hit at
  `std/mem/core.cst:110`.

So the rule is: **skip regular bodies, but NEVER skip `imut` evaluation or generic
template availability.**

## 2. Build the verification harness FIRST (do not skip)

This is a self-hosting compiler; a subtle miscompile passes small tests and breaks
elsewhere. Build the oracle before touching semantics.

Backup the trusted compiler — the `caustic` target writes `./caustic`:
```bash
cp ./caustic /tmp/caustic.trusted
```

**Oracle A — per-module equivalence (fast, precise).** A correct `--module-only M`
must emit the SAME functions/globals for M as the whole-program build does.
```bash
# whole-program reference assembly:
rm -rf .caustic && ./caustic src/main.cst    # writes src/main.cst.s (5.7 MB, all funcs)
# per-module:
./caustic --module-only src/ir/opt_mem.cst   # writes src/ir/opt_mem.cst.s
# every symbol defined in opt_mem.cst.s (label `^_..._cst_X:`) must appear,
# byte-identical in body, inside main.cst.s. Diff the function bodies by symbol.
```
Write a small script that, for a sampled set of modules, extracts each defined
symbol's body from both and asserts equality. Mismatch = miscompile, with the
exact function named.

**Oracle B — self-host fixpoint (definitive).** The incrementally-built compiler
must rebuild the compiler to the same result:
```bash
rm -rf .caustic && ./caustic-mk build caustic --incremental   # builds ./caustic incrementally
cp ./caustic /tmp/caustic.inc
/tmp/caustic.inc src/main.cst -o /tmp/c.whole       # use the inc compiler, whole-program
cp /tmp/caustic.trusted ./caustic
./caustic src/main.cst -o /tmp/c.trusted
cmp /tmp/c.whole /tmp/c.trusted    # SHOULD be byte-identical (gen-N == gen-N+1)
./caustic-mk test                  # 5/5, incl. bootstrap fixpoint + as/ld toolchain
```
Restore `cp /tmp/caustic.trusted ./caustic` whenever an experiment leaves a broken
`./caustic`.

**Oracle C — breadth.** `for ex in examples/*.cst`: build whole-program vs
incremental, run both, compare exit + stdout. (Use a non-compiler target whose
`out` isn't `./caustic` to avoid clobbering the toolchain while iterating.)

Gate every change in §3 on A+B+C staying green.

## 3. Approach A (recommended): surgical import-body skip

The cheapest correct path. Reuse the prior attempt's two edits, then make them
SURGICAL so imut + generics survive.

### A1. Re-apply the two skips (the speed lever)
- `src/semantic/walk_module.cst`, `visit_use_module` pass 6: wrap
  `analyze_bodies(cached.body)` in `if (tgt.module_only_get() == 0) { ... }`.
  (Imports `tgt` already present from Stage 1.)
- `src/ir/gen_program.cst`, `gen_ir` NK_USE arm (the `if (already == 0 &&
  g.ir_done_count < 256)` block, ~line 545): add `&& tgt.module_only_get() == 0`
  so IR is not generated for imports. (Add `use "../codegen/target.cst" as tgt;`.)
  Without this, gen_ir walks unanalyzed import bodies → segfault.

At this point: first build ~0.73 s, but `os.current()` no longer folds → conditional
imports break. Fix that next.

### A2. Preserve `with imut` folding across the skip (THE critical fix)
The target's `analyze_bodies` calls `os.current()`; the Layer-2 interpreter
(`walk.cst:100+`) evaluates the imut function's body AST. Determine empirically what
state that body needs:
1. First try: in the pass-6 skip, **still analyze imut functions/globals** of the
   import; skip only non-imut bodies. I.e. walk the import body, and for each
   top-level decl, run `analyze_bodies` on it iff it is `with imut` (function or
   global). This keeps folding correct and still skips the bulk (regular fns).
   - Look at how `imut` is flagged on AST function/var nodes (`is_imut`,
     `ast.Node` field, and the `VF_*` / function flags in `semantic/defs.cst`).
2. If imut bodies reference other imuts transitively, that's fine — they're cheap
   and few. Measure: even analyzing all imut decls should stay well under 1 s.
3. Validate with Oracle A on `std/mem/core.cst` specifically (the known failure):
   `./caustic --module-only std/mem/core.cst` must exit 0 with no `mem_windows`
   error, and its emitted bodies must match whole-program.

Alternative if (1) is messy: make the imut interpreter robust to un-analyzed bodies
(resolve identifiers lazily). Prefer (1) — smaller blast radius.

### A3. Verify generic templates + instantiations
Generic instances are appended to `ast_tail` (the root chain, `generics.cst:534`)
during the TARGET's `analyze_bodies`, named by `mangle_generic` (`Slice_i64`, no
module prefix — `generics.cst:89`). They then receive the current module prefix when
their `asm_name` is built. Confirm with Oracle A that:
- A module that instantiates a generic emits the instance under ITS prefix
  (`_<mod>_cst_Slice_i64`), matching how whole-program emits it (so no duplicate and
  no missing symbol at link). If whole-program emits the instance under the
  DEFINING module's prefix instead, you must make `--module-only` attribute it the
  same way, or two modules will both define it / neither will.
- The generic TEMPLATE body must remain available (it's an AST registered in
  passes 1/5, not analyzed in pass 6) so instantiation can still clone+analyze it.
  The skip in A1 doesn't remove templates; just confirm.

### A4. Reachability sanity
`gen_ir` with `no_main` (which `--module-only` sets) marks ALL functions reachable
(`gen_program.cst:644`). With A1 the import functions aren't generated at all, so
only the target's functions exist → all reachable → all emitted (filtered by
prefix in codegen). Confirm no NULL deref from a referenced-but-absent import
function during codegen of the target.

### A5. Iterate to green
Run Oracle A (sampled modules) → B (fixpoint) → C (examples) after each fix. The
fixpoint (B) is the gate: when `/tmp/c.whole` == `/tmp/c.trusted` and the suite is
5/5, Approach A is correct. Then re-measure the first/clean incremental build.

## 4. Approach B (fallback): `.csti` interface files

Only if A's imut/generic coupling proves intractable. More robust (this is how
real compilers do separate compilation), much more code.

- Extend `src/interface.cst:emit_csti` to a COMPLETE interface: structs/enums/type
  aliases (have), non-generic signatures (have), PLUS **generic templates** (bodies)
  and **`with imut` function/global bodies** (so importers can fold), PLUS the
  module's own `use` lines (so transitive interfaces load), PLUS qualified
  cross-module type names (today `write_type` emits a bare struct name —
  `interface.cst:35` — which won't resolve across modules).
- Add a `.csti` reader path in `semantic/modules.cst:visit_use_module`: when a
  cached `.csti` exists and is fresh (hash), load it INSTEAD of the full source —
  parse its declarations (fast, no regular bodies) and register them.
- The maker writes/refreshes `.csti` per module (reuse the `.caustic/obj` cache
  machinery + `hash_file` from `caustic-maker/core/common.cst`).
- Reuse the faithful AST serializer (`src/parser/cache_read.cst` /
  `cache_write.cst`, `ast.canon_type`) if you go binary instead of textual.
- Hard parts (same as any module system): cross-module type identity, generic
  instantiation across the boundary. Budget accordingly.

## 5. Also worth doing (independent of A/B): linker DCE
Per-module objects keep all of a module's functions (no whole-program reachability
to prune), so the incremental binary is ~22% larger and includes dead functions.
A reachability GC in `caustic-ld` (mark from `_start`/entry via relocation edges,
drop unreferenced text/data) removes the bloat AND lets unreachable functions that
reference absent symbols be dropped. Independently testable: running it on a normal
whole-program `.o` must not change behavior (suite + fixpoint stay green).

## 6. Dead-ends from the prior attempt (don't repeat)
- **Blanket skip of pass-6 `analyze_bodies` for imports** → (a) segfault in
  `gen_ir` (it still walked unanalyzed import bodies — needs the A1 `gen_ir` gate),
  then (b) `nao e modulo 'mem_windows'` at `std/mem/core.cst:110` because
  `os.current()` stopped folding (root cause: the imut interpreter needs the imut
  body; see §1/§A2). The fix is the SURGICAL skip (keep imut + templates), not a
  blanket one.
- The dev-loop (Stage 2) was already fast (164 ms) because it recompiles one
  module + relinks; Stage 3's win is specifically the FIRST/CLEAN build. Don't
  regress the dev-loop while chasing the first build.

## 7. Gotchas
- `rm -rf .caustic` before every measured/whole-program build (stale module cache).
- The `caustic` target's `out` is `./caustic` — incremental builds overwrite the
  running compiler. Always keep `/tmp/caustic.trusted` and restore after a bad run.
- The maker's bins allocator max-bin is 64 KiB; never `read_file` a >64 KiB module
  in maker code (already handled via `hash_file`); keep that discipline.
- All `--module-only` / skip logic must stay gated on `tgt.module_only_get()` so the
  whole-program path (the suite, normal builds) is byte-unchanged.

## 8. File / line index (head-start)
- `src/semantic/walk_module.cst` — `visit_use_module` (passes 1–6), `analyze`
  (entry, sets module-only prefix + `record_dep`); pass-6 body analysis ~line 290.
- `src/ir/gen_program.cst` — `gen_ir`; NK_USE recursion ~line 534; `no_main`
  reachability ~line 634–644.
- `src/semantic/walk.cst:100+` — Layer-2 `with imut` AST interpreter (the folder).
- `src/parser/top.cst:299` — parse-time `if (CONST) use` dead-strip.
- `src/semantic/generics.cst` — `mangle_generic` (89), `instantiate_fn` (495),
  `ast_tail` append (534).
- `src/codegen/emit_driver.cst` — `_belongs_to_target_module` / `_global_belongs`
  emission filters (Stage 1).
- `src/codegen/target.cst` — `g_module_only` / `g_module_prefix` / `g_emit_deps_path`.
- `src/interface.cst` — `emit_csti` / `write_type` (Approach B starting point).
- `caustic-maker/exec/build.cst` — `run_pipeline_incremental` (the driver).
- `caustic-maker/core/common.cst` — `hash_file`, `mangle_path`, `u64_to_str`.

## 9. Definition of done
- First/clean `caustic-mk build caustic --incremental` < ~1.5 s (from ~5.7 s).
- Oracle B byte-identical; `./caustic-mk test` 5/5 (fixpoint included).
- Whole-program build path byte-unchanged (all new logic gated on module-only).
- Dev-loop still ~150–200 ms.
