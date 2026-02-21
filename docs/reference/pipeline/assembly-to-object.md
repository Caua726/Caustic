# Assembly to Object (caustic-as)

The assembler (`caustic-as`) is a standalone tool that transforms x86_64 assembly text (`.s` files) into ELF64 relocatable object files (`.o` files). It is written entirely in Caustic and has no external dependencies.

## Files

| File | Purpose |
|------|---------|
| `caustic-assembler/main.cst` | CLI entry point, section management, orchestration of the assembly pipeline |
| `caustic-assembler/lexer.cst` | Assembly tokenizer: recognizes instructions, registers, labels, directives, immediates, and memory operands |
| `caustic-assembler/encoder.cst` | x86_64 instruction encoder: REX prefixes, ModR/M, SIB, immediates, displacement encoding; two-pass architecture |
| `caustic-assembler/elf.cst` | ELF64 object writer: generates section headers, symbol tables, relocation entries |

## Usage

```bash
./caustic-as input.s          # produces input.s.o
```

The output filename is always the input filename with `.o` appended.

## Pipeline

```
  input.s
    |
    | (read file into memory)
    v
  [Tokenize]  lexer.cst
    |
    | (array of ParsedLine structs)
    v
  [Pass 1]    encoder.cst
    |  Collect symbols: labels, directives, global declarations
    |  Compute section offsets for each symbol
    v
  [Pass 2]    encoder.cst
    |  Encode instructions to machine code
    |  Emit data directives (.string, .byte, .quad, etc.)
    |  Record relocations for unresolved symbol references
    v
  [ELF Write] elf.cst
    |  Write ELF64 header, section headers
    |  Write .text, .data, .rodata, .bss content
    |  Write .symtab, .strtab
    |  Write .rela.text relocations
    v
  input.s.o
```

## Assembly Tokenization

The lexer reads the `.s` file line by line, producing `ParsedLine` structs. Each line is classified as one of:

| Line Kind | Value | Description |
|-----------|-------|-------------|
| `LINE_EMPTY` | 0 | Blank line or comment-only line |
| `LINE_LABEL` | 1 | Label definition (e.g., `main:`, `.L0:`) |
| `LINE_INST` | 2 | Instruction (e.g., `mov rax, rbx`) |
| `LINE_DIRECTIVE` | 3 | Assembler directive (e.g., `.text`, `.globl main`) |

Each `ParsedLine` contains:

```cst
struct ParsedLine {
    kind as i32;           // LINE_* constant
    section as i32;        // current section (SECTION_TEXT, SECTION_DATA, ...)
    inst_id as i32;        // instruction identifier
    // operand 1 (flattened)
    op1_kind as i32;       // register, immediate, memory, label
    op1_reg_id as i32;
    op1_imm as i64;
    op1_mem_base as i32;
    op1_mem_disp as i64;
    op1_mem_size as i32;
    op1_label_ptr as *u8;
    op1_label_len as i32;
    // operand 2 (flattened, same fields)
    op2_kind as i32;
    op2_reg_id as i32;
    op2_imm as i64;
    op2_mem_base as i32;
    op2_mem_disp as i64;
    op2_mem_size as i32;
    op2_label_ptr as *u8;
    op2_label_len as i32;
    // label/directive info
    label_ptr as *u8;
    label_len as i32;
    ...
}
```

Operands are parsed inline, recognizing:

- **Registers**: rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15, and their 32/16/8-bit variants (eax, ax, al, etc.), plus xmm0-xmm15
- **Immediates**: Decimal and hexadecimal integer constants
- **Memory operands**: `[base + disp]`, `[base]`, `[rip + label]`, `QWORD PTR [...]`, `DWORD PTR [...]`, etc.
- **Labels**: Symbol references in instruction operands

## Supported Directives

