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
- **One build-info for the whole link.** Each module's `S_BUILDINFO` points at
  the same `LF_BUILDINFO` (built from the first source), and the IPI's
  `LF_FUNC_ID` scopes are 0. Splitting it means per-module build infos and
  per-function scopes; the modules, their symbol streams, sources and line
  tables are already per-object.

## Exceptions (`std/exceptions.cst`)

`try`/`catch`/`throw` are the stdlib's, not the language's: `begin` captures
the caller's machine state (a naked function — with a prologue it would save
its own frame), `enter` registers it as a handler, `throw` runs the guards it
unwinds past and restores the innermost handler's state. No `_Unwind_*`, no
libc, no libgcc.

Verified on Linux x86_64 (`-O0/-O1/-O2`), Linux AArch64 (qemu) and Windows
x86_64 (wine); `tests/exceptions_test.cst` pins the depth, the value, nesting,
guard order, rethrow and that the resuming frame's own locals survive.

Known limits, deliberate:

- **`defer` does not run on a throw.** It is compiler-inlined at each `return`
  (`src/ir/gen_expr.cst:emit_deferred_calls`) and there is no addressable "run
  my defers" thunk for a throw to call. `guard`/`release` is the
  exception-safe form: the guard runs on the unwind, the paired `defer
  release` pops it on the normal path. Closing this needs the compiler, and
  the design is settled enough to state: emit a cleanup thunk per
  scope-with-defers, register it at scope entry, and have `throw` run it. Two
  open questions — the thunk must address the ORIGINAL frame's locals (either
  a naked thunk with the runtime setting rbp before the call, which needs the
  deferred call's arguments to be simple, or a new `IR_ADDR_BASE` op so a
  normal thunk can take the frame as a parameter), and mem2reg must be told
  not to promote a local whose address a deferred call reads.
- **The handler stack is a plain global, not per-thread.** The stdlib has no
  TLS to hang it off: nothing initialises a thread pointer — `clone` is handed
  `&Thread` (which nothing reads) and the main thread has none at all, so
  reading `fs:` would fault before it could tell. A throw that crosses threads
  is DETECTED (the handler must be an ancestor on this stack) rather than
  supported. Real per-thread needs a thread pointer for every thread, which is
  its own project.
- **Handlers belong to one runtime image.** An executable and a `.so` that
  each statically embed `std/exceptions.cst` have separate heads: a throw in
  the library cannot see a handler in the executable. On Linux x86_64, build
  every participant with `-lcaustic` and use the canonical
  `use "std/exceptions.cst"` import to share one `libcaustic.so`. This path is
  verified at `-O0/-O1/-O2`, including the standalone ELF linker.
- **No target without inline asm.** A wasm build that uses it fails in the
  backend with "inline asm is not supported on wasm" — loud, but not a
  diagnostic this module chooses.
- **The pattern is explicit.** Without macros, the catch is an `if` on
  `begin`'s result; `enter` must precede anything that can throw, and a throw
  with nothing entered is an uncaught panic.
- **The try pays on the hot path.** `begin` is a call plus ten stores and a
  164-byte frame, and the compiler never inlines a function containing asm
  (`src/ir/opt_loop.cst:inline_has_asm`), so a try cannot be optimized at all.
  Zero cost needs tables: the compiler emits an LSDA and a landing pad per
  frame, and `throw` walks `.eh_frame` instead of a pushed stack. We already
  emit half of it — `.eh_frame` with a `"zR"` CIE, no `P` (personality) and no
  `L` (LSDA) — so the work is the LSDA, a personality, and a DWARF CFI
  interpreter written in Caustic (the shape of `libgcc/unwind.inc` and
  `std/src/sys/personality/dwarf/eh.rs`), with no libgcc to lean on.
- **A backtrace is addresses only.** `std/exceptions.cst:backtrace` walks the
  frame-pointer chain; naming the frames needs a loadable name table, since
  `.symtab` and `.debug_*` are in the file and not in the loaded image.

## Debug info (DWARF, ELF `-g`)

- **No exception syntax in the language.** `try`/`catch`/`throw` are not
  keywords — exceptions live in the stdlib instead (see below), so there is
  no `_Unwind_*` runtime to link. What a *foreign* unwinder needs is present:
  `.eh_frame` + `.eh_frame_hdr` (with a `PT_GNU_EH_FRAME` header) in both
  executables and `--shared` objects, and a C++ exception thrown from a
  Caustic `.so` callback unwinds through it and reaches the caller's `catch`.
- **CFI covers ELF and PE.** ELF carries `.eh_frame` (x86_64 caf 1/ra rip,
  AArch64 caf 4/ra x30, pcrel FDEs in a shared object); PE carries
  `.pdata`/`.xdata` (one `RUNTIME_FUNCTION` + `UNWIND_INFO` per function,
  `llvm-readobj --unwind` clean).
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

## `__compile_error` (intentional, but narrower than it looks)

- **`__compile_error` fires when its enclosing function is EMITTED, not when
  it is called.** `src/codegen/emit_inst.cst:1405` aborts the build on
  reaching the marker, and its comment says the marker "survived dead-branch
  elim + function DCE" — true for a dead branch, but function DCE does not
  remove an uncalled function, so a helper that is nothing but a
  `__compile_error` aborts as soon as its module is imported. Verified:
  `fn unused() as void { __compile_error("boom"); }` in a file that never
  calls it still fails the build.
- **So the check belongs at the call site, inside the target guard** — that is
  what makes the branch dead and lets it be eliminated:
  `if (__target_is_causticos == 0) { __compile_error("..."); }`
  (`std/causticos/*.cst`), or `__compile_error` inline (`std/io.cst`).
- `std/errors.cst` carried seven such helpers (`unsupported`, `no_vfs`,
  `no_heap`, `no_network`, `no_processes`, `not_implemented`, `todo`), none of
  which had a single call site, and their presence made the whole module
  unimportable — `panic` and `assert` could not be used either. They were
  removed. If a stub is wanted at a site, write it there.

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

## Project health

- **CI** (`.github/workflows/ci.yml`) fetches the latest release as the
  bootstrap seed, runs `tools/precommit.sh` (self-host fixpoint at
  `-O0/-O1/-O2` + the suite), then the two opt-in suites. The seed has to be
  recent enough to compile HEAD; the build fails loudly if it is not.
- **`tools/check-cross.sh`** (qemu/wine/gcc) and **`tools/check-debuginfo.sh`**
  (llvm-dwarfdump/llvm-pdbutil/gdb/g++) are opt-in and SKIP a missing tool, so
  neither can fail for an environment reason — but they are what pins the
  cross-target ABI and the debug-info work, so run them before a release.
- **Thin test suite.** ~16 test files against ~58k lines of `.cst`. No golden
  codegen tests, no negative/error-message tests, no fuzzing.
