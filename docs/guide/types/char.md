# Char

The `char` type represents a single ASCII character, stored as 1 byte.

## Declaration

Character literals use single quotes.

```cst
let is char as letter = 'A';
let is char as digit = '0';
let is char as space = ' ';
```

`char` is properly typed in the compiler. No cast is needed when assigning character literals.

## Escape Sequences

```cst
let is char as newline = '\n';
let is char as tab = '\t';
let is char as backslash = '\\';
let is char as null = '\0';
```

## ASCII Value

Characters are stored as their ASCII byte value. You can compare and do arithmetic on them.

```cst
let is char as c = 'A';

// Check if uppercase letter
if c >= 'A' && c <= 'Z' {
    // ...
}

// Convert to lowercase (add 32)
let is char as lower = cast(char, cast(i32, c) + 32);
```

## Char and u8

`char` and `u8` are both 1-byte types. Use `cast()` to convert between them.

```cst
let is char as c = 'A';
let is u8 as byte = cast(u8, c);     // 65

let is u8 as val = 48;
let is char as ch = cast(char, val);  // '0'
```
