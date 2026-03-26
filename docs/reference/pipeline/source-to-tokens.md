# Source to Tokens

The lexer is the first phase of the Caustic compiler. It reads raw source bytes and produces a flat array of tokens suitable for consumption by the parser.

## Files

| File | Purpose |
|------|---------|
| `src/lexer/tokens.cst` | Token kind constants, `Token` and `TokenList` structs, keyword lookup table |
| `src/lexer/lexer.cst` | Main tokenizer: character-by-character scanning, error reporting |
| `src/lexer/util.cst` | Character classification helpers and ASCII constants |

## Input

The lexer receives three arguments:

- `src` (`*u8`): pointer to the source file contents in memory
- `file_size` (`i64`): byte length of the source
- `filename` (`*u8`): null-terminated filename string (used for error messages)

The source buffer is read into memory by `main.cst` using raw `open`/`read`/`lseek` syscalls. A null terminator is appended at `src[file_size]`.

## Output

The lexer returns a `TokenList`:

```cst
struct TokenList {
    data as *u8;    // pointer to Token array (heap-allocated)
    count as i32;   // number of tokens produced
    cap as i32;     // capacity of the array
}
```

The `TokenList` is a growable flat array. It starts with a capacity of 8192 tokens and doubles when full. Individual tokens are accessed by index via `tl_get()`.

## Token Structure

Each token is a 32-byte packed struct:

```cst
struct Token {
    kind as i32;      // token kind (TK_* constant)
    ptr as *u8;       // pointer into source buffer (zero-copy)
    len as i32;       // byte length of the token text
    line as i32;      // 1-based line number
    col as i32;       // 1-based column number
    int_val as i64;   // pre-parsed integer value (for TK_INTEGER)
}
```

Tokens do not copy their text. The `ptr` field points directly into the original source buffer, and `len` gives the span. This zero-copy design means the source buffer must remain valid for the lifetime of the token list.

For integer literals, the lexer pre-computes the numeric value and stores it in `int_val`, so the parser does not need to re-parse digit strings.

## Token Kinds

There are 80 token kinds, numbered 0 through 79:

### Literals (0-5)

| Constant | Value | Description |
|----------|-------|-------------|
| `TK_EOF` | 0 | End of file |
| `TK_IDENT` | 1 | Identifier (variable/function/type name) |
| `TK_INTEGER` | 2 | Integer literal (decimal) |
| `TK_FLOAT` | 3 | Floating-point literal |
| `TK_STRING` | 4 | String literal (between double quotes) |
| `TK_CHAR` | 5 | Character literal (between single quotes) |

### Arithmetic Operators (6-10)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_PLUS` | 6 | `+` |
| `TK_MINUS` | 7 | `-` |
| `TK_STAR` | 8 | `*` |
| `TK_SLASH` | 9 | `/` |
| `TK_PERCENT` | 10 | `%` |

### Comparison Operators (11-17)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_EQ` | 11 | `=` |
| `TK_EQEQ` | 12 | `==` |
| `TK_NE` | 13 | `!=` |
| `TK_LT` | 14 | `<` |
| `TK_LE` | 15 | `<=` |
| `TK_GT` | 16 | `>` |
| `TK_GE` | 17 | `>=` |

### Logical and Bitwise Operators (18-26)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_NOT` | 18 | `!` |
| `TK_AND` | 19 | `&` |
| `TK_OR` | 20 | `\|` |
| `TK_AND_AND` | 21 | `&&` |
| `TK_OR_OR` | 22 | `\|\|` |
| `TK_XOR` | 23 | `^` |
| `TK_BITNOT` | 24 | `~` |
| `TK_SHL` | 25 | `<<` |
| `TK_SHR` | 26 | `>>` |

### Special (27-28)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_ASSIGN` | 27 | `:=` (unused / reserved) |
| `TK_ARROW` | 28 | `->` |

### Delimiters (29-39)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_LPAREN` | 29 | `(` |
| `TK_RPAREN` | 30 | `)` |
| `TK_LBRACE` | 31 | `{` |
| `TK_RBRACE` | 32 | `}` |
| `TK_LBRACKET` | 33 | `[` |
| `TK_RBRACKET` | 34 | `]` |
| `TK_COMMA` | 35 | `,` |
| `TK_DOT` | 36 | `.` |
| `TK_SEMICOLON` | 37 | `;` |
| `TK_COLON` | 38 | `:` |
| `TK_ELLIPSIS` | 39 | `...` |

### Keywords (40-66)

