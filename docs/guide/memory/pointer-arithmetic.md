# Pointer Arithmetic

Caustic does not have built-in pointer arithmetic operators (`ptr + n`). Instead, cast pointers to integers, compute the offset in bytes, and cast back.

## Basic Pattern

```cst
// advance ptr by N bytes
let is *u8 as next = cast(*u8, cast(i64, ptr) + N);
```

## Typed Element Access

For arrays of a known type, multiply the index by the element size:

```cst
// access element at index i in an i32 array
let is *i32 as base = cast(*i32, mem.galloc(40));  // 10 i32s
let is i64 as i = 3;
let is *i32 as elem = cast(*i32, cast(i64, base) + i * 4);
*elem = 99;
```

## Struct Array Traversal

```cst
struct Item { id as i32; val as i64; }

let is *u8 as buf = mem.galloc(cast(i64, sizeof(Item)) * 10);
let is i64 as i = 0;
while (i < 10) {
    let is *Item as it = cast(*Item, cast(i64, buf) + i * cast(i64, sizeof(Item)));
    it.id = cast(i32, i);
    it.val = i * 100;
    i = i + 1;
}
```

## Byte-Level Access

```cst
fn read_byte(base as *u8, offset as i64) as u8 {
    let is *u8 as p = cast(*u8, cast(i64, base) + offset);
    return *p;
}
```

## Important Notes

- All offsets are in **bytes**, not elements.
- Caustic structs are **packed** (no padding). A `{i32, i64}` struct is 12 bytes, not 16.
- Use `sizeof(T)` to get the correct element size.
