# Relocations

Relocations are entries in the `.rela.text` section that tell the linker where and how to patch machine code bytes. They are generated during pass 2 whenever the assembler encounters a reference that cannot be fully resolved at assembly time -- either because the target is in a different section or because the target symbol is undefined (external).

## Relocation Entry Format

Each relocation is a 24-byte `Elf64_Rela` entry:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 8 | r_offset | Byte offset within the section where the patch applies |
| 0x08 | 8 | r_info | Symbol index (upper 32 bits) and relocation type (lower 32 bits) |
| 0x10 | 8 | r_addend | Constant addend to add to the computed value |

The `r_info` field is composed as: `(sym_index << 32) | type`.

## Supported Relocation Types

### R_X86_64_PC32 (type 2)

**PC-relative 32-bit offset.** Used for:
- RIP-relative data accesses within the same object (e.g., `lea rdi, [rip + .LC0]` referencing a `.rodata` string)
- Local function calls within `.text` that reference symbols in other sections

**Formula**: `S + A - P`
- `S` = symbol value (virtual address of the target)
- `A` = addend (typically -4, because the rel32 field is 4 bytes and the CPU adds relative offset from the end of the instruction)
- `P` = virtual address of the relocation site (where the patch is applied)

The assembler sets `addend = -4` for these relocations, because the CPU interprets the rel32 relative to the instruction pointer after the 4-byte displacement field.

### R_X86_64_PLT32 (type 4)

**PLT-relative 32-bit offset.** Used for:
- Calls to undefined (external) functions: `call external_function`
- Jumps to undefined symbols: `jmp external_function`

**Formula**: Same as PC32: `S + A - P`

In practice, the linker treats PLT32 the same as PC32 for static linking (direct call). For dynamic linking, the linker redirects these through PLT stubs.

**When generated**: During pass 2, if an instruction is `call` or `jmp` and the label refers to an undefined symbol (not found in `.text` during pass 1), the assembler:
1. Marks the symbol as global
2. Emits a PLT32 relocation at `offset = instruction_offset + 1` (the rel32 starts after the 1-byte opcode for call/jmp)
3. Sets `addend = -4`

### R_X86_64_GOTPCREL (type 9)

**GOT-relative PC-relative 32-bit offset.** Used for:
- RIP-relative loads of external data symbols (e.g., `mov rax, [rip + stdout]`)
- Accessing global variables defined in shared libraries

**Formula**: `G + GOT + A - P`
- `G` = offset of the GOT entry for this symbol
- `GOT` = base of the GOT
- `A` = addend (-4)
- `P` = relocation site address

**When generated**: During pass 2, if a `mov` instruction has an `OP_RIP_LABEL` operand referencing an undefined symbol, the assembler generates a GOTPCREL relocation. For defined symbols, it generates a PC32 relocation instead.

## Relocation Offset Calculation

The `r_offset` value points to the 4-byte field within the encoded instruction that needs patching. The assembler calculates this as:

- For **call/jmp**: `instruction_offset + 1` (1-byte opcode before the rel32)
- For **RIP-relative instructions**: `instruction_offset + instruction_size - 4` (the rel32 is always the last 4 bytes of the instruction)

## Internal Data Structures

The assembler uses two internal structs:

```cst
struct Reloc {
    offset as i64;     // r_offset
    sym_idx as i64;    // index into the assembler's symbol table
    rtype as i64;      // R_X86_64_PC32, R_X86_64_PLT32, or R_X86_64_GOTPCREL
    addend as i64;     // r_addend
}

struct RelocList {
    data as *u8;       // array of Reloc entries
    count as i32;      // number of entries
    cap as i32;        // allocated capacity
}
```

During ELF output, the `sym_idx` is remapped from the assembler's internal symbol numbering to the ELF symbol table indices using the `sym_map` array (since the ELF requires locals before globals, which may reorder symbols).

## Relocation Workflow

1. **Pass 1**: Symbols are collected. No relocations are generated.
2. **Pass 2**: For each instruction with a label or RIP-relative operand:
   - Look up the symbol in the symbol table
   - If the symbol is in `.text` and fully resolved, pass the offset directly to `encode()` (no relocation needed)
   - If the symbol is in another section (`.data`, `.rodata`) or undefined, emit a relocation via `reloc_add()` and pass 0 as the resolved value to `encode()`
3. **ELF output**: Relocations are serialized into the `.rela.text` section with remapped symbol indices.
