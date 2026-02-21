# Token Types

The Caustic lexer recognizes 77 distinct token kinds, numbered from `TK_EOF=0` through `TK_XOR_EQ=76`. Each token kind is represented as an `i32` constant in the `Token.kind` field.

## Complete Token Kind Table

### Literals and Identifiers (0-5)

| Kind | Value | Description |
|------|-------|-------------|
| `TK_EOF` | 0 | End of file marker |
| `TK_IDENT` | 1 | Identifier (variable name, type name, etc.) |
| `TK_INTEGER` | 2 | Integer literal (decimal digits, value stored in `int_val`) |
| `TK_FLOAT` | 3 | Floating-point literal (digits with decimal point) |
| `TK_STRING` | 4 | String literal (double-quoted, with escape processing) |
| `TK_CHAR` | 5 | Character literal (single-quoted, with escape processing) |

### Arithmetic Operators (6-10)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_PLUS` | 6 | `+` | Addition |
| `TK_MINUS` | 7 | `-` | Subtraction or unary negation |
| `TK_STAR` | 8 | `*` | Multiplication or pointer dereference/type |
| `TK_SLASH` | 9 | `/` | Division |
| `TK_PERCENT` | 10 | `%` | Modulo |

### Comparison Operators (11-17)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_EQ` | 11 | `==` | Equal to |
| `TK_NEQ` | 12 | `!=` | Not equal to |
| `TK_LT` | 13 | `<` | Less than |
| `TK_GT` | 14 | `>` | Greater than |
| `TK_LE` | 15 | `<=` | Less than or equal |
| `TK_GE` | 16 | `>=` | Greater than or equal |
| `TK_NOT` | 17 | (reserved slot) | — |

### Logical and Bitwise Operators (18-26)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_NOT` | 18 | `!` | Logical NOT |
| `TK_BAND` | 19 | `&` | Bitwise AND or address-of |
| `TK_BOR` | 20 | `\|` | Bitwise OR |
| `TK_BXOR` | 21 | `^` | Bitwise XOR |
| `TK_BNOT` | 22 | `~` | Bitwise NOT |
| `TK_LAND` | 23 | `&&` | Logical AND |
| `TK_LOR` | 24 | `\|\|` | Logical OR |
| `TK_SHL` | 25 | `<<` | Shift left |
| `TK_SHR` | 26 | `>>` | Shift right |

### Special Operators (27-28)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_ASSIGN` | 27 | `=` | Assignment |
| `TK_ARROW` | 28 | (reserved) | Arrow operator |

### Delimiters and Punctuation (29-39)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_LPAREN` | 29 | `(` | Left parenthesis |
| `TK_RPAREN` | 30 | `)` | Right parenthesis |
| `TK_LBRACE` | 31 | `{` | Left brace |
| `TK_RBRACE` | 32 | `}` | Right brace |
| `TK_LBRACKET` | 33 | `[` | Left bracket |
| `TK_RBRACKET` | 34 | `]` | Right bracket |
| `TK_COMMA` | 35 | `,` | Comma |
| `TK_DOT` | 36 | `.` | Dot (member access) |
| `TK_SEMICOLON` | 37 | `;` | Semicolon |
| `TK_COLON` | 38 | `:` | Colon |
| `TK_ELLIPSIS` | 39 | `...` | Ellipsis (variadic) |

### Keywords (40-66)

| Kind | Value | Keyword | Description |
|------|-------|---------|-------------|
| `TK_FN` | 40 | `fn` | Function declaration |
| `TK_IS` | 41 | `is` | Type annotation marker |
| `TK_AS` | 42 | `as` | Name binding / alias |
| `TK_IF` | 43 | `if` | Conditional branch |
| `TK_DO` | 44 | `do` | Do-while loop |
| `TK_LET` | 45 | `let` | Variable declaration |
| `TK_FOR` | 46 | `for` | For loop |
| `TK_ASM` | 47 | `asm` | Inline assembly |
| `TK_USE` | 48 | `use` | Module import |
| `TK_GEN` | 49 | `gen` | Generic type parameter |
| `TK_WITH` | 50 | `with` | Global qualifier (mut/imut) |
| `TK_ELSE` | 51 | `else` | Else branch |
| `TK_CASE` | 52 | `case` | Match case arm |
| `TK_CAST` | 53 | `cast` | Type cast expression |
| `TK_ENUM` | 54 | `enum` | Enum declaration |
| `TK_IMPL` | 55 | `impl` | Implementation block |
| `TK_WHILE` | 56 | `while` | While loop |
| `TK_BREAK` | 57 | `break` | Loop break |
| `TK_MATCH` | 58 | `match` | Match expression |
| `TK_DEFER` | 59 | `defer` | Deferred function call |
| `TK_RETURN` | 60 | `return` | Function return |
| `TK_STRUCT` | 61 | `struct` | Struct declaration |
| `TK_SIZEOF` | 62 | `sizeof` | Size of type |
| `TK_EXTERN` | 63 | `extern` | External function declaration |
| `TK_FN_PTR` | 64 | `fn_ptr` | Function pointer creation |
| `TK_SYSCALL` | 65 | `syscall` | Direct Linux syscall |
| `TK_CONTINUE` | 66 | `continue` | Loop continue |

### Compound Assignment Operators (67-76)

| Kind | Value | Symbol | Description |
|------|-------|--------|-------------|
| `TK_PLUS_EQ` | 67 | `+=` | Add and assign |
| `TK_MINUS_EQ` | 68 | `-=` | Subtract and assign |
| `TK_STAR_EQ` | 69 | `*=` | Multiply and assign |
| `TK_SLASH_EQ` | 70 | `/=` | Divide and assign |
| `TK_PERCENT_EQ` | 71 | `%=` | Modulo and assign |
| `TK_SHL_EQ` | 72 | `<<=` | Shift left and assign |
| `TK_SHR_EQ` | 73 | `>>=` | Shift right and assign |
| `TK_BAND_EQ` | 74 | `&=` | Bitwise AND and assign |
| `TK_BOR_EQ` | 75 | `\|=` | Bitwise OR and assign |
| `TK_XOR_EQ` | 76 | `^=` | Bitwise XOR and assign |

## Token Kind Categories

For reference, the token kinds fall into these logical groups:

- **Literals/Identifiers** (0-5): Raw values from source text.
- **Binary Operators** (6-26): Arithmetic, comparison, logical, and bitwise operators.
- **Special** (27-28): Assignment and arrow.
- **Delimiters** (29-39): Grouping and separation punctuation.
- **Keywords** (40-66): Reserved words with dedicated token kinds.
- **Compound Assignment** (67-76): Shorthand `op=` operators.

## Notes

- `TK_STAR` (8) is overloaded: it represents multiplication in expressions and the pointer sigil in type syntax (`*T`). The parser disambiguates based on context.
- `TK_BAND` (19) similarly represents both bitwise AND in expressions and the address-of operator (`&x`) in unary prefix position.
- `TK_MINUS` (7) represents both subtraction and unary negation.
- The `TK_ELLIPSIS` (39) token is three characters (`...`) and is used for variadic parameter syntax.
