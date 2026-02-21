# ELF Object Format

The assembler (`elf.cst`) generates ELF64 relocatable object files conforming to the System V ABI for x86_64. Each `.o` file contains machine code, data, symbols, and relocations that the linker later combines into an executable.

## File Layout

The generated `.o` file has the following layout:

```
Offset 0x00:   ELF Header (64 bytes)
Offset 0x40:   .text section data
                (aligned to 16)
                .data section data
                (aligned to 16)
                .rodata section data
                (aligned to 16)
                .symtab section data
                (aligned to 8)
                .strtab section data
                .rela.text section data
                (aligned to 8)
                .shstrtab section data
                (aligned to 8)
                Section header table (9 entries x 64 bytes)
```

## ELF Header

The 64-byte ELF header contains:

| Offset | Size | Field | Value |
|--------|------|-------|-------|
| 0x00 | 4 | Magic | `7f 45 4c 46` ("\x7fELF") |
| 0x04 | 1 | Class | 2 (64-bit) |
| 0x05 | 1 | Data | 1 (little-endian) |
| 0x06 | 1 | EI_VERSION | 1 |
| 0x07 | 1 | OS/ABI | 0 (System V) |
| 0x08 | 8 | Padding | 0 |
| 0x10 | 2 | e_type | 1 (ET_REL -- relocatable) |
| 0x12 | 2 | e_machine | 62 (EM_X86_64) |
| 0x14 | 4 | e_version | 1 |
| 0x18 | 8 | e_entry | 0 (not an executable) |
| 0x20 | 8 | e_phoff | 0 (no program headers) |
| 0x28 | 8 | e_shoff | Offset of section header table |
| 0x30 | 4 | e_flags | 0 |
| 0x34 | 2 | e_ehsize | 64 |
| 0x36 | 2 | e_phentsize | 0 |
| 0x38 | 2 | e_phnum | 0 |
| 0x3A | 2 | e_shentsize | 64 |
| 0x3C | 2 | e_shnum | 9 |
| 0x3E | 2 | e_shstrndx | 8 (index of .shstrtab) |

## Sections

The object file contains 9 section headers (indices 0-8):

### Section 0: SHT_NULL

The mandatory null section header (64 bytes of zeros).

### Section 1: .text

- **Type**: SHT_PROGBITS (1)
- **Flags**: SHF_ALLOC | SHF_EXECINSTR (0x06)
- **Alignment**: 16 bytes
- **Content**: Encoded machine code from all instructions in `.text` segments

### Section 2: .data

- **Type**: SHT_PROGBITS (1)
- **Flags**: SHF_ALLOC | SHF_WRITE (0x03)
- **Alignment**: 8 bytes
- **Content**: Initialized read-write data from `.data` directives

### Section 3: .rodata

- **Type**: SHT_PROGBITS (1)
- **Flags**: SHF_ALLOC (0x02)
- **Alignment**: 1 byte
- **Content**: Read-only data, typically string literals

### Section 4: .bss

- **Type**: SHT_NOBITS (8)
- **Flags**: SHF_ALLOC | SHF_WRITE (0x03)
- **Alignment**: 8 bytes
- **Content**: None (uninitialized data; only the size is recorded)

### Section 5: .symtab

- **Type**: SHT_SYMTAB (2)
- **Alignment**: 8 bytes
- **Entry size**: 24 bytes per symbol
- **sh_link**: 6 (points to .strtab)
- **sh_info**: Index of first global symbol (number of local symbols)

See the [Symbol Table](symbol-table.md) reference for entry format.

### Section 6: .strtab

- **Type**: SHT_STRTAB (3)
- **Alignment**: 1 byte
- **Content**: Null-terminated strings for symbol names. Starts with a null byte at offset 0.

### Section 7: .rela.text

- **Type**: SHT_RELA (4)
- **Alignment**: 8 bytes
- **Entry size**: 24 bytes per relocation
- **sh_link**: 5 (points to .symtab)
- **sh_info**: 1 (applies to .text section)

See the [Relocations](relocations.md) reference for entry format.

### Section 8: .shstrtab

- **Type**: SHT_STRTAB (3)
- **Alignment**: 1 byte
- **Content**: Null-terminated section name strings. Contains: `\0.text\0.data\0.rodata\0.bss\0.symtab\0.strtab\0.rela.text\0.shstrtab\0`

## Section Header Format

Each section header is 64 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | sh_name | Offset into .shstrtab |
| 0x04 | 4 | sh_type | Section type (SHT_*) |
| 0x08 | 8 | sh_flags | Section flags (SHF_*) |
| 0x10 | 8 | sh_addr | Virtual address (0 for relocatable) |
| 0x18 | 8 | sh_offset | File offset of section data |
| 0x20 | 8 | sh_size | Section size in bytes |
| 0x28 | 4 | sh_link | Associated section index |
| 0x2C | 4 | sh_info | Additional info (varies by type) |
| 0x30 | 8 | sh_addralign | Alignment requirement |
| 0x38 | 8 | sh_entsize | Entry size for fixed-size entries |

## Alignment

Section data in the file is aligned to 16 bytes for `.text`, `.data`, and `.rodata`, and to 8 bytes for `.symtab`, `.rela.text`, and `.shstrtab`. Padding bytes (zeros) are inserted between sections as needed.

## Implementation Notes

The `write_elf()` function in `elf.cst` builds the complete object file in a single `ByteBuffer` and writes it to the file descriptor in one `write()` syscall. The layout computation happens in two steps:

1. **Forward offset calculation**: Starting from the ELF header size (64), each section's file offset is computed sequentially, with alignment padding between sections.

2. **Section header table**: Placed at the end of the file, after all section data. Its offset is stored in the ELF header's `e_shoff` field.
