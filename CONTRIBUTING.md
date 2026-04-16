# Contributing to Caustic

First off — thanks for being here. Caustic is a self-hosted compiler that goes from source to native x86_64 Linux binary with zero external dependencies. If that sounds interesting, you're in the right place.

## Philosophy

Before contributing, understand what Caustic values:

- **No magic, no implicit.** Every operation is visible. No hidden allocations, no implicit conversions, no runtime surprises.
- **Speed and simplicity.** If a feature adds overhead or complexity without clear benefit, it doesn't go in.
- **Zero dependencies.** No libc, no LLVM, no runtime. Just Linux syscalls.
- **Real professional language.** Not a toy. Caustic is designed to build real software — compilers, operating systems, servers.

If your contribution aligns with these, it's probably welcome.

## What we accept

- Bug fixes with a test case
- Performance improvements with benchmarks
- New stdlib modules or functions that are generally useful
- Documentation improvements
- LSP/tooling improvements
- New language features **with an approved issue first**

## What we don't accept

- **Purely cosmetic changes** — reformatting code, renaming things "for consistency" without functional improvement
- **External dependencies** — no libc, no linking to C libraries, no LLVM, no cargo/npm/whatever
- **Implicit/magic features** — if the user can't see what the code does by reading it, it's too implicit
- **Slow features** — anything that adds measurable overhead to compilation or runtime without justification
- **Speculative features** — "this might be useful someday" is not a reason. Open an issue first, discuss, get approval, then implement
- **Abstraction for abstraction's sake** — three lines of clear code beats one line of clever code

## Getting started

### Prerequisites

- Linux x86_64 (the only supported platform)
- The Caustic toolchain installed (compiler + assembler + linker)

See [docs/getting-started.md](docs/getting-started.md) for installation instructions.

### Building from source

```bash
git clone https://github.com/Caua726/caustic
cd caustic

# Build everything (requires caustic already installed)
./caustic-mk build caustic
./caustic-mk build caustic-as
./caustic-mk build caustic-ld
```

If you don't have `caustic` installed yet, download a release first from [GitHub Releases](https://github.com/Caua726/caustic/releases), install it, then build from source.

### Running tests

```bash
bash tests/run_tests.sh
```

This runs:
1. All unit tests in `tests/*.cst`
2. 4-generation bootstrap of the compiler (gen3 must equal gen4)
3. Bootstrap of the assembler and linker
4. Full toolchain rebuild and verification

Everything runs in a temp directory and cleans up after itself. All tests must pass before merging.

### Project structure

```
src/
  lexer/       — tokenization
  parser/      — recursive descent → AST
  semantic/    — type checking, symbol tables
  ir/          — virtual register IR, optimization
  codegen/     — register allocation, x86_64 assembly output

caustic-assembler/  — .s → .o (ELF relocatable)
caustic-linker/     — .o → executable (ELF static/dynamic)

std/           — standard library (io, mem, string, linux, etc.)
lsp/           — language server (hover, completion, goto-def, etc.)
docs/          — language reference, tutorials, stdlib docs
tests/         — test files and test runner
examples/      — example programs
```

## How to contribute

### 1. Find something to work on

- Check [Issues](https://github.com/Caua726/caustic/issues) for bugs and feature requests
- Issues labeled `good-first-issue` are a good starting point
- If you want to add a feature, **open an issue first** to discuss

### 2. Fork and branch

```bash
git fork  # or fork on GitHub
git checkout -b my-feature
```

Branch names don't matter. Name them whatever makes sense to you.

### 3. Make your changes

Follow the checklist below.

### 4. Submit a Pull Request

- Fork the repo, push your branch, open a PR against `main`
- Describe what you changed and why
- Link the issue if there is one
- PRs are reviewed by maintainers

## Pre-merge checklist

Every PR must satisfy all applicable items. If something doesn't apply, say why.

### Code

- [ ] Compiles: `./caustic-mk build caustic` succeeds
- [ ] Tests pass: `bash tests/run_tests.sh`
- [ ] Bootstrap: at minimum gen2, ideally gen4 for risky changes
- [ ] Existing examples still build and run

### Documentation

If you added or changed **language syntax**:
- [ ] Updated `CLAUDE.md` with examples
- [ ] Updated `docs/language.md` with syntax, description, examples
- [ ] Updated `docs/tutorial.md` if it affects how beginners learn

If you added or changed **stdlib functions/modules**:
- [ ] Updated `docs/stdlib/<module>.md`
- [ ] Included signature, description, parameters, return value, example

### LSP / Tooling

If you added a **new keyword or modifier**:
- [ ] Added hover docs in `lsp/keywords.cst`
- [ ] Verified hover works in VS Code

If you changed **declaration syntax**:
- [ ] Verified the LSP symbol scanner (`lsp/symbols.cst`) handles it
- [ ] Verified outline, completion, go-to-definition still work

If you changed **codegen output**:
- [ ] Verified `caustic-as` handles the new assembly
- [ ] Verified `caustic-ld` handles any new ELF sections/symbols
- [ ] Checked with `readelf -S`, `readelf -s`, `readelf -r`

### VS Code Extension

If you changed the LSP:
- [ ] Rebuilt: `caustic lsp/main.cst -o caustic-lsp`
- [ ] Tested in VS Code with window reload
- [ ] If publishing: bumped version, rebuilt `.vsix`

The extension lives at [github.com/Caua726/caustic-lsp](https://github.com/Caua726/caustic-lsp).

## Commit style

```
area: short description

Examples:
  lexer: handle @ character
  parser: add with section clause  
  lsp: hover for section keyword
  docs: add tutorial for enums
  stdlib: add string.split_at function
```

Areas: `lexer`, `parser`, `semantic`, `ir`, `codegen`, `assembler`, `linker`, `lsp`, `docs`, `stdlib`, `tests`, `examples`

Keep messages short and lowercase. No emojis. No "fix: fix the fix".

## Learning Caustic

If you're new to the language:

- [Getting Started](docs/getting-started.md) — install and write your first program
- [Tutorial](docs/tutorial.md) — learn the language step by step
- [Language Reference](docs/language.md) — complete syntax reference
- [Standard Library](docs/stdlib/) — all available modules and functions

## Questions?

Open an issue. There's no wrong question if you're trying to contribute.
