# Operators and Punctuation

The lexer recognizes a variety of single-character and multi-character operator and punctuation tokens. Multi-character tokens require one or two characters of lookahead to disambiguate.

## Single-Character Tokens

These tokens consist of exactly one character and require no lookahead (unless the next character forms a multi-character token):

| Character | Token Kind | Value | Description |
|-----------|-----------|-------|-------------|
| `+` | `TK_PLUS` | 6 | Addition |
| `-` | `TK_MINUS` | 7 | Subtraction / unary negation |
| `*` | `TK_STAR` | 8 | Multiplication / pointer sigil |
| `/` | `TK_SLASH` | 9 | Division |
| `%` | `TK_PERCENT` | 10 | Modulo |
| `=` | `TK_ASSIGN` | 27 | Assignment |
| `<` | `TK_LT` | 13 | Less than |
| `>` | `TK_GT` | 14 | Greater than |
| `!` | `TK_NOT` | 18 | Logical NOT |
| `&` | `TK_BAND` | 19 | Bitwise AND / address-of |
| `\|` | `TK_BOR` | 20 | Bitwise OR |
| `^` | `TK_BXOR` | 21 | Bitwise XOR |
| `~` | `TK_BNOT` | 22 | Bitwise NOT |
| `(` | `TK_LPAREN` | 29 | Left parenthesis |
| `)` | `TK_RPAREN` | 30 | Right parenthesis |
| `{` | `TK_LBRACE` | 31 | Left brace |
| `}` | `TK_RBRACE` | 32 | Right brace |
| `[` | `TK_LBRACKET` | 33 | Left bracket |
| `]` | `TK_RBRACKET` | 34 | Right bracket |
| `,` | `TK_COMMA` | 35 | Comma separator |
| `.` | `TK_DOT` | 36 | Member access |
| `;` | `TK_SEMICOLON` | 37 | Statement terminator |
| `:` | `TK_COLON` | 38 | Colon |

## Two-Character Tokens

These tokens require one character of lookahead. The lexer checks the next character after the first to determine whether to emit a two-character token or fall back to the single-character version.

| Sequence | Token Kind | Value | Composed From |
|----------|-----------|-------|---------------|
| `==` | `TK_EQ` | 11 | `=` + `=` |
| `!=` | `TK_NEQ` | 12 | `!` + `=` |
| `<=` | `TK_LE` | 15 | `<` + `=` |
| `>=` | `TK_GE` | 16 | `>` + `=` |
| `<<` | `TK_SHL` | 25 | `<` + `<` |
| `>>` | `TK_SHR` | 26 | `>` + `>` |
| `&&` | `TK_LAND` | 23 | `&` + `&` |
| `\|\|` | `TK_LOR` | 24 | `\|` + `\|` |
| `+=` | `TK_PLUS_EQ` | 67 | `+` + `=` |
| `-=` | `TK_MINUS_EQ` | 68 | `-` + `=` |
| `*=` | `TK_STAR_EQ` | 69 | `*` + `=` |
| `/=` | `TK_SLASH_EQ` | 70 | `/` + `=` |
| `%=` | `TK_PERCENT_EQ` | 71 | `%` + `=` |
| `&=` | `TK_BAND_EQ` | 74 | `&` + `=` |
| `\|=` | `TK_BOR_EQ` | 75 | `\|` + `=` |
| `^=` | `TK_XOR_EQ` | 76 | `^` + `=` |

## Three-Character Tokens

These tokens require two characters of lookahead:

| Sequence | Token Kind | Value | Description |
|----------|-----------|-------|-------------|
| `<<=` | `TK_SHL_EQ` | 72 | Shift left and assign |
| `>>=` | `TK_SHR_EQ` | 73 | Shift right and assign |
| `...` | `TK_ELLIPSIS` | 39 | Variadic parameter marker |

## Lookahead and Disambiguation

The lexer uses a greedy, longest-match approach for operator scanning. When a character can start multiple tokens, the lexer checks subsequent characters to select the longest match:

### `<` disambiguation

1. Check next character:
   - `=` produces `TK_LE` (`<=`)
   - `<` proceeds to check one more:
     - `=` produces `TK_SHL_EQ` (`<<=`)
     - otherwise produces `TK_SHL` (`<<`)
   - otherwise produces `TK_LT` (`<`)

### `>` disambiguation

1. Check next character:
   - `=` produces `TK_GE` (`>=`)
   - `>` proceeds to check one more:
     - `=` produces `TK_SHR_EQ` (`>>=`)
     - otherwise produces `TK_SHR` (`>>`)
   - otherwise produces `TK_GT` (`>`)

### `=` disambiguation

1. Check next character:
   - `=` produces `TK_EQ` (`==`)
   - otherwise produces `TK_ASSIGN` (`=`)

### `!` disambiguation

1. Check next character:
   - `=` produces `TK_NEQ` (`!=`)
   - otherwise produces `TK_NOT` (`!`)

### `&` disambiguation

1. Check next character:
   - `&` produces `TK_LAND` (`&&`)
   - `=` produces `TK_BAND_EQ` (`&=`)
   - otherwise produces `TK_BAND` (`&`)

### `|` disambiguation

1. Check next character:
   - `|` produces `TK_LOR` (`||`)
   - `=` produces `TK_BOR_EQ` (`|=`)
   - otherwise produces `TK_BOR` (`|`)

### `.` disambiguation

1. Check next two characters:
   - `..` (two more dots) produces `TK_ELLIPSIS` (`...`)
   - otherwise produces `TK_DOT` (`.`)

### `/` disambiguation

1. Check next character:
   - `/` begins a line comment (skip to end of line, no token emitted)
   - `=` produces `TK_SLASH_EQ` (`/=`)
   - otherwise produces `TK_SLASH` (`/`)

## Compound Assignment Operators

All compound assignment operators follow the pattern `<op>=`:

```cst
x += 1;     // x = x + 1
x -= 1;     // x = x - 1
x *= 2;     // x = x * 2
x /= 2;     // x = x / 2
x %= 3;     // x = x % 3
x <<= 4;    // x = x << 4
x >>= 4;    // x = x >> 4
x &= mask;  // x = x & mask
x |= flag;  // x = x | flag
x ^= bits;  // x = x ^ bits
```

These are tokenized as single compound tokens (not as separate operator and assignment tokens), which simplifies parser handling.

## Contextual Overloading

Several tokens have different meanings depending on parsing context. The lexer produces the same token kind regardless; disambiguation happens in the parser:

| Token | Context 1 | Context 2 |
|-------|-----------|-----------|
| `TK_STAR` | Multiplication (`a * b`) | Pointer type (`*u8`) or dereference (`*ptr`) |
| `TK_BAND` | Bitwise AND (`a & b`) | Address-of (`&x`) |
| `TK_MINUS` | Subtraction (`a - b`) | Unary negation (`-x`) |
| `TK_LT` / `TK_GT` | Comparison (`a < b`) | Generic syntax boundary (before `gen` keyword) |
