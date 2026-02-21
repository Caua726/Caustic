# Section Merging

When multiple object files are linked together, caustic-ld concatenates matching sections from
each input into a single combined section in the output. This is implemented in
`caustic-linker/linker.cst`.

## Merge Strategy

Sections with the same name are concatenated in the order the input files were specified on the
command line. The linker tracks a running byte offset for each section type so that symbol
addresses can be adjusted to their new positions in the merged output.

```
Input files:     lib.o              main.o
                 ------             -------
.text            [fn_a] [fn_b]      [main]
.data            [lib_data]         [main_data]
.rodata          [lib_str]          [main_str]

Merged output:
.text            [fn_a] [fn_b] | [main]
                  ^              ^
                  offset=0       offset=size(lib.o .text)
.data            [lib_data] | [main_data]
.rodata          [lib_str]  | [main_str]
```

## Section Types Merged

| Section  | Flags      | Content                          |
|----------|------------|----------------------------------|
| .text    | R+X        | Machine code from all input .o   |
| .data    | R+W        | Initialized mutable globals      |
| .rodata  | R          | Read-only data (string literals) |
| .bss     | R+W (zero) | Zero-initialized globals         |

## Symbol Offset Adjustment

Each symbol defined in an input object has a value (st_value) that is an offset within its
original section. After merging, that offset must be adjusted by the amount of section data
that came before it from earlier input files.

```
Example:
  lib.o   .text size = 128 bytes
  main.o  fn main at offset 0 in its .text

After merge:
  main's address = base_of_merged_text + 128 + 0 = base + 128
```

The linker stores a per-object, per-section contribution offset. When building the global
symbol table, each symbol's final address is:

```
final_address = section_load_address + section_contribution_offset[obj] + symbol.st_value
```

## BSS Handling

.bss sections contain no file data — only a size. The linker accumulates the total .bss size
from all input objects. In the output ELF, .bss is placed after .data and .rodata in the R+W
PT_LOAD segment. The OS maps it as anonymous zero-filled memory; nothing is written to the file.

```
merged_bss_size = sum of all input .bss sh_size values
```

## Section Alignment

Sections in the merged output are aligned to 16-byte boundaries. Before appending each input
section's data, padding bytes (0x00 for .text/.data/.rodata, counted into .bss) are inserted
so the section starts on an aligned offset.

## Order of Sections in Output

The output ELF places sections in the following order:

```
[ELF header]
[Program headers]
[_start stub]         — 28 bytes of entry point code
[merged .text]        — all input .text concatenated
[merged .rodata]      — all input .rodata concatenated
[merged .data]        — all input .data concatenated
[.bss]                — zero-filled, size only (no file bytes)

(dynamic mode only):
[.plt]
[.got]
[.dynamic]
[.dynsym]
[.dynstr]
[.hash]
[.rela.plt]
```

## Relocation Offset Adjustment

Relocation entries also carry offsets into their target section. After merging, each relocation's
r_offset must be incremented by the same contribution offset applied to the symbols of that
object file:

```
adjusted_r_offset = original_r_offset + section_contribution_offset[obj]
```

All relocations from all input objects are gathered into a single list and processed together
during the relocation application phase.

## Size Calculation

Section sizes must be computed before the ELF layout is finalized, because segment sizes and
load addresses depend on them. caustic-ld performs a dry-run merge pass to total up all section
sizes, then computes the final virtual address layout, then performs the actual data copy and
relocation application in a second pass.
