# Literal Parsing

The lexer recognizes four types of literal tokens: integers, floats, strings, and characters. Each has specific scanning rules and escape handling.

## Integer Literals

**Token kind**: `TK_INTEGER` (2)

Integer literals consist of one or more decimal digits (`0`-`9`). The lexer scans all consecutive digits and converts the value inline, storing the result in the `int_val` field of the `Token` struct.

```
42
0
1234567890
```

### Storage

- `ptr` and `len` point to the digit sequence in the source buffer.
- `int_val` holds the pre-parsed 64-bit integer value (`i64`).
- The parser and later phases read `int_val` directly, avoiding redundant string-to-integer conversion.

### Limitations

- Only decimal (base 10) literals are supported. There is no hexadecimal (`0x`), octal (`0o`), or binary (`0b`) prefix notation.
- No underscore separators (e.g., `1_000_000` is not supported).
- No suffix notation for specifying type (e.g., `42i32`). The type is determined by the declaration context.
- Negative numbers are not parsed as negative literals. A negative value like `-5` is parsed as unary minus (`TK_MINUS`) applied to the integer literal `5`.

## Float Literals

**Token kind**: `TK_FLOAT` (3)

Float literals are sequences of digits containing exactly one decimal point. The lexer detects a float when it encounters a `.` character while scanning digits.

```
3.14
0.0
100.5
```

### Storage

- `ptr` and `len` point to the full text including the decimal point.
- The `int_val` field is not used for float tokens. The textual representation is preserved for later conversion during code generation.

### Limitations

- A leading digit is required: `.5` is not a valid float literal (the `.` would be parsed as `TK_DOT`).
- A trailing digit after the dot is required: `5.` is not valid.
- No scientific notation (e.g., `1.5e10` is not supported).
- No float suffixes. The type (`f32` or `f64`) is determined by the variable's declared type.

### Type Matching

Float literals must match the declared variable type exactly. Assigning `10` (an integer literal) to an `f64` variable will cause a type error. Use `10.0` instead:

```cst
let is f64 as x = 10.0;   // correct
let is f64 as y = 10;     // type error: integer literal for float variable
```

## String Literals

**Token kind**: `TK_STRING` (4)

String literals are enclosed in double quotes (`"`). The lexer scans from the opening `"` to the closing `"`, processing escape sequences along the way.

```
"hello world"
"line one\nline two"
"tab\there"
""
```

### Escape Sequences

The following escape sequences are recognized inside string literals:

| Escape | Character | Description |
|--------|-----------|-------------|
| `\n` | 0x0A | Newline (line feed) |
| `\t` | 0x09 | Horizontal tab |
| `\\` | 0x5C | Backslash |
| `\0` | 0x00 | Null byte |
| `\"` | 0x22 | Double quote |

### Storage

- `ptr` points to the opening `"` character in the source buffer.
- `len` includes the quote characters (the full span from opening to closing `"` inclusive).
- The actual string content (after escape processing) is resolved during later compilation phases. The lexer preserves the raw source text.

### Error Conditions

- **Unterminated string**: If end-of-file is reached before a closing `"`, the lexer reports an error with the line and column of the opening quote.
- **Unknown escape**: Unrecognized escape sequences (e.g., `\x`, `\r`) produce a lexer error.

## Character Literals

**Token kind**: `TK_CHAR` (5)

Character literals are enclosed in single quotes (`'`). They represent a single character or escape sequence.

```
'A'
'\n'
'\0'
'\\'
```

### Escape Sequences

Character literals support the same escape sequences as strings:

| Escape | Character | Description |
|--------|-----------|-------------|
| `\n` | 0x0A | Newline |
| `\t` | 0x09 | Tab |
| `\\` | 0x5C | Backslash |
| `\0` | 0x00 | Null byte |
| `\'` | 0x27 | Single quote |

### Storage

- `ptr` points to the opening `'` in the source buffer.
- `len` spans the full literal including quotes.
- The character's numeric value may be stored in `int_val` for direct use.

### Type

Character literals produce a value of type `char`. In Caustic, `char` is an 8-bit type. No cast is needed when assigning a character literal to a `char` variable:

```cst
let is char as c = 'A';    // correct, no cast needed
```

### Error Conditions

- **Unterminated character**: Missing closing `'` before end-of-line or end-of-file.
- **Empty character literal**: `''` with no content between quotes.
- **Multi-character literal**: More than one character (or one escape sequence) between quotes.

## Comments

While not literals, comment handling is part of the lexer's scanning loop:

- **Line comments**: Begin with `//` and extend to end of line. The lexer skips all characters until a newline or end-of-file.
- **No block comments**: Caustic does not support `/* ... */` style block comments.

Comments are discarded entirely; no tokens are produced for them.
