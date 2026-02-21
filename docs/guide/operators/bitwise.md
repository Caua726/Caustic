# Bitwise Operators

## Operators

| Operator | Description       | Example     |
|----------|------------------|-------------|
| `&`      | Bitwise AND       | `a & b`     |
| `\|`     | Bitwise OR        | `a \| b`    |
| `^`      | Bitwise XOR       | `a ^ b`     |
| `~`      | Bitwise NOT       | `~a`        |
| `<<`     | Left shift        | `a << n`    |
| `>>`     | Right shift       | `a >> n`    |

All bitwise operators work on integer types (`i8`-`i64`, `u8`-`u64`).

## Bitwise AND

Masks specific bits. Common for extracting fields.

```cst
let is i32 as val = 0xABCD;
let is i32 as lo_byte = val & 0xFF;     // 0xCD
let is i32 as hi_byte = (val >> 8) & 0xFF;  // 0xAB
```

## Bitwise OR

Sets specific bits. Common for combining flags.

```cst
let is i32 as O_RDONLY = 0;
let is i32 as O_CREAT = 64;
let is i32 as O_TRUNC = 512;
let is i32 as flags = O_CREAT | O_TRUNC;
```

## Bitwise XOR

Flips specific bits. XOR with itself yields zero.

```cst
let is i32 as a = 0xFF;
let is i32 as b = 0x0F;
let is i32 as result = a ^ b;  // 0xF0
```

## Bitwise NOT

Inverts all bits.

```cst
let is i32 as mask = ~0;  // all bits set (0xFFFFFFFF)
```

## Shift Left

Shifts bits left, filling with zeros. Equivalent to multiplying by powers of 2.

```cst
let is i32 as x = 1;
let is i32 as y = x << 4;  // 16
```

## Shift Right

Shifts bits right. For signed types, the sign bit is preserved (arithmetic shift). For unsigned types, zeros are shifted in (logical shift).

```cst
let is i32 as x = 256;
let is i32 as y = x >> 4;  // 16
```

## Common Patterns

**Check if bit is set:**
```cst
if (flags & FLAG_MASK != 0) {
    // bit is set
}
```

**Set a bit:**
```cst
flags = flags | (1 << bit_pos);
```

**Clear a bit:**
```cst
flags = flags & ~(1 << bit_pos);
```

**Byte extraction (safe for negative values):**
```cst
let is i32 as byte0 = val & 0xFF;
let is i32 as byte1 = (val >> 8) & 0xFF;
```

Use bitwise shift and mask instead of division for byte extraction, since division truncates toward zero and gives wrong results for negative numbers.
