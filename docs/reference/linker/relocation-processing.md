# Relocation Processing

After symbols are resolved to virtual addresses, caustic-ld applies relocations to patch the
raw machine code bytes with the correct addresses. Implementation is in
`caustic-linker/linker.cst`.

## What Is a Relocation

When caustic-as assembles a .cst.s file, it cannot know the final addresses of symbols that
are defined in other files, or even the final load address of functions in the same file when
position-independent addressing is used. It emits placeholder bytes (usually zeros) at those
locations and records a relocation entry describing how to fill them in.

A relocation entry says:

```
At byte offset <r_offset> in section <section>,
patch the instruction using symbol <sym> with addend <r_addend>,
using relocation formula <type>.
```

## Relocation Entry Format

Each ELF64 RELA entry is 24 bytes:

```
r_offset : u64 — byte offset within the target section to patch
r_info   : u64 — symbol index (bits 63:32) | relocation type (bits 31:0)
r_addend : i64 — constant value added to the symbol address
```

## Supported Relocation Types

### R_X86_64_PC32 (type 2)

PC-relative 32-bit relocation. Used for direct calls and jumps to nearby symbols.

```
patch_value = symbol_address + r_addend - patch_address
```

The 4 bytes at r_offset are replaced with this signed 32-bit value. At runtime, the CPU adds
the program counter (address of the next instruction) to this value to compute the final target.
The assembler emits r_addend = -4 for `call` instructions, accounting for the 4-byte immediate
being part of the instruction before the PC advances.

```asm
call  some_function        ; emits E8 <4 bytes>, reloc R_X86_64_PC32 sym=some_function addend=-4
```

### R_X86_64_PLT32 (type 4)

Same formula as PC32, but signals that a PLT entry should be used for the symbol if it is
defined in a shared library. In static mode, caustic-ld treats PLT32 identically to PC32
(calls go directly to the function). In dynamic mode, the symbol address used is the PLT stub
address rather than the final function address.

```
patch_value = plt_stub_address + r_addend - patch_address
```

### R_X86_64_32S (type 11)

Absolute 32-bit relocation, sign-extended to 64 bits. Used for data references where the full
address fits in 32 signed bits.

```
patch_value = symbol_address + r_addend
```

The 4 bytes at r_offset are replaced with this value. The linker verifies that the result fits
in a signed 32-bit integer; if not, it is a fatal error (address out of range).

## Application Process

For each relocation in the merged relocation list:

1. Look up the symbol name in the global symbol table to get its virtual address.
2. Compute the patch address: `section_load_address + r_offset`.
3. Compute the patch value using the type-specific formula.
4. Write the patch value as a little-endian 32-bit integer at the patch address in the
   output buffer.

```
patch_address = load_address(.text) + adjusted_r_offset
symbol_va     = global_table[sym_name].address

For PC32 / PLT32:
  value = (i32)(symbol_va + r_addend - patch_address)

For 32S:
  value = (i32)(symbol_va + r_addend)

Write 4 bytes little-endian at patch_address in output buffer.
```

## Relocation of .data References

If .data contains a pointer to a function or another data symbol (e.g., a function pointer
stored as a global), a relocation of type R_X86_64_64 (absolute 64-bit) may be emitted.
caustic-ld handles this by writing the full 8-byte virtual address at the specified offset.

## Relocation Errors

| Condition                              | Error                              |
|----------------------------------------|------------------------------------|
| Symbol not in global table             | undefined symbol                   |
| PC32/PLT32 value out of i32 range     | relocation overflow (target > 2GB away) |
| 32S value out of i32 range            | relocation overflow                |
| Unknown relocation type               | unsupported relocation type        |

## PLT Relocations in Dynamic Mode

In dynamic mode, .rela.plt entries are written to the output for the dynamic linker to process.
These are different from the .rela.text relocations caustic-ld processes internally: the linker
does not patch GOT entries itself (they start as addresses pointing into the PLT resolver chain).
The dynamic linker fills GOT slots at process startup. See `dynamic-linking.md`.

## Verification

After applying all relocations, caustic-ld verifies that no undefined references remain in the
relocation list. Any symbol that could not be resolved causes a fatal link error before the
output file is written.
