# caustic-ld

A self-hosted static and dynamic linker for the Caustic toolchain, written entirely in Caustic.

Takes `.o` ELF object files produced by `caustic-as` and outputs standalone Linux x86_64 executables — no GCC or GNU ld required.

## Build

```bash
# From the repo root:
make linker    # builds ./caustic-ld
```

## Usage

```bash
# Single object:
./caustic-ld program.cst.s.o -o program

# Multi-object:
./caustic-ld main.cst.s.o lib.cst.s.o -o program

# Dynamic linking (libc):
./caustic-ld program.cst.s.o -lc -o program

# Custom base address:
./caustic-ld program.cst.s.o --base 0x600000 -o program
```

## Full pipeline

```bash
./caustic main.cst              # compile to .s
./caustic-as main.cst.s         # assemble to .o
./caustic-ld main.cst.s.o -o main   # link to executable
./main
```

## Features

- **Static linking**: single and multi-object, resolves all relocations (R_X86_64_PC32, R_X86_64_PLT32)
- **Dynamic linking**: `-lc`, `-lm`, etc. Generates PLT/GOT, .dynamic, .dynsym, .hash, .rela.plt, PT_INTERP
- **Eager binding**: all dynamic symbols resolved at startup (DT_BIND_NOW)
- **_start stub**: auto-generated entry point that calls `main` and exits via syscall
- **ELF output**: proper program headers, section layout, symbol table

## Architecture

```
.o files → elf_reader.cst → linker.cst → elf_writer.cst → executable
```

| File | Purpose |
|------|---------|
| `elf_reader.cst` | Parses ELF object files (sections, symbols, relocations) |
| `linker.cst` | Symbol resolution, relocation, PLT/GOT generation, dynamic metadata |
| `elf_writer.cst` | Writes static or dynamic ELF executables |
| `main.cst` | CLI parsing and orchestration |

## Static ELF layout

```
ELF header
Program headers (2): PT_LOAD R+X, PT_LOAD R+W
.text        (code)
.rodata      (string literals)
.data        (initialized globals)
.symtab / .strtab
```

## Dynamic ELF layout

```
ELF header
Program headers (4): PT_LOAD R+X, PT_LOAD R+W, PT_INTERP, PT_DYNAMIC
.interp      (/lib64/ld-linux-x86-64.so.2)
.hash        (ELF hash table)
.dynsym      (dynamic symbol table)
.dynstr      (dynamic string table)
.rela.plt    (PLT relocations)
.plt         (procedure linkage table)
.text        (code)
.rodata      (string literals)
.got.plt     (global offset table)
.dynamic     (dynamic linker metadata)
.data        (initialized globals)
.symtab / .strtab
```

## Limitations

- x86_64 Linux only
- No shared library output (only executables)
- No debug info (DWARF) passthrough
- No weak symbols or symbol versioning
