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

### Stage 1 — module-only emission (compiler)
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

### Stage 2 — maker orchestration (incremental driver)
Teach the maker to drive separate compilation:
- Discover the module graph (the compiler can emit a dep list, or the maker
  scans `use` transitively).
- Per module: `hash(source)` → `.caustic/obj/<hash>.o`. If present, reuse;
  else `caustic --module-only=M -o M.s && caustic-as M.s && cache M.o`.
- Link all `.o` → binary (with DCE).
- Dev-loop win: change one module → recompile just that `.o` + relink.
**Risk:** first/clean build still pays redundant semantic per module (see Stage 3).

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
