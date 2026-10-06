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
- **One PE module for the whole link.** A multi-file Windows build puts every
  object's symbols in a single module named after the first source. Splitting
  it means N module streams in the MSF, an N-entry ModuleInfo substream and
  per-function LF_FUNC_ID scopes in the IPI — the MSF layout, DBI, TPI/IPI and
  module builder change together. `caustic-linker/pe/{pe_debug,pdb_writer}.cst`.
- **`llvm-pdbutil dump --modules` reports "stream too short".** The DBI's
  ModuleInfo substream is one byte short of what the parser expects, after the
  module entry; it predates the enum work (`hello.pdb` shows it too) and
  llvm-pdbutil still lists the module. `caustic-linker/pe/pdb_writer.cst:_emit_dbi`.

## Debug info (DWARF, ELF `-g`)

- **No exception runtime.** `.eh_frame` + `.eh_frame_hdr` are complete and a
  runtime unwinder reads them (`eu-stack` unwinds a live process), but nothing
  links `libgcc`'s `_Unwind_RaiseException`, so `try`/`catch` has no runtime
  behind it. `caustic-linker/elf/eh_frame.cst`.
- **CFI is ELF only.** PE uses `.pdata`/`.xdata`, which are not emitted. Both
  ELF architectures now carry CFI: x86_64 (caf 1, ra rip) and AArch64 (caf 4,
  ra x30) each get their own CIE.
- **Locals are stack-homed at `-g`.** A `let` is a fixed `DW_OP_fbreg` off rbp,
  which is correct because `-g` skips mem2reg (otherwise the promoted value
  would live in a register and the slot would go stale). The cost is that a
  debug build runs without that pass. Parameters get a location list
  (register before the prologue end, frame slot after) — except in a
  single-line function, where the prologue end is not distinguishable from the
  body, so it falls back to the frame slot. A location list per live range
  would restore the optimization and close the single-line case.
- **`.cstdebug` line offsets are object-local.** Not remapped when dead-code
  elimination repacks `.text`, but codegen emits only reachable functions, so
  the offsets match what is emitted; the `-g` ELF build takes the serial
  whole-program path regardless. No stale row has been observed on either
  target.
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

## Language: implemented

- **`with imut` on functions — comptime.** A function annotated `with imut`
  whose arguments all fold to literals is evaluated at semantic time and the
  call is rewritten to the resulting `NK_NUM`; codegen never sees the call.
  The parser sets the flag (`src/parser/top.cst`), `try_fold_imut_fn` /
  `imut_eval_expr` / `imut_exec_stmt` interpret the body (`src/semantic/walk.cst`),
  and the fold happens at the call site. A call with a runtime argument (or a
  callee that is not `with imut`) stays a real call. Verified: `sq(7)` and a
  loop-based `fib(10)` fold to constants, `use(x)` with a runtime `x` does not.
  The purity check is by construction — a non-pure body simply fails to fold.

## Project health (deferred — owner deprioritized)

Recorded for honesty; not currently being worked on.

- **No CI.** The bootstrap fixpoint (`gen2 == gen3`, byte-for-byte) and
  `caustic-mk test` are only run manually. A workflow that builds all five
  targets and runs the fixpoint on every push would protect self-hosting.
- **Thin test suite.** ~16 test files against ~58k lines of `.cst`. No golden
  codegen tests, no negative/error-message tests, no fuzzing.
