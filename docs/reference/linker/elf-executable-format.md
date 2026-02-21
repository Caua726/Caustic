# ELF Executable Format

This document describes the structure of ELF executables produced by caustic-ld. The output
is a standard ELF64 executable for x86_64 Linux, compatible with the kernel's exec() loader.

## ELF Header (64 bytes, offset 0)

```
e_ident[16]:
  [0-3]   7F 45 4C 46     — magic number "\x7fELF"
  [4]     02              — EI_CLASS: ELFCLASS64
  [5]     01              — EI_DATA: ELFDATA2LSB (little-endian)
  [6]     01              — EI_VERSION: EV_CURRENT
  [7]     00              — EI_OSABI: ELFOSABI_NONE
  [8-15]  00...           — padding

e_type     : 2 bytes = ET_EXEC (2)          — executable file
e_machine  : 2 bytes = EM_X86_64 (62)
e_version  : 4 bytes = EV_CURRENT (1)
e_entry    : 8 bytes = virtual address of _start
e_phoff    : 8 bytes = 64 (program headers immediately follow)
e_shoff    : 8 bytes = 0  (no section headers in output)
e_flags    : 4 bytes = 0
e_ehsize   : 2 bytes = 64 (ELF header size)
e_phentsize: 2 bytes = 56 (size of one program header)
e_phnum    : 2 bytes = 2 (static) or 4 (dynamic)
e_shentsize: 2 bytes = 64
e_shnum    : 2 bytes = 0
e_shstrndx : 2 bytes = 0
```

## Program Headers

Program headers describe memory segments to the kernel loader. caustic-ld does not emit
section headers; sections are only used during linking, not at runtime.

### Static Output (2 program headers, starting at offset 64)

Each program header is 56 bytes. Fields:

```
p_type   : 4 bytes — segment type
p_flags  : 4 bytes — permissions
p_offset : 8 bytes — offset in file where segment data begins
p_vaddr  : 8 bytes — virtual address to map this segment to
p_paddr  : 8 bytes — physical address (same as vaddr, unused on Linux)
p_filesz : 8 bytes — size of segment data in the file
p_memsz  : 8 bytes — size of segment in memory (>= filesz for .bss)
p_align  : 8 bytes — segment alignment (0x200000 = 2MB)
```

Header 0 — PT_LOAD R+X:
```
p_type   = 1 (PT_LOAD)
p_flags  = 5 (PF_R | PF_X)
p_offset = 0
p_vaddr  = 0x400000
p_filesz = 176 + _start_size + .text_size + .rodata_size
p_memsz  = same
p_align  = 0x200000
```

Header 1 — PT_LOAD R+W:
```
p_type   = 1 (PT_LOAD)
p_flags  = 6 (PF_R | PF_W)
p_offset = offset of .data in file
p_vaddr  = 0x400000 + offset
p_filesz = .data_size
p_memsz  = .data_size + .bss_size
p_align  = 0x200000
```

### Dynamic Output (4 program headers)

Dynamic mode adds two more headers after the two PT_LOAD entries:

Header 2 — PT_INTERP:
```
p_type   = 3 (PT_INTERP)
p_flags  = 4 (PF_R)
p_offset = offset of .interp section in file
p_vaddr  = virtual address of .interp
p_filesz = length of interpreter path string (including null terminator)
p_memsz  = same
p_align  = 1
```

Header 3 — PT_DYNAMIC:
```
p_type   = 2 (PT_DYNAMIC)
p_flags  = 6 (PF_R | PF_W)
p_offset = offset of .dynamic section in file
p_vaddr  = virtual address of .dynamic
p_filesz = size of .dynamic
p_memsz  = same
p_align  = 8
```

## File Layout (Static)

```
Offset    Content                        Size
------    -------                        ----
0         ELF Header                     64
64        Program Header 0 (R+X)         56
120       Program Header 1 (R+W)         56
176       _start stub                    28
204       .text (merged)                 variable
204+text  .rodata (merged)               variable
...       [alignment padding to page]
page_n    .data (merged)                 variable
          (.bss — no file bytes)
```

## File Layout (Dynamic, additional sections)

```
...       .data                          variable
...       .interp                        45 bytes (path + null)
...       .plt                           16 + (16 * num_extern_syms) bytes
...       .got                           24 + (8 * num_extern_syms) bytes
...       .dynamic                       16 * num_dynamic_entries bytes
...       .dynsym                        24 * (1 + num_extern_syms) bytes
...       .dynstr                        variable (all symbol + library names)
...       .hash                          variable (4 + 4 + nbucket*4 + nchain*4)
...       .rela.plt                      24 * num_extern_syms bytes
```

## Virtual Address Space

All sections are mapped starting from 0x400000, the conventional ELF base for x86_64 Linux
non-PIE executables. The kernel does not randomize the load address for ET_EXEC binaries
(unless ASLR is set to mode 2, which is uncommon).

```
0x400000    — ELF header (mapped but rarely accessed at runtime)
0x400040    — program headers
0x4000B0    — _start
0x4000CC    — .text begins (typical, depends on sizes)
...
```

## No Section Headers

caustic-ld does not write a section header table to the output. ELF section headers are
optional for execution — only program headers are required. Omitting them reduces output
size and simplifies the writer. Tools like `readelf -S` will report "no sections" for
caustic-ld output, which is expected.

## Inspecting Output

```bash
readelf -h program     # ELF header
readelf -l program     # program headers (segments)
objdump -d program     # disassemble .text
xxd program | head     # raw hex dump of first bytes
```
