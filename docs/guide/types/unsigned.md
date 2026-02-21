# Unsigned Integers

Caustic provides four unsigned integer types for non-negative values.

| Type | Size | Range |
|------|------|-------|
| `u8` | 1 byte | 0 to 255 |
| `u16` | 2 bytes | 0 to 65,535 |
| `u32` | 4 bytes | 0 to 4,294,967,295 |
| `u64` | 8 bytes | 0 to 18,446,744,073,709,551,615 |

## Declaration

```cst
let is u8 as byte = 255;
let is u16 as port = 8080;
let is u32 as count = 1000000;
let is u64 as addr = 0xDEADBEEF;
```

## Common Uses

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

## Casting Between Signed and Unsigned

Use `cast()` to convert between signed and unsigned types. The bit pattern is preserved.

```cst
let is i32 as signed_val = 0 - 1;
let is u32 as unsigned_val = cast(u32, signed_val);    // 4294967295

let is u8 as byte = 200;
let is i8 as signed_byte = cast(i8, byte);             // -56
```

## Pointers as u64

Pointer values can be cast to `u64` for arithmetic and back.

```cst
let is *u8 as ptr = mem.galloc(64);
let is u64 as addr = cast(u64, ptr);
let is u64 as next = addr + 8;
let is *u8 as ptr2 = cast(*u8, next);
```
