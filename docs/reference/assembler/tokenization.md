# Assembly Tokenization

The tokenizer in `lexer.cst` converts raw assembly source text into a flat array of `Token` structs. It processes the entire input in a single pass, character by character, producing tokens that the parser then groups into structured `ParsedLine` records.

## Token Kinds

| Constant | Value | Description |
|----------|-------|-------------|
| `TK_EOF` | 0 | End of file marker |
| `TK_NEWLINE` | 1 | Line terminator (newline character) |
| `TK_IDENT` | 2 | Identifier: label name, local label (`.L0`, `.LC0`), or unrecognized word |
| `TK_NUMBER` | 3 | Numeric literal (decimal or hexadecimal) |
| `TK_STRING` | 4 | String literal enclosed in double quotes |
| `TK_COMMA` | 5 | `,` operand separator |
| `TK_COLON` | 6 | `:` label terminator |
| `TK_LBRACKET` | 7 | `[` memory operand start |
| `TK_RBRACKET` | 8 | `]` memory operand end |
| `TK_PLUS` | 9 | `+` displacement add |
| `TK_MINUS` | 10 | `-` displacement subtract or negative immediate |
| `TK_STAR` | 11 | `*` (reserved, not currently used in operand parsing) |
| `TK_REGISTER` | 13 | Register name (e.g., `rax`, `xmm0`, `r8d`) |
| `TK_INSTRUCTION` | 14 | Instruction mnemonic (e.g., `mov`, `add`, `syscall`) |
| `TK_DIRECTIVE` | 15 | Assembler directive (e.g., `.text`, `.globl`, `.string`) |
| `TK_SIZE_PREFIX` | 16 | Size prefix: `QWORD PTR`, `DWORD PTR`, `WORD PTR`, `BYTE PTR` |

## Token Struct

```cst
struct Token {
    kind as i32;        // TK_* constant
    ptr as *u8;         // pointer into source buffer (not null-terminated)
    len as i32;         // length of token text
    line as i32;        // source line number (1-based)
    num_val as i64;     // parsed numeric value (for TK_NUMBER)
    reg_id as i32;      // register ID (for TK_REGISTER)
    inst_id as i32;     // instruction ID (for TK_INSTRUCTION)
    size_val as i32;    // operand size in bits (for TK_SIZE_PREFIX: 8, 16, 32, or 64)
}
```

Tokens store a pointer directly into the source buffer rather than copying the text. This is safe because the source buffer remains allocated for the entire assembly process.

## Tokenization Rules

### Whitespace and Comments

- Spaces (0x20), tabs (0x09), and carriage returns (0x0D) are skipped silently.
- Newlines (0x0A) emit a `TK_NEWLINE` token and increment the line counter.
- Comments start with `#` or `//` and extend to the end of the line. The comment text is skipped entirely.

### Numeric Literals

Decimal and hexadecimal literals are supported:

- **Decimal**: A sequence of ASCII digits `0-9`. Example: `42`, `1024`.
- **Hexadecimal**: Starts with `0x` or `0X`, followed by hex digits `0-9`, `a-f`, `A-F`. Example: `0xFF`, `0x400000`.

The `parse_number()` function extracts the value, and `count_number_len()` determines how many characters to consume. The parsed value is stored in `tok.num_val`.

### String Literals

Strings are delimited by double quotes (`"`). Escape sequences within strings are recognized but not decoded at the tokenization stage -- that happens later in `decode_string()` during pass 2. The tokenizer handles backslash escaping to correctly find the closing quote (a `\"` inside a string does not terminate it).

### Identifiers and Keywords

An identifier starts with a letter (`a-z`, `A-Z`) or underscore (`_`) and continues with alphanumeric characters or dots (`.`). Once the full identifier text is extracted, it is checked in the following order:

1. **Size prefix**: If the identifier is `QWORD`, `DWORD`, `WORD`, or `BYTE`, the tokenizer looks ahead for a following `PTR` keyword. If found, the token becomes `TK_SIZE_PREFIX` with `size_val` set to 64, 32, 16, or 8 respectively. If `PTR` is not found, the position is restored and the identifier is treated normally.

2. **Register**: The identifier is checked against all known register names via `lookup_register()`. If matched, the token becomes `TK_REGISTER` with the appropriate `reg_id`. See the instruction encoding reference for the full register table.

3. **Instruction**: If not a register, the identifier is checked against all instruction mnemonics via `lookup_instruction()`. If matched, the token becomes `TK_INSTRUCTION` with the appropriate `inst_id`.

4. **Skip words**: The identifiers `noprefix` and `PTR` (when not part of a size prefix) are silently skipped.

5. **Fallback**: Any unrecognized identifier becomes `TK_IDENT`. This covers label names, function names, and local labels.

### Directives

Tokens starting with a dot (`.`) are scanned as a dot followed by alphanumeric characters and additional dots. The resulting string is checked against the set of known directives:

- `.intel_syntax`, `.text`, `.data`, `.rodata`, `.bss`
- `.globl`, `.global`, `.section`
- `.string`, `.asciz`, `.ascii`
- `.byte`, `.word`, `.value`, `.long`, `.quad`, `.zero`

If the dot-prefixed token matches a known directive, it becomes `TK_DIRECTIVE`. Otherwise, it is classified as `TK_IDENT` -- this handles local labels like `.L0`, `.LC0`, `.LBB0_1`.

### Punctuation

Single-character tokens are produced for: `,` (comma), `:` (colon), `[` (left bracket), `]` (right bracket), `+` (plus), `-` (minus), `*` (star).

The minus sign is always emitted as `TK_MINUS`. Negative numbers are handled at the parsing stage by combining a `TK_MINUS` followed by a `TK_NUMBER`.

## TokenList

Tokens are stored in a dynamically-growing array:

```cst
struct TokenList {
    data as *u8;    // pointer to token array
    count as i32;   // number of tokens
    cap as i32;     // allocated capacity
}
```

The initial capacity is estimated as `src_len / 6 + 4096` (roughly 2 tokens per 12 bytes of source). When capacity is exceeded, the array doubles in size. Access is via `tl_get(tl, idx)` which returns a pointer to the token at the given index.

An `TK_EOF` token is always appended at the end of the list to serve as a sentinel.

## Intel Syntax

caustic-as uses Intel syntax exclusively. The `.intel_syntax noprefix` directive at the start of assembly files is recognized and skipped. AT&T syntax is not supported.

In Intel syntax:
- Destination operand comes first: `mov rax, rbx` (rax = rbx)
- Memory operands use square brackets: `mov rax, [rbp - 8]`
- Size prefixes use `QWORD PTR`, `DWORD PTR`, etc.: `mov QWORD PTR [rbp - 8], rax`
- No `%` prefix on registers, no `$` prefix on immediates
