# String Escape Sequences

Caustic supports standard escape sequences in both string literals and char literals.

## Supported Escapes

| Escape | Value | Description       |
|--------|-------|-------------------|
| `\n`   | 10    | Newline           |
| `\t`   | 9     | Horizontal tab    |
| `\\`   | 92    | Backslash         |
| `\0`   | 0     | Null byte         |
| `\"`   | 34    | Double quote      |

## In String Literals

```cst
// Newline
syscall(1, 1, "Hello\n", 6);

// Tab-separated columns
syscall(1, 1, "Name\tAge\tCity\n", 14);

// Embedded quotes
syscall(1, 1, "She said \"hello\"\n", 17);

// Null byte in the middle
let is [8]u8 as buf;
// String literals are already null-terminated
```

## In Char Literals

```cst
let is char as newline = '\n';    // 10
let is char as tab = '\t';        // 9
let is char as backslash = '\\';  // 92
let is char as null = '\0';       // 0
```

## Multi-line Output

Build multi-line strings with `\n`:

```cst
fn print_help() as void {
    syscall(1, 1, "Usage: program [options]\n\n", 25);
    syscall(1, 1, "Options:\n", 9);
    syscall(1, 1, "  -h\tShow help\n", 16);
    syscall(1, 1, "  -v\tShow version\n", 19);
}
```

## Null-Terminated Strings

All Caustic string literals are automatically null-terminated, which makes them compatible with C functions:

```cst
extern fn puts(s as *u8) as i32;

fn main() as i32 {
    puts("Already null-terminated");
    return 0;
}
```

## Gotcha: Counting Bytes

When passing strings to `syscall(1, ...)` (write), the length parameter counts the bytes in the string, where each escape sequence counts as **one byte**:

```cst
// "Hi\n" = 'H' + 'i' + '\n' = 3 bytes
syscall(1, 1, "Hi\n", 3);
```
