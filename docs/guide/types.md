# Types

## Integers

Caustic provides four signed integer types with explicit sizes.

| Type | Size | Range |
|------|------|-------|
| `i8` | 1 byte | -128 to 127 |
| `i16` | 2 bytes | -32,768 to 32,767 |
| `i32` | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `i64` | 8 bytes | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |

### Declaration

```cst
let is i8 as small = 42;
let is i16 as medium = 1000;
let is i32 as normal = 100000;
let is i64 as large = 9999999999;
```

### Literal Narrowing

Integer literals are `i64` by default, but the compiler automatically narrows them when assigned to a smaller type. No cast is needed as long as the value fits.

```cst
let is i32 as x = 42;    // 42 is i64, narrows to i32
let is i8 as y = 100;    // 100 is i64, narrows to i8
```

### Arithmetic

All standard arithmetic operators work on integers.

```cst
let is i32 as a = 10;
let is i32 as b = 3;

let is i32 as sum = a + b;       // 13
let is i32 as diff = a - b;      // 7
let is i32 as prod = a * b;      // 30
let is i32 as quot = a / b;      // 3 (integer division, truncates)
let is i32 as rem = a % b;       // 1
```

### Bitwise Operations

```cst
let is i32 as x = 0xFF;
let is i32 as a = x & 0x0F;     // AND: 0x0F
let is i32 as b = x | 0x100;    // OR: 0x1FF
let is i32 as c = x ^ 0xFF;     // XOR: 0x00
let is i32 as d = x << 4;       // shift left: 0xFF0
let is i32 as e = x >> 4;       // shift right: 0x0F
```

### Overflow Behavior

Integer overflow wraps around silently. There are no runtime overflow checks.

```cst
let is i8 as x = 127;
let is i8 as y = x + 1;    // wraps to -128
```

## Unsigned Integers

Caustic provides four unsigned integer types for non-negative values.

| Type | Size | Range |
|------|------|-------|
| `u8` | 1 byte | 0 to 255 |
| `u16` | 2 bytes | 0 to 65,535 |
| `u32` | 4 bytes | 0 to 4,294,967,295 |
| `u64` | 8 bytes | 0 to 18,446,744,073,709,551,615 |

### Declaration

```cst
let is u8 as byte = 255;
let is u16 as port = 8080;
let is u32 as count = 1000000;
let is u64 as addr = 0xDEADBEEF;
```

### Common Uses

`u8` is the standard byte type, used for raw memory and character data.

```cst
let is *u8 as buffer = mem.galloc(1024);
let is u8 as ch = 65;    // ASCII 'A'
```

`u64` is used for sizes, addresses, and syscall arguments.

```cst
let is u64 as size = 4096;
let is u64 as page = cast(u64, ptr);
```

### Casting Between Signed and Unsigned

Use `cast()` to convert between signed and unsigned types. The bit pattern is preserved.

```cst
let is i32 as signed_val = 0 - 1;
let is u32 as unsigned_val = cast(u32, signed_val);    // 4294967295

let is u8 as byte = 200;
let is i8 as signed_byte = cast(i8, byte);             // -56
```

### Pointers as u64

Pointer values can be cast to `u64` for arithmetic and back.

```cst
let is *u8 as ptr = mem.galloc(64);
let is u64 as addr = cast(u64, ptr);
let is u64 as next = addr + 8;
let is *u8 as ptr2 = cast(*u8, next);
```

## Floats

Caustic supports IEEE 754 floating-point types using SSE registers.

| Type | Size | Precision |
|------|------|-----------|
| `f32` | 4 bytes | Single precision (~7 decimal digits) |
| `f64` | 8 bytes | Double precision (~15 decimal digits) |

### Declaration

Float literals **must** include a decimal point. Using `10` instead of `10.0` will be treated as an integer and cause a type error.

```cst
let is f64 as pi = 3.14159265358979;
let is f32 as ratio = 0.5;
let is f64 as zero = 0.0;
```

### Arithmetic

```cst
let is f64 as a = 10.0;
let is f64 as b = 3.0;

let is f64 as sum = a + b;     // 13.0
let is f64 as diff = a - b;    // 7.0
let is f64 as prod = a * b;    // 30.0
let is f64 as quot = a / b;    // 3.333...
```

### SSE Instructions

