# Dynamic Linking

Dynamic linking allows caustic-ld to produce executables that call functions from shared
libraries (e.g., libc.so.6) resolved at runtime. The dynamic linker (ld-linux-x86-64.so.2)
loads required libraries and fills in function addresses before main executes.

## Activating Dynamic Mode

Pass a -l flag to caustic-ld:

```bash
./caustic-ld main.o -lc -o program        # links libc.so.6
./caustic-ld main.o -lm -lc -o program    # links libm.so.6 and libc.so.6
```

The `-lc` flag maps to `libc.so.6`. caustic-ld writes the full .so filename into the
.dynamic section's DT_NEEDED entries.

## Additional ELF Structures Generated

Dynamic executables require several additional sections and two extra program headers:

```
Program Headers (4 total):
  PT_LOAD    R+X    — code segment (same as static)
  PT_LOAD    R+W    — data segment (same as static)
  PT_INTERP         — path to dynamic linker
  PT_DYNAMIC        — pointer to .dynamic section

Sections (additional):
  .interp     — "/lib64/ld-linux-x86-64.so.2\0"
  .plt        — Procedure Linkage Table stubs
  .got        — Global Offset Table entries
  .dynamic    — dynamic linking metadata
  .dynsym     — dynamic symbol table (exported/imported symbols)
  .dynstr     — string table for dynamic symbols and library names
  .hash       — symbol hash table for fast lookup by the dynamic linker
  .rela.plt   — relocations for GOT entries (filled by dynamic linker at startup)
```

## PT_INTERP

Specifies the path to the dynamic linker (program interpreter):

```
/lib64/ld-linux-x86-64.so.2
```

The kernel reads this path from PT_INTERP and loads that program to handle library loading
before transferring control to _start.

## PLT/GOT Mechanism

External function calls use an indirection through the Procedure Linkage Table (PLT) and the
Global Offset Table (GOT). This allows the dynamic linker to fill in the real function address
at runtime without requiring the linker to know it at link time.

### PLT Entry (16 bytes per external symbol)

```asm
; PLT entry for printf:
printf@plt:
    jmp  qword [printf@got]    ; jump through GOT slot (8 bytes: FF 25 <rel32>)
    push <reloc_index>         ; push index for lazy binding resolver (5 bytes: 68 <imm32>)
    jmp  plt0                  ; jump to PLT resolver (5 bytes: E9 <rel32>)
```

On the first call, the GOT slot points back to the push instruction. The dynamic linker
resolves the symbol, updates the GOT slot with the real address, and the next call jumps
directly to the library function.

### PLT[0] Resolver Stub (16 bytes)

```asm
plt0:
    push qword [got+8]         ; push link_map pointer
    jmp  qword [got+16]        ; jump to dynamic linker resolver
```

### GOT Layout

```
got[0]   — address of .dynamic section (set by linker)
got[1]   — link_map pointer (filled by dynamic linker)
got[2]   — dynamic linker resolver function (filled by dynamic linker)
got[3]   — initial value: address of printf@plt+6 (lazy binding)
got[4]   — initial value: address of next_symbol@plt+6
...
```

## .dynamic Section

An array of (tag, value) pairs terminated by DT_NULL:

| Tag          | Value                                     |
|--------------|-------------------------------------------|
| DT_NEEDED    | string offset of library name (e.g. libc.so.6) |
| DT_PLTRELSZ  | size of .rela.plt in bytes               |
| DT_PLTGOT    | virtual address of .got                  |
| DT_SYMTAB    | virtual address of .dynsym               |
| DT_STRTAB    | virtual address of .dynstr               |
| DT_STRSZ     | size of .dynstr                           |
| DT_SYMENT    | size of one .dynsym entry (24)           |
| DT_HASH      | virtual address of .hash                 |
| DT_PLTREL    | DT_RELA (7)                              |
| DT_JMPREL    | virtual address of .rela.plt             |
| DT_NULL      | 0 (terminator)                            |

## .dynsym and .dynstr

.dynsym contains 24-byte ELF64 symbol entries for each imported external symbol. .dynstr
holds their null-terminated names. The dynamic linker uses these to locate the definitions
in the loaded shared libraries.

## .hash

A System V hash table over .dynsym, allowing O(1) symbol lookup by name. The dynamic linker
uses this when resolving PLT entries at runtime.

```
nbucket = computed from symbol count
nchain  = symbol count
bucket[hash(name) % nbucket] = starting dynsym index
chain[i] = next dynsym index with same hash bucket (linked list)
```

## .rela.plt

One 24-byte RELA entry per external symbol:

```
r_offset = virtual address of GOT slot for this symbol
r_info   = (dynsym_index << 32) | R_X86_64_JUMP_SLOT (7)
r_addend = 0
```

The dynamic linker processes these entries at startup (or lazily on first call) and writes
the resolved function address into the GOT slot.

## stdio and libc Exit

When using libc (printf, fwrite, etc.), the program must call libc's `exit()` rather than
the raw `sys_exit` syscall. The libc `exit()` function flushes stdio buffers before terminating.
If `sys_exit` is called directly, buffered output may be lost.

The `_start` stub calls `main` and passes its return value to `sys_exit` directly, which is
correct for Caustic programs that use only syscall-based I/O (std/linux.cst). Programs mixing
libc stdio with direct sys_exit should call libc's exit instead.
