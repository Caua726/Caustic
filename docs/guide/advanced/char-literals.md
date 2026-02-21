# Char Literals

Char literals use single quotes and represent a single byte value.

## Syntax

```cst
let is char as c = 'A';
```

## Type

Char literals have type `char`, which is 1 byte. No cast is needed when assigning to a `char` variable.

## ASCII Values

```cst
let is char as letter = 'A';   // 65
let is char as digit = '0';    // 48
let is char as space = ' ';    // 32
let is char as bang = '!';     // 33
```

## Escape Sequences

Standard escape sequences work inside char literals:

```cst
let is char as newline = '\n';     // 10
let is char as tab = '\t';        // 9
let is char as backslash = '\\';  // 92
let is char as null = '\0';       // 0
```

## Comparisons

Compare characters directly:

```cst
fn is_digit(c as char) as bool {
    return c >= '0' && c <= '9';
}

fn is_alpha(c as char) as bool {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

fn is_whitespace(c as char) as bool {
    return c == ' ' || c == '\t' || c == '\n';
}
```

## Arithmetic

Character arithmetic works for tasks like case conversion or digit parsing:

```cst
fn to_lower(c as char) as char {
    if c >= 'A' && c <= 'Z' {
        return cast(char, cast(i32, c) + 32);
    }
    return c;
}

fn digit_value(c as char) as i32 {
    return cast(i32, c) - cast(i32, '0');
}
```

## In Arrays

```cst
fn main() as i32 {
    let is [5]char as word;
    word[0] = 'H';
    word[1] = 'e';
    word[2] = 'l';
    word[3] = 'l';
    word[4] = 'o';
    syscall(1, 1, &word, 5);
    return 0;
}
```

## Char vs u8

Both `char` and `u8` are 1 byte. Use `char` when working with text and `u8` when working with raw bytes. They may need a cast to convert between them depending on context.
