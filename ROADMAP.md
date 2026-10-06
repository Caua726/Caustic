# Roadmap & Known Gaps

A single place to track what is deliberately incomplete in the toolchain, so
the limitations live in the repo instead of in someone's head. Each entry
points at the source so it is easy to pick up. Status as of 2026-06-14.

## Windows backend gaps

The Windows (`--target=windows-x86_64`) port is functional but a few facades
still fall back or no-op where the Linux path is complete.

- **`std/process.cst` — `capture()` is Linux-only.** Spawning + reading a
  child's stdout needs `CreatePipe` + `STARTUPINFO` wiring on Windows. `spawn`
  / `wait` already work via `CreateProcessA` + `WaitForSingleObject`.
- **`std/env.cst:180` — `execvp` returns -1 on Windows.** `CreateProcessA` is
  not yet wired into the `execvp` shim.
- **`std/io.cst` — directory ops unimplemented on Windows.** `list_dir`
  (~1004), `is_dir` (~1074), and `get_cwd` (~1085) need the
  `FindFirstFileA`/`FindNextFileA`/`GetCurrentDirectoryA` path; today they are
  Linux-syscall only.

## Assembler gaps

- **`caustic-assembler/main.cst:175` — native `windows-x86_64` target not
  implemented.** PE/COFF objects currently come from the compiler+linker path,
  not from `caustic-as` directly.
- **`caustic-assembler/output/coff.cst:26` — COFF object writer not
  implemented ("Fase 2").**

## PE debug info

- **TPI/IPI hash streams are absent.** The PDB carries no name-based type
  hash streams (`hashStreamIndex = 0xFFFF`), only the type records. MSVC's
  hash algorithm by type name was not reproduced — a wrong hash is worse than
  none — so `llvm-pdbutil` and address-based consumers work while name-indexed
  type lookup does not. `caustic-linker/pe/pdb_writer.cst:_emit_tpi_header`.
- **One source file per PE module.** The module stream's STRINGTABLE/FILECHKSMS
  carry the first `.cstdebug` file; a link of several objects with different
  source paths still reports one. `caustic-linker/pe/pe_debug.cst:build_module_stream`.

## Semantic

- **`src/semantic/types.cst:608` — `TODO(safety)`.** A strict pointer/type
  check (introduced in `f8d2c0a`) was relaxed because it rejected valid code;
  revisit with a more precise rule.

## stdlib placeholder helpers (intentional)

- **`std/errors.cst:43,47` — `error_not_implemented()` / `error_todo()`** abort
  the build via `__compile_error`. These are intentional stubs meant to be
  called at sites that are not done yet; listed here so they are not mistaken
  for finished functionality.

## Language: future work (design exists)

- **`with imut` on functions — comptime "Layer 2".** Extend the `imut`
  qualifier from variables to functions: a pure (no syscall / extern / mut
  globals) function called with all-literal args is evaluated by a tree-walking
  interpreter at semantic time and substituted as a literal. Variables/globals
  already have this via init-expression const-prop; functions do not. Estimated
  ~500 lines (parser annotation + semantic purity check + AST interpreter).
  Full design notes in the comptime design discussion referenced from
  `CLAUDE.md`. Defer until a concrete use case appears (lookup-table
  generation, compile-time string hashing, etc).

## Project health (deferred — owner deprioritized)

Recorded for honesty; not currently being worked on.

- **No CI.** The bootstrap fixpoint (`gen2 == gen3`, byte-for-byte) and
  `caustic-mk test` are only run manually. A workflow that builds all five
  targets and runs the fixpoint on every push would protect self-hosting.
- **Thin test suite.** ~16 test files against ~58k lines of `.cst`. No golden
  codegen tests, no negative/error-message tests, no fuzzing.
