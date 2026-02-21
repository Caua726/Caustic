# String Type

The `string` type in Caustic is 8 bytes -- a pointer to null-terminated character data.

## String Literals

String literals are enclosed in double quotes and stored in the `.rodata` (read-only data) section of the binary.

```cst
let is string as msg = "Hello, World!";
let is *u8 as greeting = "Hi there";
```

String literals are effectively `*u8` pointers to read-only memory.

## Escape Sequences

```cst
let is string as lines = "line one\nline two\n";
let is string as tabbed = "col1\tcol2\tcol3";
let is string as quoted = "say \"hello\"";
let is string as path = "C:\\Users\\file";
let is string as terminated = "data\0more";
```

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |
| `\0` | Null byte |

## Writing Strings

Since Caustic uses raw Linux syscalls, write strings with the `write` syscall.

```cst
let is *u8 as msg = "hello\n";
syscall(1, 1, msg, 6);    // write(stdout, msg, len)
```

Or use `std/io.cst` for formatted output:

```cst
use "std/io.cst" as io;
io.printf("value: %d\n", 42);
```

## Dynamic Strings

For strings that grow, shrink, or are modified at runtime, use the `String` struct from `std/string.cst`.

```cst
use "std/string.cst" as str;
use "std/mem.cst" as mem;

let is str.String as s = str.new("hello");
str.append(&s, " world");
// s.ptr holds "hello world", s.len is 11
str.free(&s);
```

The `String` struct has three fields:
- `ptr` -- pointer to the character data (`*u8`)
- `len` -- current length in bytes (`i64`)
- `cap` -- allocated capacity in bytes (`i64`)

## String Literals Are Read-Only

Do not attempt to modify string literal data. It lives in `.rodata` and writing to it will cause a segmentation fault. Copy the data first if you need a mutable version.

```cst
use "std/mem.cst" as mem;

let is *u8 as src = "hello";
let is *u8 as buf = mem.galloc(6);
// copy bytes from src to buf, then modify buf
```