The compiler generates SSE instructions for float operations:
- `f64`: `addsd`, `subsd`, `mulsd`, `divsd` (scalar double)
- `f32`: `addss`, `subss`, `mulss`, `divss` (scalar single)

Float values are stored in XMM registers (xmm0-xmm15), separate from general-purpose integer registers.

### No Implicit Conversion

Integers and floats cannot be mixed without an explicit cast.

```cst
let is f64 as x = 10.0;
let is i64 as n = 5;

// ERROR: cannot add f64 and i64 directly
// let is f64 as result = x + n;

// Correct: cast the integer to f64
let is f64 as result = x + cast(f64, n);
```

### Float to Integer

```cst
let is f64 as val = 3.7;
let is i64 as truncated = cast(i64, val);    // 3 (truncates toward zero)
```

## Bool

The `bool` type represents a boolean value, stored as 1 byte.

### Declaration

```cst
let is bool as flag = true;
let is bool as done = false;
```

The values `true` and `false` are stored internally as `1` and `0`.

### Comparison Results

Comparison operators produce `bool` values.

```cst
let is i32 as x = 10;
let is i32 as y = 20;

let is bool as eq = x == y;     // false
let is bool as ne = x != y;     // true
let is bool as lt = x < y;      // true
let is bool as gt = x > y;      // false
let is bool as le = x <= y;     // true
let is bool as ge = x >= y;     // false
```

### Conditions

Booleans are used directly in `if` and `while` conditions.

```cst
let is bool as running = true;

if running {
    // ...
}

while running {
    // ...
}
```

### Logical Operators

```cst
let is bool as a = true;
let is bool as b = false;

if a && b {
    // both true
}

if a || b {
    // at least one true
}
```

## Char

The `char` type represents a single ASCII character, stored as 1 byte.

### Declaration

Character literals use single quotes.

```cst
let is char as letter = 'A';
let is char as digit = '0';
let is char as space = ' ';
```

`char` is properly typed in the compiler. No cast is needed when assigning character literals.

### Escape Sequences

```cst
let is char as newline = '\n';
let is char as tab = '\t';
let is char as backslash = '\\';
let is char as null = '\0';
```

### ASCII Value

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

### Char and u8

`char` and `u8` are both 1-byte types. Use `cast()` to convert between them.

```cst
let is char as c = 'A';
let is u8 as byte = cast(u8, c);     // 65

let is u8 as val = 48;
let is char as ch = cast(char, val);  // '0'
```

## Void

The `void` type represents the absence of a value. It has a size of 0 bytes.

### Function Return Type

`void` is used when a function does not return a value.

```cst
fn print_number(n as i64) as void {
    // write n to stdout
    // no return value
}
```

A `void` function can use `return;` with no expression, or simply fall through to the end of the body.

```cst
fn greet() as void {
    syscall(1, 1, "hello\n", 6);
    return;    // optional, implicit at end of function
}
```

### Restrictions

You cannot declare a variable of type `void`.

```cst
// ERROR: cannot have a void variable
// let is void as nothing;
```

You cannot use the result of a void function in an expression.

```cst
fn do_work() as void { }

// ERROR: void has no value
// let is i64 as x = do_work();
```

### Void Pointers

While `void` itself has no size, `*u8` is used as the generic pointer type in Caustic (similar to `void*` in C).

```cst
let is *u8 as generic_ptr = mem.galloc(64);
let is *i32 as typed_ptr = cast(*i32, generic_ptr);
```

## String

The `string` type in Caustic is 8 bytes -- a pointer to null-terminated character data.

### String Literals

String literals are enclosed in double quotes and stored in the `.rodata` (read-only data) section of the binary.

```cst
let is string as msg = "Hello, World!";
let is *u8 as greeting = "Hi there";
```

String literals are effectively `*u8` pointers to read-only memory.

### Escape Sequences

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

### Writing Strings

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

### Dynamic Strings

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

### String Literals Are Read-Only

Do not attempt to modify string literal data. It lives in `.rodata` and writing to it will cause a segmentation fault. Copy the data first if you need a mutable version.

```cst
use "std/mem.cst" as mem;

let is *u8 as src = "hello";
let is *u8 as buf = mem.galloc(6);
// copy bytes from src to buf, then modify buf
```

## Type Coercion

