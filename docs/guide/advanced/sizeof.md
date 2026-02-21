# Sizeof

`sizeof(Type)` returns the size of a type in bytes as an `i64`.

## Syntax

```cst
sizeof(Type)
```

## Primitive Sizes

```cst
sizeof(i8)    // 1
sizeof(i16)   // 2
sizeof(i32)   // 4
sizeof(i64)   // 8
sizeof(u8)    // 1
sizeof(u16)   // 2
sizeof(u32)   // 4
sizeof(u64)   // 8
sizeof(f32)   // 4
sizeof(f64)   // 8
sizeof(bool)  // 1
sizeof(char)  // 1
sizeof(*u8)   // 8 (all pointers are 8 bytes on x86_64)
```

## Struct Sizes

Caustic structs are **packed** -- there is no alignment padding between fields.

```cst
struct Point {
    x as i32;
    y as i32;
}
// sizeof(Point) = 8 (4 + 4)

struct Mixed {
    a as i32;
    b as i64;
    c as i32;
}
// sizeof(Mixed) = 20 (4 + 8 + 4), NOT 24
```

This differs from C, where `Mixed` would be 24 bytes due to alignment padding.

## Common Use: Heap Allocation

```cst
use "std/mem.cst" as mem;

struct Buffer {
    data as *u8;
    len as i64;
    cap as i64;
}

fn new_buffer(cap as i64) as *Buffer {
    let is *u8 as raw = mem.galloc(sizeof(Buffer));
    let is *Buffer as buf = cast(*Buffer, raw);
    buf.data = mem.galloc(cap);
    buf.len = 0;
    buf.cap = cap;
    return buf;
}
```

## Array Sizing

Calculate total bytes for N elements:

```cst
let is i64 as count = 100;
let is *u8 as arr = mem.galloc(count * sizeof(i64));
```

## Gotcha: Packed Structs

Because structs are packed, mixing field sizes can cause issues with C FFI. If you need C-compatible layout, use all-`i64` fields or manually add padding fields:

```cst
// C-compatible layout for { int a; double b; }
struct CCompat {
    a as i32;
    _pad as i32;   // manual padding to match C alignment
    b as f64;
}
// sizeof(CCompat) = 16, matches C layout
```
