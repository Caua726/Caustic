# Integers

Caustic provides four signed integer types with explicit sizes.

| Type | Size | Range |
|------|------|-------|
| `i8` | 1 byte | -128 to 127 |
| `i16` | 2 bytes | -32,768 to 32,767 |
| `i32` | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `i64` | 8 bytes | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |

## Declaration

```cst
let is i8 as small = 42;
let is i16 as medium = 1000;
let is i32 as normal = 100000;
let is i64 as large = 9999999999;
```

## Literal Narrowing

Integer literals are `i64` by default, but the compiler automatically narrows them when assigned to a smaller type. No cast is needed as long as the value fits.

```cst
let is i32 as x = 42;    // 42 is i64, narrows to i32
let is i8 as y = 100;    // 100 is i64, narrows to i8
```

## Arithmetic

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

## Bitwise Operations

```cst
let is i32 as x = 0xFF;
let is i32 as a = x & 0x0F;     // AND: 0x0F
let is i32 as b = x | 0x100;    // OR: 0x1FF
let is i32 as c = x ^ 0xFF;     // XOR: 0x00
let is i32 as d = x << 4;       // shift left: 0xFF0
let is i32 as e = x >> 4;       // shift right: 0x0F
```

## Overflow Behavior

Integer overflow wraps around silently. There are no runtime overflow checks.

```cst
let is i8 as x = 127;
let is i8 as y = x + 1;    // wraps to -128
```