Caustic has minimal implicit type coercion. Most conversions require an explicit `cast()`.

### Integer Literal Narrowing

The one implicit coercion: integer literals (which are `i64` by default) automatically narrow to smaller integer types when assigned.

```cst
let is i32 as a = 42;      // i64 literal narrows to i32
let is i16 as b = 1000;    // i64 literal narrows to i16
let is i8 as c = 100;      // i64 literal narrows to i8
let is u8 as d = 255;      // i64 literal narrows to u8
```

This only works for literals, not for variables or expressions.

```cst
let is i64 as big = 42;
// ERROR: cannot implicitly narrow a variable
// let is i32 as x = big;

// Correct: use cast
let is i32 as x = cast(i32, big);
```

### Explicit Cast

The `cast(TargetType, expr)` builtin converts between types.

#### Integer widening and narrowing

```cst
let is i32 as small = 100;
let is i64 as wide = cast(i64, small);     // widen: 100
let is i8 as narrow = cast(i8, wide);      // narrow: 100 (truncates if too large)
```

#### Signed/unsigned conversion

```cst
let is i32 as signed = 0 - 1;
let is u32 as unsigned = cast(u32, signed);    // bit pattern preserved
```

#### Integer/float conversion

```cst
let is i64 as n = 42;
let is f64 as f = cast(f64, n);     // 42.0

let is f64 as pi = 3.14;
let is i64 as truncated = cast(i64, pi);    // 3
```

#### Pointer casts

```cst
let is *u8 as raw = mem.galloc(32);
let is *i32 as typed = cast(*i32, raw);

let is u64 as addr = cast(u64, raw);       // pointer to integer
let is *u8 as ptr = cast(*u8, addr);       // integer to pointer
```

### No Implicit Widening

Unlike some languages, Caustic does not implicitly widen integers in expressions.

```cst
let is i32 as a = 10;
let is i64 as b = 20;

// ERROR: type mismatch, i32 vs i64
// let is i64 as c = a + b;

// Correct: cast to matching type
let is i64 as c = cast(i64, a) + b;
```

### No Implicit Int/Float Mixing

```cst
let is f64 as x = 10.0;
let is i64 as y = 5;

// ERROR: cannot add f64 and i64
// let is f64 as z = x + y;

// Correct
let is f64 as z = x + cast(f64, y);
```

## Negative Literals

Caustic does not have negative literal syntax (no unary minus on numbers). Negative values are expressed using subtraction.

### Local Variables

Negative values work fine in local variable declarations.

```cst
let is i32 as x = 0 - 1;        // -1
let is i64 as y = 0 - 100;      // -100
let is i32 as min = 0 - 128;    // -128
```

### Immutable Globals: The Gotcha

Immutable globals (`with imut`) require a compile-time constant expression. Subtraction like `0 - 1` is not considered constant, so this fails:

```cst
// ERROR: not a constant expression
// let is i32 as NEGATIVE with imut = 0 - 1;
```

#### Workaround 1: Use positive sentinel values

```cst
let is i32 as NOT_FOUND with imut = 99;
let is i32 as UNSET with imut = 255;
let is i32 as INVALID with imut = 99999;
```

#### Workaround 2: Use mutable globals

```cst
let is i32 as NEGATIVE with mut = 0 - 1;    // works, but mutable
```

### Function Arguments and Returns

Negative values work normally in expressions, function calls, and return statements.

```cst
fn negate(x as i32) as i32 {
    return 0 - x;
}

fn main() as i32 {
    let is i32 as result = negate(42);    // -42
    return 0;
}
```

### Byte Extraction from Negative Numbers

When extracting bytes from negative numbers, avoid division (it truncates toward zero). Use bitwise operations instead.

```cst
let is i64 as val = 0 - 1;

// WRONG: division truncates toward zero for negatives
// let is i64 as high_byte = val / 256;

// CORRECT: bitwise shift and mask
let is i64 as high_byte = (val >> 8) & 255;
```

## Number Literal Formats

```cst
let is i64 as dec = 1_000_000;    // underscores for readability
let is i64 as hex = 0xFF_FF;      // hexadecimal
let is i64 as bin = 0b1010_1010;  // binary
let is i64 as oct = 0o755;        // octal
let is f64 as pi = 3.14159;      // float
```
