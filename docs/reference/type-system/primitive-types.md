# Primitive Types

Caustic provides 14 primitive types covering signed and unsigned integers, floating-point numbers, booleans, characters, strings, and void. Each primitive type has a global singleton `Type` object, and type identity is determined by pointer equality.

## Type Table

| Kind Constant | Name | Size (bytes) | Value Range |
|---------------|------|-------------|-------------|
| `TY_VOID` (0) | `void` | 0 | No value |
| `TY_I8` (1) | `i8` | 1 | -128 to 127 |
| `TY_I16` (2) | `i16` | 2 | -32768 to 32767 |
| `TY_I32` (3) | `i32` | 4 | -2147483648 to 2147483647 |
| `TY_I64` (4) | `i64` | 8 | -9223372036854775808 to 9223372036854775807 |
| `TY_U8` (5) | `u8` | 1 | 0 to 255 |
| `TY_U16` (6) | `u16` | 2 | 0 to 65535 |
| `TY_U32` (7) | `u32` | 4 | 0 to 4294967295 |
| `TY_U64` (8) | `u64` | 8 | 0 to 18446744073709551615 |
| `TY_F32` (9) | `f32` | 4 | IEEE 754 single-precision |
| `TY_F64` (10) | `f64` | 8 | IEEE 754 double-precision |
| `TY_BOOL` (11) | `bool` | 1 | `true` or `false` |
| `TY_CHAR` (12) | `char` | 1 | ASCII character (0-127) |
| `TY_STRING` (13) | `string` | 8 | Pointer to null-terminated byte sequence |

## Global Singletons

Each primitive type has exactly one `Type` object allocated globally:

- `type_void`
- `type_i8`, `type_i16`, `type_i32`, `type_i64`
- `type_u8`, `type_u16`, `type_u32`, `type_u64`
- `type_f32`, `type_f64`
- `type_bool`
- `type_char`
- `type_string`

All nodes with a given primitive type point to the same singleton. Type comparison uses pointer equality:

```
// Checking if a node is i32:
if node.ty == type_i32 {
    // yes, it is i32
}
```

Creating a new `Type` struct with `kind = TY_I32` instead of using `type_i32` will break pointer equality checks throughout the compiler. This is a critical invariant.

## Integer Types

### Signed Integers (i8, i16, i32, i64)

Signed integers use two's complement representation. Arithmetic follows standard two's complement rules.

```cst
let is i8 as small = 127;
let is i16 as medium = 30000;
let is i32 as normal = 1000000;
let is i64 as large = 9000000000;
```

### Unsigned Integers (u8, u16, u32, u64)

Unsigned integers represent non-negative values only. They are commonly used for byte manipulation, flags, and addresses.

```cst
let is u8 as byte = 255;
let is u32 as flags = 0xFF00FF00;
let is u64 as addr = cast(u64, ptr);
```

### Integer Literal Typing

All integer literals are initially typed as `i64`. When used in a context expecting a different integer type, [literal narrowing](../semantic/type-inference.md) automatically adjusts the type.

## Floating-Point Types

### f32 (single-precision)

4 bytes, IEEE 754 single-precision. Approximately 7 decimal digits of precision. Uses SSE instructions for arithmetic.

### f64 (double-precision)

8 bytes, IEEE 754 double-precision. Approximately 15 decimal digits of precision. Uses SSE instructions (`addsd`, `subsd`, `mulsd`, `divsd`).

```cst
let is f64 as pi = 3.14159265358979;
let is f32 as approx = 3.14;
```

Float literals are initially typed as `f64`. When assigned to an `f32` variable, the literal narrows automatically.

**Important**: Float literals must include a decimal point. Using `10` instead of `10.0` for a float variable will cause a type mismatch.

## Boolean Type

The `bool` type is 1 byte and holds either `true` or `false`. Comparison operators produce `bool` results, and logical operators (`&&`, `||`) require `bool` operands.

```cst
let is bool as flag = true;
let is bool as check = 10 > 5;   // true
```

## Character Type

The `char` type is 1 byte and represents a single ASCII character. Character literals use single quotes:

```cst
let is char as c = 'A';
let is char as newline = '\n';
```

Characters are properly typed -- no cast is needed when assigning a character literal to a `char` variable.

## String Type

The `string` type is an 8-byte pointer to a null-terminated byte sequence (string literal in the data section). It is equivalent to `*u8` in memory representation:

```cst
let is string as msg = "hello world";
```

String literals are stored in the `.rodata` section of the output assembly. For dynamic strings with length tracking, use the `std/string.cst` library's `String` struct.

## Void Type

`void` represents the absence of a value. It is used as the return type for functions that return nothing:

```cst
fn do_something() as void {
    // no return value
}
```

`void` has zero size. Variables cannot be declared with type `void`. There is no `*void` pointer type; use `*u8` as a generic pointer instead.
