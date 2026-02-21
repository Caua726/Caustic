# Object to Executable (caustic-ld)

The linker (`caustic-ld`) is a standalone tool that combines one or more ELF64 relocatable object files (`.o`) into a final executable. It supports both static and dynamic linking and is written entirely in Caustic with no external dependencies.

## Files

| File | Purpose |
|------|---------|
| `caustic-linker/main.cst` | CLI parsing and orchestration |
| `caustic-linker/elf_reader.cst` | ELF64 object file reader: parses headers, sections, symbols, relocations |
| `caustic-linker/linker.cst` | Core linking logic: section merging, symbol resolution, relocation application |
| `caustic-linker/elf_writer.cst` | ELF64 executable writer: generates program headers, segments, entry stub |

## Usage

```bash
# Single object, static linking
./caustic-ld program.cst.s.o -o program

# Multiple objects
./caustic-ld main.cst.s.o lib.cst.s.o -o program

# Dynamic linking with libc
./caustic-ld program.cst.s.o -lc -o program
```

### CLI Arguments

| Argument | Description |
|----------|-------------|
| `<file.o>` | Input object file (one or more) |
| `-o <name>` | Output executable name (default: `a.out`) |
| `-lc` | Link against libc (`libc.so.6`) dynamically |

## Pipeline

```
  input1.o  input2.o  ...
      |         |
      v         v
  [ELF Reader]  elf_reader.cst
      |         |
      | (parsed section contents, symbol tables, relocations)
      v
  [Section Merge]  linker.cst
      |
      | Concatenate .text sections from all objects
      | Concatenate .data sections from all objects
      | Concatenate .rodata sections from all objects
      | Sum .bss sizes from all objects
      v
  [Symbol Resolution]  linker.cst
      |
      | Build global symbol table
      | Match undefined references to definitions
      | Detect undefined symbols (error)
      v
  [Relocation Application]  linker.cst
      |
      | Apply R_X86_64_PC32, R_X86_64_32S, R_X86_64_PLT32
      | Patch machine code with resolved addresses
      v
  [ELF Writer]  elf_writer.cst
      |
      | Write ELF64 header
      | Write program headers (PT_LOAD segments)
      | Write _start entry stub
      | Write section contents
      | Optionally: write PLT/GOT/.dynamic for dynamic linking
      v
  executable
```

## Section Merging

When linking multiple object files, the linker concatenates corresponding sections:

1. **`.text` merge**: Machine code from each object is appended sequentially. Symbol offsets from the second and subsequent objects are adjusted by the cumulative `.text` size of preceding objects.

2. **`.rodata` merge**: Read-only data (strings, float constants) from each object is appended. References are adjusted accordingly.

3. **`.data` merge**: Initialized global data from each object is concatenated.

4. **`.bss` merge**: BSS sizes are summed. No file content is stored; the segment size simply grows.

## Symbol Resolution

The linker maintains a `GlobalSymTab` -- a combined symbol table across all input objects.

### Resolution Rules

1. **Defined symbols**: Symbols with a valid section index and value are definitions. Their final address is computed as: `section_base_address + section_offset_within_merged_section + symbol_offset_within_original_section`.

2. **Undefined symbols**: Symbols with `SHN_UNDEF` are references that must be matched to a definition in another object file. If no definition is found (and it is not provided by a dynamic library), the linker reports an error.

3. **Global vs. Local**: Only `STB_GLOBAL` symbols participate in cross-object resolution. `STB_LOCAL` symbols are scoped to their defining object.

4. **Duplicate definitions**: If two objects define the same global symbol, this is a linker error.

## Relocation Application

After section merging and symbol resolution, the linker patches the machine code by applying relocations:

### Relocation Types

| Type | Description | Formula |
|------|-------------|---------|
| `R_X86_64_PC32` | 32-bit PC-relative | `S + A - P` (symbol + addend - patch location) |
| `R_X86_64_32S` | 32-bit signed absolute | `S + A` |
| `R_X86_64_PLT32` | 32-bit PC-relative to PLT | `S + A - P` (static) or PLT entry (dynamic) |

Where:
- `S` = final address of the referenced symbol
- `A` = addend from the relocation entry (typically -4)
- `P` = address of the location being patched

Each relocation patches 4 bytes at the specified offset in the `.text` section.

## ELF Executable Layout

### Static Executable

A static executable has 2 PT_LOAD program headers:

```
ELF Header (64 bytes)
Program Headers (2 * 56 bytes)
[padding to 4096-byte page boundary]

Segment 1: R+X (Read + Execute)
  .text    (machine code)
  .rodata  (read-only data)

[padding to 4096-byte page boundary]

Segment 2: R+W (Read + Write)
  .data    (initialized globals)
  .bss     (zero-initialized globals, implicit)

Section Headers (optional, for debugging)
  .symtab
  .strtab
  .shstrtab
```