| Constant | Value | Keyword |
|----------|-------|---------|
| `TK_FN` | 40 | `fn` |
| `TK_RETURN` | 41 | `return` |
| `TK_LET` | 42 | `let` |
| `TK_IS` | 43 | `is` |
| `TK_AS` | 44 | `as` |
| `TK_WITH` | 45 | `with` |
| `TK_IF` | 46 | `if` |
| `TK_ELSE` | 47 | `else` |
| `TK_WHILE` | 48 | `while` |
| `TK_FOR` | 49 | `for` |
| `TK_DO` | 50 | `do` |
| `TK_BREAK` | 51 | `break` |
| `TK_CONTINUE` | 52 | `continue` |
| `TK_STRUCT` | 53 | `struct` |
| `TK_ENUM` | 54 | `enum` |
| `TK_MATCH` | 55 | `match` |
| `TK_CASE` | 56 | `case` |
| `TK_USE` | 57 | `use` |
| `TK_ASM` | 58 | `asm` |
| `TK_SYSCALL` | 59 | `syscall` |
| `TK_SIZEOF` | 60 | `sizeof` |
| `TK_CAST` | 61 | `cast` |
| `TK_GEN` | 62 | `gen` |
| `TK_DEFER` | 63 | `defer` |
| `TK_EXTERN` | 64 | `extern` |
| `TK_IMPL` | 65 | `impl` |
| `TK_FN_PTR` | 66 | `fn_ptr` |

### Compound Assignment (67-76)

| Constant | Value | Symbol |
|----------|-------|--------|
| `TK_PLUS_EQ` | 67 | `+=` |
| `TK_MINUS_EQ` | 68 | `-=` |
| `TK_STAR_EQ` | 69 | `*=` |
| `TK_SLASH_EQ` | 70 | `/=` |
| `TK_PERCENT_EQ` | 71 | `%=` |
| `TK_SHL_EQ` | 72 | `<<=` |
| `TK_SHR_EQ` | 73 | `>>=` |
| `TK_AND_EQ` | 74 | `&=` |
| `TK_OR_EQ` | 75 | `\|=` |
| `TK_XOR_EQ` | 76 | `^=` |

## Keyword Lookup

Keywords are identified by a length-dispatch table in `keyword_lookup()`. The function first branches on the identifier length (2 through 8), then does sequential string comparisons within each length bucket:

- **Length 2**: `fn`, `is`, `as`, `if`, `do`
- **Length 3**: `let`, `for`, `asm`, `use`, `gen`
- **Length 4**: `with`, `else`, `case`, `cast`, `enum`, `impl`
- **Length 5**: `while`, `break`, `match`, `defer`
- **Length 6**: `return`, `struct`, `sizeof`, `extern`, `fn_ptr`
- **Length 7**: `syscall`
- **Length 8**: `continue`

Any identifier that does not match a keyword returns `TK_IDENT`.

## Scanning Algorithm

The lexer uses a single `while` loop advancing through the source buffer character by character:

1. **Whitespace**: Spaces, tabs, carriage returns are skipped. Newlines increment the line counter and reset the column counter.

2. **Comments**: `//` skips to end of line.

3. **String literals**: Opening `"` scans until closing `"`, handling escape sequences (`\n`, `\t`, `\\`, `\"`, `\0`). The token spans the content between the quotes (excluding the quote characters).

4. **Character literals**: Opening `'` scans a single character or escape sequence, closing `'`. The `int_val` field stores the character code.

5. **Numbers**: Sequences of digits produce `TK_INTEGER` with the value pre-computed in `int_val`. If a `.` follows digits, it becomes `TK_FLOAT`.

6. **Identifiers and keywords**: Sequences of alphanumeric characters or underscores are scanned as a span, then passed through `keyword_lookup()` to determine if they are a keyword or a plain identifier.

7. **Operators and delimiters**: Single and multi-character operators are recognized by checking the current character and peeking at the next. Multi-character tokens (`==`, `!=`, `<=`, `>=`, `&&`, `||`, `<<`, `>>`, `->`, `...`, `+=`, etc.) take priority over their single-character prefixes.

8. **EOF**: A `TK_EOF` token is appended at the end.

## Error Reporting

Lexer errors (unexpected characters, unterminated strings) call `error_at()`, which prints the filename, line, column, error message, the offending source line, and a caret (`^`) pointing to the exact column. After printing, the compiler exits immediately.

```
Error in test.cst:5:12: unexpected character
  let is i32 as x = @;
             ^
```
