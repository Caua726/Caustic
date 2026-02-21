# Pointers

Caustic uses typed pointers for all indirect memory access. There is no garbage collector and no runtime safety checks.

## Syntax

`*T` declares a pointer to type `T`.

```cst
let is *i32 as p;
let is *Point as sp;
let is **u8 as pp;       // pointer to pointer
```

## Generic Byte Pointer

`*u8` is the generic byte pointer, equivalent to `void*` in C. The heap allocator returns `*u8`, which you cast to the desired type.

```cst
let is *u8 as raw = mem.galloc(64);
let is *i32 as nums = cast(*i32, raw);
```

## Null Pointer

There is no `null` literal. Use `cast(*T, 0)` to create a null pointer.

```cst
let is *u8 as ptr = cast(*u8, 0);
```

Check for null by casting back to integer:

```cst
if (cast(i64, ptr) == 0) {
    // ptr is null
}
```

## Pointer Size

All pointers are 8 bytes (64-bit addresses) regardless of the pointed-to type.

## Passing Pointers to Functions

```cst
fn fill(buf as *u8, len as i64) as void {
    // write to buf[0..len]
}

let is [32]u8 as data;
fill(&data[0], 32);
```
