# Packed Layout

Caustic structs are **packed** -- there is no alignment padding between fields. Fields are laid out sequentially in memory, and `sizeof(struct)` equals the sum of all field sizes.

## Layout Rules

- Fields are stored in declaration order
- No padding between fields
- No trailing padding
- `sizeof(struct)` = sum of all field sizes

## Example

```cst
struct Mixed {
    a as i64;   // offset 0, size 8
    b as i32;   // offset 8, size 4
    c as i64;   // offset 12, size 8
}
// sizeof(Mixed) = 20
```

In C with standard alignment, the equivalent struct would be 24 bytes (4 bytes padding after `b`). In Caustic, it is 20 bytes.

## Field Offsets

Offsets are computed by summing preceding field sizes:

```cst
struct Example {
    a as u8;    // offset 0, size 1
    b as i32;   // offset 1, size 4
    c as i64;   // offset 5, size 8
    d as u8;    // offset 13, size 1
}
// sizeof(Example) = 14
```

## C Interop Warning

Because C structs have alignment padding and Caustic structs do not, their layouts will differ for mixed-size fields. If you need to match a C struct layout:

- Use all-i64 fields (naturally aligned at any offset multiple of 8)
- Or manually insert padding fields:

```cst
struct CCompatible {
    a as i64;       // offset 0
    b as i32;       // offset 8
    _pad as i32;    // offset 12 (manual padding)
    c as i64;       // offset 16
}
// sizeof = 24, matches C layout
```

## Sizeof

Use `sizeof(StructName)` to get the total size at compile time:

```cst
struct Point { x as i32; y as i32; }

let is *Point as p = cast(*Point, mem.galloc(sizeof(Point)));
// allocates 8 bytes
```
