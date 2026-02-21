# caustic-ld — Linker Overview

caustic-ld is the self-hosted linker for the Caustic compiler toolchain. It reads ELF relocatable
object files (.o), merges their sections, resolves symbol references, applies relocations, and
writes an executable ELF binary. It is written entirely in the Caustic language with no libc or
external library dependencies.

## Pipeline

```
Input .o files
     |
     v
[1] Read ELF objects        — parse headers, sections, symbols, relocations
     |
     v
[2] Merge sections          — concatenate .text, .data, .rodata, .bss across all inputs
     |
     v
[3] Resolve symbols         — match undefined references to defined symbols
     |
     v
[4] Apply relocations       — patch instruction bytes with resolved addresses
     |
     v
[5] Write ELF executable    — emit ELF header, program headers, section data
     |
     v
Output executable
```

## Linking Modes

### Static linking (default)

Produces a fully self-contained ELF binary. No runtime dependencies. All symbol references are
resolved at link time from the provided .o files.

```
caustic-ld main.o -o program
caustic-ld lib.o main.o -o program
```

Output has two PT_LOAD segments:
- R+X: executable code (.text)
- R+W: read-write data (.data, .rodata, .bss)

### Dynamic linking

Produces an ELF binary that links against shared libraries at runtime. Uses PLT/GOT stubs for
external function calls. The dynamic linker (ld-linux) is invoked by the kernel at startup.

```
caustic-ld main.o -lc -o program    # link against libc.so.6
```

Additional ELF structures are generated: PT_INTERP, PT_DYNAMIC, .plt, .got, .dynamic, .dynsym,
.dynstr, .hash, .rela.plt.

## Source Layout

```
caustic-linker/
  main.cst        — entry point, argument parsing, top-level orchestration
  elf_reader.cst  — ELF .o file parsing
  linker.cst      — section merging, symbol resolution, relocation application
  elf_writer.cst  — ELF executable output
```

## Build and Usage

```bash
make linker          # builds ./caustic-ld

./caustic-ld input.o -o output
./caustic-ld a.o b.o c.o -o output
./caustic-ld main.o -lc -o output    # dynamic, links libc

make test-linker     # run the full test suite (54 tests)
```

## Full Toolchain Pipeline

```bash
# Compile
./caustic program.cst              # emits program.cst.s

# Assemble
./caustic-as program.cst.s         # emits program.cst.s.o

# Link
./caustic-ld program.cst.s.o -o program

# Run
./program
```

## Design Constraints

- No libc: all I/O and syscalls go through Caustic's std/linux.cst wrappers.
- No external linker libraries: ELF structures are built manually in memory.
- Heap sizing: allocated as total_input_size * 4 + 256KB, minimum 512KB (plus 512KB for dynamic mode).
- Packed structs: Caustic structs have no alignment padding; internal data structures use all-i64
  fields where possible to avoid cross-boundary field issues.
