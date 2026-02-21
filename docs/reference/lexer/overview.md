# Lexer Overview

The Caustic lexer is the first phase of the compiler pipeline. It performs single-pass character scanning over a source buffer, producing a flat array of tokens that the parser consumes.

## Input and Output

- **Input**: Raw source code as a `*u8` buffer, read entirely into memory before scanning begins.
- **Output**: A `TokenList`, which is a dynamically-grown array of `Token` structs.

## Source Files

The lexer implementation is split across three files in `src/lexer/`:

| File | Purpose |
|------|---------|
| `lexer.cst` | Main scanner loop and character dispatch |
| `tokens.cst` | Token kind constants, Token struct definition, TokenList struct |
| `util.cst` | Helper functions for character classification and token construction |

## Token Struct

Each token is a 32-byte packed struct with the following layout:

```
struct Token {
    kind    as i32;   // Token kind (TK_EOF=0 through TK_XOR_EQ=76)
    ptr     as *u8;   // Pointer into original source buffer
    len     as i32;   // Length of the token text in bytes
    line    as i32;   // 1-based line number
    col     as i32;   // 1-based column number
    int_val as i64;   // Pre-parsed integer value (for TK_INTEGER tokens)
}
```

Total size: 32 bytes (packed, no alignment padding).

## Zero-Copy Design

The lexer does not allocate memory for token text. Each token's `ptr` field points directly into the original source buffer, and `len` records the byte count. This avoids per-token string allocations and copies. The source buffer must remain valid for the entire lifetime of the token list.

## Scanning Algorithm

The lexer uses a single-pass, character-at-a-time scanning strategy:

1. Skip whitespace and comments (line comments with `//`).
2. Record the current position as the token start, along with line and column.
3. Dispatch on the current character:
   - Alphabetic or `_`: scan an identifier or keyword.
   - Digit: scan a numeric literal (integer or float).
   - `"`: scan a string literal with escape handling.
   - `'`: scan a character literal with escape handling.
   - Operator or punctuation character: scan one or two characters, using lookahead for multi-character tokens like `==`, `<=`, `<<`, `+=`, and `...`.
4. Append the resulting `Token` to the `TokenList`.
5. Repeat until end of input, then emit `TK_EOF`.

## Keyword Recognition

After scanning an identifier, the lexer checks whether the text matches one of Caustic's 27 reserved keywords. Keyword lookup is performed by dispatching on the identifier's length first, then comparing against known keywords of that length. This avoids hash table overhead while keeping lookup fast for a small keyword set.

## TokenList

The `TokenList` is a dynamically-sized array that grows as tokens are appended. It provides indexed access for the parser, which walks through tokens sequentially using a position cursor.

## Error Handling

Lexer errors are reported with line and column information. Common error conditions include:

- Unterminated string literals (missing closing `"`).
- Unterminated character literals (missing closing `'`).
- Unrecognized characters that do not match any token pattern.

## Debug Mode

The compiler's `-debuglexer` flag runs only the lexer phase and prints each token with its kind, text, line, and column. This is useful for diagnosing tokenization issues:

```bash
./caustic -debuglexer source.cst
```
