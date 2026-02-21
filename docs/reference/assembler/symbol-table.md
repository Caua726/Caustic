# Symbol Table

The assembler builds a symbol table during assembly and writes it to the `.symtab` section of the ELF object file. Symbol names are stored in the separate `.strtab` section as null-terminated strings.

## Internal Symbol Representation

During assembly, each symbol is tracked with the following fields:

```cst
struct Symbol {
    name_ptr  as *u8;   // pointer to name in source buffer
    name_len  as i32;   // name length in bytes
    section   as i32;   // 0=.text, 1=.data, 2=.rodata, 3=.bss, 99=undefined
    offset    as i64;   // byte offset within the section
    is_global as i32;   // 1 if declared with .globl
    strtab_off as i32;  // offset in .strtab (assigned during ELF output)
}
```

Symbols are stored in a flat array inside a `SymbolTable` struct (initial capacity: 1024 entries).

## Symbol Resolution

`sym_find_or_add()` handles both definition and forward reference in a single pass:

- If a symbol with that name already exists and has `section == 99` (undefined), and the new call provides a real section, the entry is updated in place. This handles forward references -- labels used before their definition.
- If a symbol with that name already exists and the new call sets `is_global = 1`, the global flag is applied to the existing entry.
- If the name is not found, a new entry is created.

This design means `.globl` directives may appear before or after the label they name.

## .globl Directive

The `.globl name` directive marks a symbol as globally visible to the linker. The Caustic compiler emits `.globl` for every function it defines, enabling multi-object linking:

```asm
.globl my_function
my_function:
    ; ...
```

Without `.globl`, a label is local to the object file and invisible to the linker.

## ELF .symtab Format

Each entry in `.symtab` is 24 bytes (`Elf64_Sym`):

| Offset | Size | Field     | Description                                     |
|--------|------|-----------|-------------------------------------------------|
| 0x00   | 4    | st_name   | Byte offset into .strtab                        |
| 0x04   | 1    | st_info   | `(binding << 4) | type`                         |
| 0x05   | 1    | st_other  | Visibility (always 0 = STV_DEFAULT)             |
| 0x06   | 2    | st_shndx  | Section index (1=.text, 2=.data, 3=.rodata, 4=.bss, 0=undef) |
| 0x08   | 8    | st_value  | Offset within the section (for relocatable .o)  |
| 0x10   | 8    | st_size   | Symbol size (always 0 in caustic-as output)     |

### Binding (st_info high nibble)

| Binding    | Value | Meaning                                       |
|------------|-------|-----------------------------------------------|
| STB_LOCAL  | 0     | Not visible outside this object file          |
| STB_GLOBAL | 1     | Visible to the linker; must be unique         |

### Type (st_info low nibble)

| Type        | Value | Meaning                                      |
|-------------|-------|----------------------------------------------|
| STT_NOTYPE  | 0     | No type (used for local branch labels)       |
| STT_OBJECT  | 1     | Data object (.data / .rodata / .bss globals) |
| STT_FUNC    | 2     | Function (.text globals)                     |
| STT_SECTION | 3     | Synthetic section symbol                     |

## Symbol Ordering in .symtab

ELF requires all local symbols to precede global symbols. The `.symtab` section header's `sh_info` field records the index of the first global symbol. The assembler writes entries in this order:

1. **Null entry** -- 24 zero bytes (required by ELF).
2. **Section symbols** -- four `STT_SECTION / STB_LOCAL` entries for `.text`, `.data`, `.rodata`, `.bss`. They have `st_name = 0` and exist so that section-relative relocations can reference a section as a whole.
3. **Local symbols** -- all user symbols with `is_global == 0`.
4. **Global symbols** -- all user symbols with `is_global == 1`. Those in `.text` get `STT_FUNC`; those in other sections get `STT_OBJECT`.

A `sym_map` array is built during this process, translating from internal symbol indices to final ELF symbol indices. This mapping is used when writing `.rela.text` entries, which reference symbols by ELF index.

## .strtab Format

The string table is a sequence of null-terminated names:

```
\0                    <- offset 0 (empty string, for the null entry and section symbols)
my_function\0         <- offset 1
.L0\0                 <- offset 13
.LC0\0                <- offset 17
```

The first byte is always `\0`. Each name is appended in the order symbols are written to `.symtab`, and the resulting offset is stored in `strtab_off`.

## Symbols Emitted by the Caustic Compiler

| Pattern       | Section  | Binding    | Type        | Example             |
|---------------|----------|------------|-------------|---------------------|
| Function name | `.text`  | STB_GLOBAL | STT_FUNC    | `my_func:`          |
| Local label   | `.text`  | STB_LOCAL  | STT_NOTYPE  | `.L0:`, `.L1:`      |
| String literal| `.rodata`| STB_LOCAL  | STT_NOTYPE  | `.LC0:`, `.LC1:`    |
| Mutable global| `.data`  | STB_GLOBAL | STT_OBJECT  | `my_global:`        |

## Undefined Symbols

Symbols referenced but not defined in this object file appear with `st_shndx = 0` (SHN_UNDEF). They always have `STB_GLOBAL` binding, because the linker must resolve them against another object or shared library.
