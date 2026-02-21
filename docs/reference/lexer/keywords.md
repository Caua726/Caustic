# Keywords

Caustic has 28 reserved keywords. These words cannot be used as variable names, function names, struct names, module aliases, or any other user-defined identifier. Each keyword has a dedicated token kind, so the parser never sees them as `TK_IDENT`.

## Keyword Table

| Keyword | Token Kind | Value | Length | Description |
|---------|-----------|-------|--------|-------------|
| `fn` | `TK_FN` | 40 | 2 | Function declaration |
| `is` | `TK_IS` | 41 | 2 | Type annotation in declarations |
| `as` | `TK_AS` | 42 | 2 | Name binding in declarations and imports |
| `if` | `TK_IF` | 43 | 2 | Conditional branch |
| `do` | `TK_DO` | 44 | 2 | Do-while loop |
| `let` | `TK_LET` | 45 | 3 | Variable declaration |
| `for` | `TK_FOR` | 46 | 3 | For loop |
| `asm` | `TK_ASM` | 47 | 3 | Inline assembly block |
| `use` | `TK_USE` | 48 | 3 | Module import |
| `gen` | `TK_GEN` | 49 | 3 | Generic type parameter specifier |
| `with` | `TK_WITH` | 50 | 4 | Global variable qualifier |
| `else` | `TK_ELSE` | 51 | 4 | Else branch of conditional |
| `case` | `TK_CASE` | 52 | 4 | Match arm |
| `cast` | `TK_CAST` | 53 | 4 | Type cast expression |
| `enum` | `TK_ENUM` | 54 | 4 | Enum declaration |
| `impl` | `TK_IMPL` | 55 | 4 | Implementation block for a type |
| `while` | `TK_WHILE` | 56 | 5 | While loop |
| `break` | `TK_BREAK` | 57 | 5 | Break out of loop |
| `match` | `TK_MATCH` | 58 | 5 | Pattern match expression |
| `defer` | `TK_DEFER` | 59 | 5 | Deferred function call |
| `return` | `TK_RETURN` | 60 | 6 | Return from function |
| `struct` | `TK_STRUCT` | 61 | 6 | Struct declaration |
| `sizeof` | `TK_SIZEOF` | 62 | 6 | Size of a type expression |
| `extern` | `TK_EXTERN` | 63 | 6 | External function declaration |
| `fn_ptr` | `TK_FN_PTR` | 64 | 6 | Function pointer expression |
| `syscall` | `TK_SYSCALL` | 65 | 7 | Direct Linux syscall invocation |
| `continue` | `TK_CONTINUE` | 66 | 8 | Continue to next loop iteration |
| `type` | `TK_TYPE` | 77 | 4 | Type alias declaration |

## Keyword Lookup Strategy

The lexer uses a length-dispatch approach for keyword recognition. After scanning an identifier token (a sequence of alphanumeric characters and underscores starting with a letter or underscore), the lexer checks whether it matches a keyword:

1. Switch on the identifier's byte length.
2. For each length, compare against the known keywords of that length using byte-by-byte comparison.
3. If a match is found, change the token kind from `TK_IDENT` to the corresponding `TK_*` keyword kind.
4. If no match, the token remains `TK_IDENT`.

This avoids hash table overhead. Since keywords range from 2 to 8 characters, the dispatch has at most 7 branches, each with a small number of comparisons.

### Keywords by Length

| Length | Keywords |
|--------|----------|
| 2 | `fn`, `is`, `as`, `if`, `do` |
| 3 | `let`, `for`, `asm`, `use`, `gen` |
| 4 | `with`, `else`, `case`, `cast`, `enum`, `impl`, `type` |
| 5 | `while`, `break`, `match`, `defer` |
| 6 | `return`, `struct`, `sizeof`, `extern`, `fn_ptr` |
| 7 | `syscall` |
| 8 | `continue` |

## Important Notes

- **`gen` is a keyword**: It cannot be used as a variable name or module alias. Use alternatives like `gn` or `generator` in user code.
- **`fn` is a keyword**: Cannot be used as a variable name. Use `fref`, `func`, or similar.
- **`fn_ptr` contains an underscore**: Despite containing `_`, it is treated as a single keyword token, not as an identifier `fn` followed by `_ptr`. The lexer specifically handles this multi-word keyword.
- **`is` and `as` in declarations**: These keywords structure the unique Caustic declaration syntax (`let is Type as name`), distinguishing Caustic from languages that use `:` for type annotations.
- **Case sensitivity**: All keywords are lowercase. `FN`, `Fn`, `IF`, etc. are valid identifiers, not keywords.
- **`true` and `false`**: These are not keywords in the lexer. They are recognized as identifiers and handled at the parser or semantic level as boolean constants.
- **`mut` and `imut`**: These are not keywords. They appear after `with` in global declarations and are recognized contextually by the parser as identifiers.
