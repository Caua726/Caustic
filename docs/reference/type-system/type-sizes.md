# Type Sizes

This document provides a complete reference for the size of every type in Caustic. All sizes are in bytes. The `sizeof()` expression returns these values as `i64` at compile time.

## Primitive Type Sizes

| Type | Size (bytes) |
|------|-------------|
| `void` | 0 |
| `i8` | 1 |
| `i16` | 2 |
| `i32` | 4 |
| `i64` | 8 |
| `u8` | 1 |
| `u16` | 2 |
| `u32` | 4 |
| `u64` | 8 |
| `f32` | 4 |
| `f64` | 8 |
| `bool` | 1 |
| `char` | 1 |
| `string` | 8 |

## Pointer Sizes

All pointer types are 8 bytes, regardless of the pointed-to type:

| Type | Size (bytes) |
|------|-------------|
| `*i8` | 8 |
| `*i32` | 8 |
| `*i64` | 8 |
| `*u8` | 8 |
| `*Point` | 8 |
| `**i32` | 8 |

## Array Sizes

Array size = `N * sizeof(element_type)`:

| Type | Size (bytes) | Calculation |
|------|-------------|-------------|
| `[10]i32` | 40 | 10 * 4 |
| `[256]u8` | 256 | 256 * 1 |
| `[4]i64` | 32 | 4 * 8 |
| `[100]bool` | 100 | 100 * 1 |
| `[3][4]i32` | 48 | 3 * (4 * 4) |

## Struct Sizes

Struct size = sum of all field sizes (packed, no alignment padding):

```cst
struct A { x as i32; y as i32; }
// sizeof(A) = 4 + 4 = 8

struct B { a as i8; b as i64; c as i32; }
// sizeof(B) = 1 + 8 + 4 = 13

struct C { p as *u8; val as i32; }
// sizeof(C) = 8 + 4 = 12

struct D { inner as A; z as i32; }
// sizeof(D) = sizeof(A) + 4 = 8 + 4 = 12
```

### Packed Layout Examples

Caustic's packed layout differs from C's padded layout:

| Struct Fields | Caustic Size | Typical C Size |
|--------------|-------------|----------------|
| `{i8, i64}` | 9 | 16 |
| `{i64, i32, i64}` | 20 | 24 |
| `{i8, i32, i8}` | 6 | 12 |
| `{i32, f64}` | 12 | 16 |
| `{i64, i64}` | 16 | 16 |

When all fields are the same size, packed and aligned layouts produce the same result.

## Enum Sizes

Enum size = tag size (4 bytes) + max payload:

```cst
enum Simple { A; B; C; }
// sizeof(Simple) = 4 + 0 = 4 (tag only)

enum WithData {
    Small(x as i32);           // payload: 4
    Large(a as i64, b as i64); // payload: 16
    None;                       // payload: 0
}
// sizeof(WithData) = 4 + 16 = 20 (tag + largest payload)
```

| Enum Variants | Tag | Max Payload | Total Size |
|--------------|-----|-------------|------------|
| No payload variants | 4 | 0 | 4 |
| Largest variant has one `i32` | 4 | 4 | 8 |
| Largest variant has one `i64` | 4 | 8 | 12 |
| Largest variant has two `i64` | 4 | 16 | 20 |
| Largest variant has a struct (size 24) | 4 | 24 | 28 |

## sizeof() Expression

The `sizeof()` expression returns the size of a type as `i64`:

```cst
let is i64 as s1 = sizeof(i32);       // 4
let is i64 as s2 = sizeof(*u8);       // 8
let is i64 as s3 = sizeof([10]i32);   // 40
let is i64 as s4 = sizeof(Point);     // depends on Point definition
```

`sizeof()` is evaluated at compile time. The argument is a type name, not an expression.

## Size Rules Summary

| Type Category | Size Formula |
|--------------|-------------|
| Void | 0 |
| Integer (signed/unsigned) | 1, 2, 4, or 8 depending on width |
| Float | 4 (f32) or 8 (f64) |
| Bool | 1 |
| Char | 1 |
| String | 8 (pointer) |
| Pointer (*T) | 8 (always) |
| Array ([N]T) | N * sizeof(T) |
| Struct | sum of field sizes (packed) |
| Enum | 4 + max(variant payload sizes) |
