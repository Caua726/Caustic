# ELF Object File Reading

caustic-ld reads standard ELF64 relocatable object files (.o) produced by caustic-as. The
reader is implemented in `caustic-linker/elf_reader.cst`.

## ELF64 File Structure

An ELF relocatable object has the following layout:

```
[ ELF Header       ]  64 bytes, describes file type and architecture
[ Section Headers  ]  array of 64-byte entries, one per section
[ Section Data     ]  raw bytes for each section (.text, .data, .symtab, etc.)
```

The ELF header contains the offset to the section header table and the count of entries. The
section header string table index (e_shstrndx) gives the section whose data is the section name
string table.

## ELF Header Fields Read

| Field       | Size | Purpose                                      |
|-------------|------|----------------------------------------------|
| e_ident     | 16   | Magic number, class (64-bit), endianness      |
| e_type      | 2    | ET_REL (1) for relocatable object             |
| e_machine   | 2    | EM_X86_64 (62)                               |
| e_shoff     | 8    | Offset to section header table               |
| e_shentsize | 2    | Size of one section header entry (64)        |
| e_shnum     | 2    | Number of section headers                    |
| e_shstrndx  | 2    | Index of section name string table           |

## Section Headers

Each section header entry describes one section:

| Field      | Size | Purpose                                       |
|------------|------|-----------------------------------------------|
| sh_name    | 4    | Offset into section name string table         |
| sh_type    | 4    | SHT_PROGBITS, SHT_SYMTAB, SHT_RELA, etc.    |
| sh_flags   | 8    | SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE          |
| sh_offset  | 8    | Byte offset of section data in file           |
| sh_size    | 8    | Size of section data in bytes                 |
| sh_link    | 4    | For SHT_SYMTAB: index of associated strtab   |
| sh_info    | 4    | For SHT_SYMTAB: index of first global symbol |
| sh_entsize | 8    | Entry size for table sections                 |

## Sections Parsed

### .text

Raw x86_64 machine code. Loaded at the executable's code segment. Relocations reference
symbols whose addresses are not yet known at assemble time.

### .data and .rodata

Initialized data. .rodata is read-only (string literals, constants). .data is read-write
(mutable globals).

### .bss

Zero-initialized data. Not stored in the file — only the size is recorded. The linker
reserves space in the output and the OS zero-fills it.

### .symtab

Symbol table. Each entry (24 bytes) describes a function, variable, or label:

```
st_name  : 4 bytes — offset into .strtab
st_info  : 1 byte  — binding (LOCAL/GLOBAL) and type (FUNC/OBJECT/NOTYPE)
st_other : 1 byte  — visibility (unused by caustic-ld)
st_shndx : 2 bytes — section index (SHN_UNDEF=0 for undefined symbols)
st_value : 8 bytes — offset within the section
st_size  : 8 bytes — symbol size in bytes
```

A symbol with `st_shndx == SHN_UNDEF` is an external reference that must be supplied by
another object or shared library.

### .strtab

Null-terminated strings for symbol names. Indexed by offsets stored in .symtab entries.

### .rela.text

Relocation entries for the .text section. Each entry (24 bytes):

```
r_offset : 8 bytes — byte offset within .text to patch
r_info   : 8 bytes — symbol index (upper 32 bits) and relocation type (lower 32 bits)
r_addend : 8 bytes — constant addend for the relocated value
```

## Internal Representation

After reading, each object file is represented as a collection of:

- Section buffers: raw byte arrays with their sizes and file offsets
- Symbol list: name, binding, section index, value (offset), defining object index
- Relocation list: section, offset, type, symbol name, addend

These structures are kept in a flat heap-allocated array indexed by object file number, allowing
the merger and relocation stages to reference them by (object_index, symbol_index) pairs.

## Error Conditions

- Magic number mismatch: not an ELF file
- e_type != ET_REL: not a relocatable object (caustic-ld does not accept shared libraries as input)
- e_machine != EM_X86_64: wrong architecture
- Truncated file: section data extends beyond file size