| Directive | ID | Description |
|-----------|-----|-------------|
| `.text` | 1 | Switch to .text section |
| `.data` | 2 | Switch to .data section |
| `.rodata` / `.section .rodata` | 3 | Switch to .rodata section |
| `.bss` | 4 | Switch to .bss section |
| `.globl <name>` | 5 | Declare a global symbol |
| `.string "..."` | 6 | Emit a null-terminated string |
| `.byte <n>` | 7 | Emit a byte |
| `.word <n>` | 8 | Emit a 16-bit value |
| `.long <n>` | 9 | Emit a 32-bit value |
| `.quad <n>` | 10 | Emit a 64-bit value |
| `.zero <n>` | 11 | Emit n zero bytes |
| `.section <name>` | 12 | Switch to named section |
| `.intel_syntax noprefix` | 13 | Set Intel syntax mode (expected at file start) |

## Two-Pass Assembly

### Pass 1: Symbol Collection

The first pass scans all parsed lines to:

1. Track the current section (`.text`, `.data`, `.rodata`, `.bss`)
2. Record label definitions with their section and offset within that section
3. Mark global symbols (from `.globl` directives)
4. Compute instruction sizes to determine label offsets accurately
5. Process data directives to track section sizes

After pass 1, every label has a known section and offset.

### Pass 2: Instruction Encoding

The second pass generates actual machine code:

1. For each instruction, encode the x86_64 binary representation:
   - **REX prefix**: Set when using 64-bit operands or extended registers (r8-r15)
   - **Opcode**: One or two bytes depending on the instruction
   - **ModR/M byte**: Encodes register/memory operand addressing modes
   - **SIB byte**: Scale-Index-Base, used for complex memory addressing
   - **Displacement**: 8-bit or 32-bit offset for memory operands
   - **Immediate**: 8, 16, 32, or 64-bit constant operands

2. For data directives, emit raw bytes into the appropriate section buffer

3. When an instruction references a symbol that is in a different section or is external, a **relocation entry** is recorded instead of the final address

## Relocation Types

The assembler generates relocations for symbol references that cannot be resolved at assembly time:

| Relocation | Description |
|------------|-------------|
| `R_X86_64_PC32` | 32-bit PC-relative offset (used for `call`, `jmp`, RIP-relative addressing) |
| `R_X86_64_32S` | 32-bit signed absolute address |
| `R_X86_64_PLT32` | 32-bit PC-relative PLT entry (for calls to external functions) |

Each relocation entry records:

- The offset within the section where the fixup is needed
- The symbol being referenced
- The relocation type
- An addend value (typically -4 for PC-relative relocations, accounting for the instruction pointer advance)

## ELF Output

The generated `.o` file is a standard ELF64 relocatable object with these sections:

| Section | Description |
|---------|-------------|
| `.text` | Machine code (executable instructions) |
| `.data` | Initialized global data |
| `.rodata` | Read-only data (string literals, float constants) |
| `.bss` | Zero-initialized data (only size recorded, no file content) |
| `.symtab` | Symbol table (labels, globals, externs) |
| `.strtab` | String table for symbol names |
| `.rela.text` | Relocation entries for the .text section |
| `.shstrtab` | Section header string table |

### ELF Header

- `e_type`: `ET_REL` (1) -- relocatable object
- `e_machine`: `EM_X86_64` (62)
- `e_entry`: 0 (no entry point for .o files)

### Symbol Table Entries

Each symbol has:

- Name (index into `.strtab`)
- Value (offset within its section)
- Size
- Binding: `STB_LOCAL` or `STB_GLOBAL`
- Type: `STT_NOTYPE`, `STT_FUNC`, or `STT_SECTION`
- Section index: which section the symbol is defined in (or `SHN_UNDEF` for external references)

## Heap Sizing

The assembler dynamically sizes its heap based on the input file size: `file_size * 30 + 4MB`, with a minimum of 8MB. This scaling factor accounts for the expansion from assembly text to internal data structures and was tuned to handle files exceeding 1.8MB.