**Memory layout**:

| Component | Virtual Address |
|-----------|----------------|
| ELF header + phdrs | `0x400000` |
| `.text` | `0x401000` (page-aligned) |
| `.rodata` | immediately after `.text` |
| `.data` | next page boundary after R+X segment |
| `.bss` | immediately after `.data` (in memory only) |

### Dynamic Executable

A dynamic executable adds structures for the runtime linker:

```
ELF Header (64 bytes)
Program Headers (4 * 56 bytes)

Segment 1: PT_INTERP
  path to dynamic linker: "/lib64/ld-linux-x86-64.so.2"

Segment 2: PT_LOAD (R+X)
  .text
  .rodata
  .plt     (Procedure Linkage Table stubs)

Segment 3: PT_LOAD (R+W)
  .got.plt (Global Offset Table)
  .data
  .dynamic (dynamic linking metadata)
  .bss

Segment 4: PT_DYNAMIC
  (points to .dynamic section)

Additional sections:
  .dynsym   (dynamic symbol table)
  .dynstr   (dynamic string table)
  .hash     (symbol hash table for dynamic linker)
  .rela.plt (PLT relocations)
```

## The _start Entry Stub

Every Caustic executable begins execution at `_start`, not at `main`. The linker generates a 28-byte stub that sets up the initial stack frame and calls `main`:

```asm
_start:
    xor rbp, rbp          # clear frame pointer (ABI requirement)
    pop rdi               # argc (pushed by kernel)
    mov rsi, rsp          # argv (pointer to argument array on stack)
    and rsp, -16          # align stack to 16 bytes
    call main             # call the Caustic main function
    mov rdi, rax          # main's return value becomes exit code
    mov rax, 60           # syscall number for exit
    syscall               # exit(main_return_value)
```

This stub ensures that:
- `main` receives `argc` in rdi and `argv` (as a raw pointer) in rsi
- The stack is 16-byte aligned before the call (System V ABI requirement)
- The process exits cleanly with `main`'s return value as the exit code

## Dynamic Linking Details

When `-lc` is specified, the linker generates the additional structures needed for the Linux dynamic linker (`ld-linux-x86-64.so.2`) to resolve libc symbols at runtime.

### PLT (Procedure Linkage Table)

Each external function gets a PLT stub. On the first call, the stub jumps through the GOT (which initially points to the resolver). After resolution, the GOT is patched with the real function address, so subsequent calls go directly.

PLT stub structure (per function):
```asm
.plt.func:
    jmp [rip + got_offset]    # jump through GOT
    push relocation_index     # push reloc index for lazy resolver
    jmp .plt_resolve          # jump to PLT[0] (resolver)
```

### GOT (Global Offset Table)

The `.got.plt` section contains:
- Entry 0: Address of `.dynamic` section
- Entry 1: Reserved for dynamic linker (link_map pointer)
- Entry 2: Reserved for dynamic linker (resolver function)
- Entry 3+: One entry per external function, initially pointing to the PLT push/jmp sequence

### .dynamic Section

Contains `DT_*` entries telling the runtime linker where to find:
- `DT_NEEDED`: Shared library name (e.g., `libc.so.6`)
- `DT_SYMTAB`: Address of `.dynsym`
- `DT_STRTAB`: Address of `.dynstr`
- `DT_HASH`: Address of `.hash`
- `DT_JMPREL`: Address of `.rela.plt`
- `DT_PLTRELSZ`: Size of PLT relocations
- `DT_PLTGOT`: Address of `.got.plt`
- `DT_NULL`: Terminator

### Library Name Mapping

The `-lc` flag maps to `libc.so.6` as the `DT_NEEDED` entry. The linker embeds this name in the `.dynstr` section.

## Heap Sizing

The linker sizes its heap based on total input file sizes: `total_file_sizes * 4 + 256KB`, with a minimum of 512KB. An additional 512KB is allocated when dynamic linking is enabled, to accommodate PLT/GOT and dynamic section structures.

## Multi-Object Linking Example

Compiling and linking a program with a separate library:

```bash
# Compile library (no main required)
./caustic -c lib.cst
./caustic-as lib.cst.s

# Compile main program
./caustic main.cst
./caustic-as main.cst.s

# Link both objects
./caustic-ld main.cst.s.o lib.cst.s.o -o program
```

Cross-object references work through:
- The compiler emits `.globl` for all functions
- `extern fn` declarations in Caustic create undefined symbol references
- The linker resolves these references during symbol resolution
