# Documentation Overhaul — Design Spec

**Date:** 2026-03-25
**Status:** Approved
**Scope:** Full documentation maintenance, correction, and restructuring

## Context

The Caustic documentation is severely outdated after rapid development from v0.0.1 to v0.0.13. The compiler evolved from C to self-hosted Caustic, gained an optimizer (-O1), build system (caustic-mk), 8 new stdlib modules, indirect calls, submodule imports, and many other features — none of which are reflected in the docs. Critical errors exist (wrong match syntax, wrong opcode counts, claims features don't exist when they do).

## Decisions

- **Language:** English (all docs)
- **Structure:** Hybrid — guide consolidated (~15 files), reference stays granular (~40+ files)
- **Audience:** Guide for language users, reference for compiler contributors
- **README style:** Balanced — impactful description, features, benchmarks, install, links
- **Approach:** Bottom-up — fix critical errors first, add missing content, then consolidate and rewrite README
- **Rewrite policy:** Selective — rewrite what's broken, consolidate guide, update reference in-place

## Diagnosis Summary

### P0 — Code-breaking errors (3 files)
- `docs/guide/enums/simple-enums.md` and `tagged-unions.md`: Match syntax uses `Color.Red => { ... }` instead of correct `case Red { ... }`. Also missing required type name — correct is `match TypeName (expr)`, docs show `match (expr)`. Also missing `else { ... }` default clause.
- `docs/guide/advanced/function-pointers.md`: States "Caustic has no syntax to call through function pointers" — `call()` exists since v0.0.13

### P1 — Wrong information (~10 files)
- IR opcodes: docs say 46, actual is 48 (missing IR_CALL_INDIRECT, IR_VA_START)
- Node kinds: docs say 50-51, actual is 53
- Type kinds: docs say 18, actual is 19 (missing TY_FN)
- Register allocation: docs say 3 allocatable registers, actual is 10
- docs describe only linear scan, graph coloring (-O1) not mentioned
- compiler-flags.md documents 4 of ~15 flags
- Build commands reference `make` everywhere — project uses `caustic-mk`
- README capabilities mark optimization as incomplete — -O1 exists since v0.0.10

### P2 — Missing features (17 undocumented features)
- `call()` indirect function calls
- `only` keyword in use statements
- Submodule dot-notation (`mod.submod.func()`)
- `-O0`/`-O1` optimization flags
- `--profile`, `--cache`, `--max-ram`, `--path` flags
- `-o output` full pipeline (compile + assemble + link)
- Typed fn_ptr (TY_FN with parameter/return type info)
- f32 literal narrowing
- Number literals with underscores, hex, binary, octal
- Block comments `/* */`
- caustic-mk build system (no user-facing guide at all)
- 8 stdlib modules without any docs (math, sort, map, random, net, time, env, arena)
- mem submodules (freelist, bins, arena, pool, lifo)

### P3 — Outdated but not wrong
- stdlib-roadmap.md lists implemented modules as "to do"
- examples/README.md lists 8 examples, Causticfile has 12 targets
- CLAUDE.md stdlib list shows 5 modules, actual is 15+

## Phase Plan

Execution order: F1 -> F2 -> F3 -> F5 -> F6 -> F4

### Phase 1: Fix P0 — Code-breaking errors

**Files to rewrite (3):**

1. `docs/guide/enums/simple-enums.md` — Fix all match examples: `Color.Red => { ... }` → `case Red { ... }`, add type name `match Color (c)` not `match (c)`, document `else` default clause
2. `docs/guide/enums/tagged-unions.md` — Same match syntax fix, fix destructuring examples, add type name, add `else` clause
3. `docs/guide/advanced/function-pointers.md` — Remove "no syntax to call" claim, document `call(fn_ptr, args)`, document typed fn_ptr (TY_FN)

### Phase 2: Fix P1 — Wrong information

**Files to update (~10):**

1. `docs/reference/overview.md` — Update: 48 opcodes, 53 node kinds, 19 type kinds
2. `docs/reference/ir/opcodes.md` — Add IR_VA_START (46), IR_CALL_INDIRECT (47), update count to 48
3. `docs/reference/parser/node-kinds.md` — Add NK_TYPE_ALIAS (51), NK_CALL_INDIRECT (52), update count to 53
4. `docs/reference/codegen/available-registers.md` — Rewrite: 10 allocatable registers (rbx, r12, r13, r14, r15, rsi, rdi, r8, r9, r10), caller-saved vs callee-saved distinction
5. `docs/reference/codegen/register-allocation.md` — Rewrite: document both linear scan (-O0) and graph coloring (-O1, Iterative Register Coalescing). Fix "insertion sort" claim (code uses merge sort). Update AllocCtx struct (missing fields: va_save_offset, call_positions, call_count, active_idx, used_callee_mask)
6. `docs/guide/compiler-flags.md` — Rewrite completely with all ~15 flags: `-c`, `-o`, `-O0`, `-O1`, `-debuglexer`, `-debugparser`, `-debugir`, `--profile`, `--cache`, `--max-ram`, `--path`, `--emit-interface`, `--emit-tokens`, `--emit-ast`, `--emit-ir`, `-l<lib>`
7. `docs/reference/lexer/token-types.md` — Update token count
8. `docs/reference/lexer/keywords.md` — Add `only`, `call`, `type` if missing
9. `docs/reference/type-system/` — Add TY_FN documentation
10. `docs/reference/assembler/overview.md` — Fix heap sizing formula if inconsistent

### Phase 3: Add missing documentation

**New files to create (~12):**

Reference stdlib docs (8 new):
1. `docs/reference/stdlib/math.md` — abs, min, max, clamp, pow, gcd, lcm, sign, div_ceil, is_power_of_two, align_up/down
2. `docs/reference/stdlib/sort.md` — quicksort, heapsort, mergesort, insertion/bubble sort, fn_ptr comparators, i32/i64 variants
3. `docs/reference/stdlib/map.md` — MapI64 (splitmix64), MapStr (FNV-1a), open addressing, linear probing, iterator API
4. `docs/reference/stdlib/random.md` — Rng struct, xoshiro256**, rng_init, rng_range (rejection sampling), rng_shuffle, global convenience functions
5. `docs/reference/stdlib/net.md` — Addr struct, ip4(), tcp_listen/accept/connect, udp_bind/send/recv, send_all/recv_all, PollFd, socket options
6. `docs/reference/stdlib/time.md` — now_ns/us/ms/s, unix_time, sleep_s/ms/us/ns, elapsed_ns/us/ms/s
7. `docs/reference/stdlib/env.md` — init(), argc(), argv(), arg(), getenv(), getenv_string(), env_count(), env_entry()
8. `docs/reference/stdlib/arena.md` — create(), destroy(), reset(), alloc(), alloc_str(), alloc_zero(), save()/restore(), used()/remaining()/capacity()

Reference codegen (1 new):
9. `docs/reference/codegen/optimizations.md` — addr_fusion, SSA mem2reg, constant/copy propagation, store-load forwarding, fold immediates, LICM, strength reduction, DCE, function inlining, peephole optimizer

Reference build system (2 new):
10. `docs/reference/build-system/causticfile.md` — Format spec: name, version, author, license, target blocks (src, out, flags, depends, depend with git URL + tag), system libs, script blocks
11. `docs/reference/build-system/commands.md` — build, run, test, clean, init commands

Updates to existing:
12. `docs/reference/stdlib/mem.md` — Add submodules section: freelist (O(n) alloc/free, coalescing), bins (O(1), slab-based, bitmap), arena (O(1) bump, same as std/arena.cst), pool (O(1) fixed-size), lifo (O(1) stack). Note: std/mem/arena.cst and std/arena.cst are duplicates — document both paths
13. `docs/guide/modules/use-statement.md` — Add `only` keyword syntax, submodule dot-notation
14. `docs/guide/types/integers.md` (or equivalent) — Add hex/binary/octal literals, underscore separators
15. Update `docs/stdlib-roadmap.md` — Mark all implemented modules as done, or delete file

### Phase 4: Consolidate guide (last)

**New consolidated files (15):**

```
docs/guide/
  getting-started.md     <- NEW (hello world, install, basic pipeline)
  variables.md           <- MERGE variables/{declaration,mutability,globals,shadowing,initialization}
  types.md               <- MERGE types/{integers,unsigned,floats,bool,char,void,string-type,type-coercion,negative-literals} + ADD hex/bin/oct/underscores
  functions.md           <- MERGE functions/{declaration,parameters,return-values,recursion,variadic}
  control-flow.md        <- MERGE control-flow/{if-else,while,for,do-while,break-continue,match,destructuring}
  operators.md           <- MERGE operators/{arithmetic,comparison,logical,bitwise,compound-assignment,short-circuit,operator-precedence}
  structs.md             <- MERGE structs/{declaration,field-access,packed-layout,nested-structs,struct-pointers,struct-return}
  enums.md               <- MERGE+REWRITE enums/{simple-enums,tagged-unions,construction,enum-layout}
  generics.md            <- MERGE generics/{syntax,functions,structs,enums,monomorphization,constraints}
  memory.md              <- MERGE memory/{pointers,address-of,dereferencing,pointer-arithmetic,arrays,array-of-structs,heap-allocation,stack-vs-heap}
  modules.md             <- MERGE modules/{use-statement,aliases,relative-paths,circular-imports,multi-file-projects,extern-fn} + ADD only, submodules
  impl.md                <- MERGE impl/{methods,associated-functions,desugaring,generic-impl,method-call-syntax}
  advanced.md            <- MERGE advanced/{cast,sizeof,inline-asm,syscalls,defer,defer-patterns,function-pointers,ffi,ffi-struct-passing,compatcffi,string-escapes,char-literals} + ADD call(), typed fn_ptr
  compiler-flags.md      <- Already rewritten in F2
  build-system.md        <- NEW (caustic-mk guide for users)
```

**Files to delete after consolidation (~80):**
All granular files in `docs/guide/variables/`, `docs/guide/types/`, `docs/guide/functions/`, `docs/guide/control-flow/`, `docs/guide/operators/`, `docs/guide/structs/`, `docs/guide/enums/`, `docs/guide/generics/`, `docs/guide/memory/`, `docs/guide/modules/`, `docs/guide/impl/`, `docs/guide/advanced/`.

The subdirectories themselves are deleted (empty after file removal).

### Phase 5: Rewrite README.md

**Structure:**
```markdown
# Caustic

One-line: Self-hosted x86_64 Linux compiler — no LLVM, no libc, no runtime.

## Overview
- 3-4 bullets: self-hosted toolchain, zero deps, -O1 optimizer, beats LuaJIT
- Hello world example (code + how to build + run)

## Features
Grouped bullet list (no checkboxes):
- Language: types, generics, enums+match, impl, defer, FFI, call(), modules+only
- Stdlib: 15 modules listed with one-line descriptions
- Toolchain: compiler, assembler, linker, build system (all self-hosted)
- Optimization: -O0 linear scan, -O1 graph coloring + SSA + 8 passes

## Performance
Compact table: Caustic -O1 vs GCC -O0 vs LuaJIT (v0.0.13 numbers)

## Quick Start
- Install from tarball (4 lines)
- caustic-mk workflow
- Manual pipeline (caustic + caustic-as + caustic-ld)

## Standard Library
Table: module | description (1 line each)

## Build from Source
Bootstrap caustic-mk, then caustic-mk build targets

## Documentation
Link to docs/README.md

## License
```

### Phase 6: Update CLAUDE.md and docs/README.md

**CLAUDE.md updates:**
- Build Commands: replace `make` with `caustic-mk build <target>`, keep manual pipeline as alternative
- Stdlib list: expand to all 15+ modules with submodules
- Update counts: 80+ tokens, 53 nodes, 48 opcodes
- Add flags: -O0/-O1, --profile, --cache, --max-ram, --path, -o
- Add syntax: call(), only, typed fn_ptr
- Language Syntax section: add call() example, only example

**docs/README.md updates:**
- Restructure TOC for consolidated guide (~15 links instead of ~80)
- Add "Build System" section with links
- Add all stdlib modules in Reference > Standard Library
- Add "Optimization" in Reference > Codegen
- Remove dead links to deleted granular files

**docs/stdlib-roadmap.md:**
- Delete file (everything on the roadmap is implemented)

## Quantified Scope

| Phase | Files touched | Files created | Files deleted |
|-------|--------------|---------------|---------------|
| F1: Fix P0 | 3 | 0 | 0 |
| F2: Fix P1 | ~10 | 0 | 0 |
| F3: Add missing | ~5 | ~12 | 0-1 |
| F5: README | 1 | 0 | 0 |
| F6: CLAUDE.md + index | 3 | 0 | 0-1 |
| F4: Consolidate | 0 | 15 | ~80 |
| **Total** | **~22** | **~27** | **~80** |

Net result: from ~97 doc files to ~55, all accurate and up-to-date.
