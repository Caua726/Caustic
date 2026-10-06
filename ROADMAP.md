# Roadmap & Known Gaps

A single place to track what is deliberately incomplete in the toolchain, so
the limitations live in the repo instead of in someone's head. Each entry
points at the source so it is easy to pick up. Status as of 2026-06-14.

## Windows backend gaps

The Windows (`--target=windows-x86_64`) port is functional. Directory listing,
`getcwd`, `is_dir`, `capture`, `execvp`, sockets and terminal input all have
Windows backends now.

- **No `chdir` binding on Windows.** `caustic-mk` must run from the project
  root there. A `SetCurrentDirectoryA` binding would close it.

## Assembler gaps

- **`caustic-assembler/main.cst:175` — the `caustic-as` CLI takes the input
  file first.** `caustic-as file.s --target=windows-x86_64` works; a leading
  `--target=` is read as the file name. `output/coff.cst`'s writer is
  implemented and verified (`caustic-as f.s --target=windows-x86_64` produces
  a COFF object lld-link links and wine runs).

## PE debug info

- **TPI/IPI hash streams are absent.** The PDB carries no name-based type
  hash streams (`hashStreamIndex = 0xFFFF`), only the type records. MSVC's
  hash algorithm by type name was not reproduced — a wrong hash is worse than
  none — so `llvm-pdbutil` and address-based consumers work while name-indexed
  type lookup does not. `caustic-linker/pe/pdb_writer.cst:_emit_tpi_header`.
- **One source file per PE module.** The module stream's STRINGTABLE/FILECHKSMS
  carry the first `.cstdebug` file; a link of several objects with different
  source paths still reports one. `caustic-linker/pe/pe_debug.cst:build_module_stream`.

## Debug info (DWARF, ELF `-g`)

- **No exception runtime.** `.eh_frame` + `.eh_frame_hdr` are complete and a
  runtime unwinder reads them (`eu-stack` unwinds a live process), but nothing
  links `libgcc`'s `_Unwind_RaiseException`, so `try`/`catch` has no runtime
  behind it. `caustic-linker/elf/eh_frame.cst`.
- **CFI is x86_64-ELF only.** PE uses `.pdata`/`.xdata`; the AArch64 assembler
  ignores the `.cfi_*` directives, so `linux-aarch64` objects get no CFI.
- **Locations are static per function.** A local/parameter is described as a
  fixed `DW_OP_fbreg` off rbp for the whole function, so a value still in a
  register during the prologue (before its spill) is not located — a breakpoint
  at the first line of a function may read a parameter as zero until the spill.
  A location list would fix it.
- **One source file per link.** The line table's file table carries the first
  object's source; a multi-object link reports one file.
- **Enums are not described.** Structs, pointers and arrays are; Caustic's
  tagged-union enums have no DWARF representation yet.
- **`.cstdebug` line offsets are object-local.** Like `.cstcfi` was, they are
  not remapped when dead-code elimination repacks an object's `.text`; the `-g`
  build avoids it by taking the serial whole-program path.
- **`-g` disables the parallel per-module emitter.** Debug builds take the
  serial whole-program path (the body AST must stay live for `.cstlocals`).

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
