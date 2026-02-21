# Static Linking

Static linking produces a fully self-contained ELF executable. All symbol references are
resolved from the provided .o files at link time. The output binary has no runtime library
dependencies and can run on any compatible x86_64 Linux system.

## Output ELF Structure

A statically linked caustic-ld output has the following layout in the file:

```
Offset 0:    ELF Header          (64 bytes)
Offset 64:   Program Headers     (2 entries x 56 bytes = 112 bytes)
Offset 176:  _start stub         (28 bytes)
Offset 204:  .text               (merged code from all inputs)
             .rodata             (read-only data)
             [padding to page boundary]
             .data               (initialized mutable globals)
             .bss                (zero-fill reservation — no file bytes)
```

## Program Headers (PT_LOAD Segments)

Two PT_LOAD segments describe the memory layout to the kernel:

### Segment 1: R+X (Code)

```
p_type   = PT_LOAD (1)
p_flags  = PF_R | PF_X (5)        — readable + executable
p_offset = 0                       — starts at beginning of file
p_vaddr  = 0x400000               — conventional load address
p_filesz = size of ELF header + PHs + _start + .text + .rodata
p_memsz  = same as filesz
p_align  = 0x200000               — 2MB alignment
```

The ELF header and program headers are included in this segment. They are mapped into memory
(read-only) as part of the executable region, which is standard ELF practice.

### Segment 2: R+W (Data)

```
p_type   = PT_LOAD (1)
p_flags  = PF_R | PF_W (6)        — readable + writable
p_offset = offset of .data in file
p_vaddr  = 0x400000 + p_offset    — immediately follows code in VA space
p_filesz = size of .data + .rodata (rodata placed in data segment for simplicity)
p_memsz  = filesz + bss_size      — extends for zero-fill .bss
p_align  = 0x200000
```

The .bss region is covered by p_memsz but not p_filesz. The kernel zero-fills the difference.

## Virtual Address Layout

```
0x400000                          — ELF header
0x400040                          — Program headers
0x4000B0                          — _start stub (28 bytes)
0x4000CC                          — .text begins
0x4000CC + text_size              — .rodata begins
...                               — page-aligned boundary
page_aligned                      — .data begins
page_aligned + data_size          — .bss begins (not in file)
```

The exact addresses depend on section sizes. caustic-ld pre-calculates all section sizes in a
first pass, then assigns virtual addresses, then writes data.

## Entry Point

The ELF header's e_entry field is set to the virtual address of `_start`, the linker-generated
entry stub. `_start` calls `main` and then executes sys_exit with the return value.

See `start-stub.md` for the stub's machine code.

## ELF Header Fields (Static)

```
e_ident[EI_CLASS]   = ELFCLASS64 (2)
e_ident[EI_DATA]    = ELFDATA2LSB (1)       — little-endian
e_ident[EI_OSABI]   = ELFOSABI_NONE (0)
e_type              = ET_EXEC (2)            — executable
e_machine           = EM_X86_64 (62)
e_entry             = virtual address of _start
e_phoff             = 64                     — program headers right after ELF header
e_shoff             = 0                      — no section headers in output
e_phnum             = 2                      — two PT_LOAD segments
e_phentsize         = 56
```

caustic-ld does not emit section headers in its output. Section headers are only needed for
debugging and relinking; executables work without them.

## Invocation

```bash
# Single object
./caustic-ld program.cst.s.o -o program

# Multiple objects
./caustic-ld lib.cst.s.o main.cst.s.o -o program

# Run
./program
echo "Exit: $?"
```

## No Standard Library Required

Caustic programs do not link against libc. All system interaction is through direct Linux
syscalls via `std/linux.cst`. The binary is smaller and starts faster than a libc-linked
equivalent. There is no C runtime startup (no __libc_start_main, no atexit handlers).

The only "glue" added by caustic-ld is the 28-byte _start stub that bridges the kernel's
entry convention (argc/argv on the stack) to Caustic's main(argc, argv) signature.
