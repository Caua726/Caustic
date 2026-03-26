# Operators

## Arithmetic

### Operators

| Operator | Description    | Example      |
|----------|---------------|--------------|
| `+`      | Addition       | `a + b`      |
| `-`      | Subtraction    | `a - b`      |
| `*`      | Multiplication | `a * b`      |
| `/`      | Division       | `a / b`      |
| `%`      | Modulo         | `a % b`      |

### Integer Arithmetic

Works on all integer types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`.

```cst
fn main() as i32 {
    let is i32 as a = 10;
    let is i32 as b = 3;

    let is i32 as sum = a + b;    // 13
    let is i32 as diff = a - b;   // 7
    let is i32 as prod = a * b;   // 30
    let is i32 as quot = a / b;   // 3 (truncated)
    let is i32 as rem = a % b;    // 1

    return 0;
}
```

Integer division truncates toward zero. For negative numbers, `-7 / 2` gives `-3`, not `-4`.

### Float Arithmetic

Works on `f32` and `f64`. Uses SSE instructions (addsd, subsd, mulsd, divsd).

```cst
fn main() as i32 {
    let is f64 as x = 10.0;
    let is f64 as y = 3.0;

    let is f64 as sum = x + y;    // 13.0
    let is f64 as div = x / y;    // 3.333...

    return 0;
}
```

Float literals must include a decimal point. Use `10.0` not `10` when assigning to float variables.

### Unary Minus

Negate a value with the `-` prefix operator.

```cst
let is i32 as x = 5;
let is i32 as neg = 0 - x;  // -5
```

### Pointer Arithmetic

Adding an integer to a pointer advances it by that many elements (scaled by the pointed-to type size).

```cst
let is *i32 as p = &arr[0];
let is *i32 as next = p + 1;  // points to arr[1]
```

## Comparison

### Operators

| Operator | Description      | Example     |
|----------|-----------------|-------------|
| `==`     | Equal            | `a == b`    |
| `!=`     | Not equal        | `a != b`    |
| `<`      | Less than        | `a < b`     |
| `<=`     | Less or equal    | `a <= b`    |
| `>`      | Greater than     | `a > b`     |
| `>=`     | Greater or equal | `a >= b`    |

### Usage in Conditions

```cst
fn clamp(x as i32, lo as i32, hi as i32) as i32 {
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}
```

### Integer Comparison

Works on all integer types. Signed types use signed comparison, unsigned types use unsigned comparison.

```cst
let is i32 as a = 10;
let is i32 as b = 20;

if (a < b) {
    // true
}

if (a == b) {
    // false
}
```

### Float Comparison

Comparisons work on `f32` and `f64` using SSE compare instructions.

```cst
let is f64 as x = 3.14;
let is f64 as y = 2.71;

if (x > y) {
    // true
}
```

### Pointer Comparison

Pointers can be compared for equality or ordering (address comparison).

```cst
let is *u8 as p = cast(*u8, 0);

if (p == cast(*u8, 0)) {
    // null check
}
```

### Result Type

Comparison operators produce an integer result: `1` for true, `0` for false. They can be used in expressions:

```cst
let is i32 as is_positive = x > 0;  // 1 or 0
```

## Logical

### Operators

| Operator | Description | Example        |
|----------|------------|----------------|
| `&&`     | Logical AND | `a && b`       |
| `\|\|`   | Logical OR  | `a \|\| b`     |
| `!`      | Logical NOT | `!a`           |

### Logical AND

Returns true (non-zero) only if both operands are true. Uses short-circuit evaluation: if the left operand is false, the right operand is not evaluated.

```cst
fn in_range(x as i32, lo as i32, hi as i32) as i32 {
    if (x >= lo && x <= hi) {
        return 1;
    }
    return 0;
}
```

### Logical OR

Returns true if either operand is true. Uses short-circuit evaluation: if the left operand is true, the right operand is not evaluated.

```cst
fn is_whitespace(c as char) as i32 {
    if (c == ' ' || c == '\n' || c == '\t') {
        return 1;
    }
    return 0;
}
```

### Logical NOT

Inverts a boolean value. Non-zero becomes 0, zero becomes 1.

```cst
let is i32 as found = 0;
if (!found) {
    // executes when found is 0
}
```

### Combining Conditions

Parentheses can clarify complex conditions.

```cst
if ((a > 0 && b > 0) || force) {
    // runs if both a,b positive OR force is set
}
```

### Short-Circuit Safety

Short-circuit evaluation makes it safe to guard dereferences:

```cst
if (ptr != cast(*u8, 0) && *ptr == 10) {
    // *ptr is only evaluated if ptr is non-null
}
```

## Bitwise

### Operators

| Operator | Description       | Example     |
|----------|------------------|-------------|
| `&`      | Bitwise AND       | `a & b`     |
| `\|`     | Bitwise OR        | `a \| b`    |
| `^`      | Bitwise XOR       | `a ^ b`     |
| `~`      | Bitwise NOT       | `~a`        |
| `<<`     | Left shift        | `a << n`    |
| `>>`     | Right shift       | `a >> n`    |

All bitwise operators work on integer types (`i8`-`i64`, `u8`-`u64`).

### Bitwise AND

Masks specific bits. Common for extracting fields.

```cst
let is i32 as val = 0xABCD;
let is i32 as lo_byte = val & 0xFF;     // 0xCD
let is i32 as hi_byte = (val >> 8) & 0xFF;  // 0xAB
```

### Bitwise OR

Sets specific bits. Common for combining flags.

```cst
let is i32 as O_RDONLY = 0;
let is i32 as O_CREAT = 64;
let is i32 as O_TRUNC = 512;
let is i32 as flags = O_CREAT | O_TRUNC;
```

### Bitwise XOR

Flips specific bits. XOR with itself yields zero.

```cst
let is i32 as a = 0xFF;
let is i32 as b = 0x0F;
let is i32 as result = a ^ b;  // 0xF0
```

### Bitwise NOT

Inverts all bits.

```cst
let is i32 as mask = ~0;  // all bits set (0xFFFFFFFF)
```

### Shift Left

Shifts bits left, filling with zeros. Equivalent to multiplying by powers of 2.

```cst
let is i32 as x = 1;
let is i32 as y = x << 4;  // 16
```

### Shift Right

Shifts bits right. For signed types, the sign bit is preserved (arithmetic shift). For unsigned types, zeros are shifted in (logical shift).

```cst
let is i32 as x = 256;
let is i32 as y = x >> 4;  // 16
```

### Common Patterns

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

## Compound Assignment

Compound assignment operators modify a variable in place, combining an operation with assignment.

### Arithmetic Compound

| Operator | Equivalent       | Example     |
|----------|-----------------|-------------|
| `+=`     | `x = x + y`     | `x += 1;`  |
| `-=`     | `x = x - y`     | `x -= 5;`  |
| `*=`     | `x = x * y`     | `x *= 2;`  |
| `/=`     | `x = x / y`     | `x /= 3;`  |
| `%=`     | `x = x % y`     | `x %= 10;` |

```cst
fn main() as i32 {
    let is i32 as count = 0;
    count += 1;   // count is now 1
    count += 10;  // count is now 11
    count *= 2;   // count is now 22
    count -= 2;   // count is now 20
    return count;
}
```

### Shift Compound

| Operator | Equivalent       | Example      |
|----------|-----------------|--------------|
| `<<=`    | `x = x << y`    | `x <<= 4;`  |
| `>>=`    | `x = x >> y`    | `x >>= 8;`  |

```cst
let is i32 as flags = 1;
flags <<= 3;  // flags is now 8
```

### Bitwise Compound

| Operator | Equivalent       | Example         |
|----------|-----------------|-----------------|
| `&=`     | `x = x & y`     | `x &= 0xFF;`   |
| `\|=`    | `x = x \| y`    | `x \|= mask;`  |
| `^=`     | `x = x ^ y`     | `x ^= bits;`   |

```cst
let is i32 as val = 0xFF00;
val &= 0x00FF;  // val is now 0
val |= 0x0042;  // val is now 0x42
```

### Loop Counter Pattern

Compound assignment is commonly used for loop counters:

```cst
let is i32 as i = 0;
while (i < 100) {
    // work
    i += 1;
}
```

## Short-Circuit Evaluation

Logical operators `&&` and `||` use short-circuit evaluation, meaning the right operand is only evaluated if necessary.

### Logical AND (`&&`)

If the left operand is false (0), the result is false regardless of the right operand. The right operand is **not evaluated**.

```cst
// safe_divide only divides if b is non-zero
fn safe_divide(a as i32, b as i32) as i32 {
    if (b != 0 && a / b > 10) {
        return 1;
    }
    return 0;
}
```

If `b` is 0, the expression `a / b` is never executed, avoiding a division by zero.

### Logical OR (`||`)

If the left operand is true (non-zero), the result is true regardless of the right operand. The right operand is **not evaluated**.

```cst
fn has_access(is_admin as i32, has_token as i32) as i32 {
    if (is_admin || has_token) {
        return 1;
    }
    return 0;
}
```

If `is_admin` is true, `has_token` is never checked.

### Null Pointer Guard

The most important use case is guarding pointer dereferences:

```cst
fn check(ptr as *i32) as i32 {
    if (ptr != cast(*i32, 0) && *ptr == 42) {
        return 1;
    }
    return 0;
}
```

If `ptr` is null, `*ptr` is never evaluated, preventing a segfault.

### Implementation

The compiler implements short-circuit evaluation using conditional jumps in the IR. For `&&`, if the left side is false, it jumps past the right side. For `||`, if the left side is true, it jumps past the right side. No temporary boolean values are created.

### Chaining

Short-circuit operators can be chained. Evaluation stops as soon as the result is determined:

```cst
if (a > 0 && b > 0 && c > 0) {
    // only reached if all three are positive
    // if a <= 0, neither b > 0 nor c > 0 is evaluated
}
```

## Operator Precedence

Operators are listed from highest precedence (evaluated first) to lowest. Operators on the same row have equal precedence and are evaluated left to right.

| Precedence | Operators                        | Description              |
|------------|----------------------------------|--------------------------|
| 1 (highest)| `()`                            | Grouping                 |
| 2          | `[]` `.`                         | Indexing, member access   |
| 3          | `!` `~` `-` (unary) `*` `&`     | Unary NOT, complement, negate, deref, address-of |
| 4          | `*` `/` `%`                      | Multiplication, division, modulo |
| 5          | `+` `-`                          | Addition, subtraction    |
| 6          | `<<` `>>`                        | Bit shifts               |
| 7          | `<` `<=` `>` `>=`               | Relational comparison    |
| 8          | `==` `!=`                        | Equality comparison      |
| 9          | `&`                              | Bitwise AND              |
| 10         | `^`                              | Bitwise XOR              |
| 11         | `\|`                             | Bitwise OR               |
| 12         | `&&`                             | Logical AND              |
| 13         | `\|\|`                           | Logical OR               |
| 14 (lowest)| `=` `+=` `-=` `*=` `/=` `%=` `<<=` `>>=` `&=` `\|=` `^=` | Assignment |

### Examples

```cst
// Multiplication before addition
let is i32 as x = 2 + 3 * 4;     // 14, not 20

// Comparison before logical AND
if (a > 0 && b < 10) { }          // parsed as (a > 0) && (b < 10)

// Bitwise AND before bitwise OR
let is i32 as r = a & 0xF | b;    // parsed as (a & 0xF) | b

// Shift before comparison
if (1 << n > 100) { }             // parsed as (1 << n) > 100
```

### Using Parentheses

When in doubt, use parentheses to make intent explicit. This is especially important with bitwise operators, where precedence is often surprising:

```cst
// Without parens: & binds tighter than ==
if (flags & MASK == 0) { }        // parsed as flags & (MASK == 0) -- probably wrong!

// With parens: correct intent
if ((flags & MASK) == 0) { }      // check if masked bits are zero
```

### Associativity

All binary operators are left-associative:

```cst
let is i32 as x = a - b - c;     // parsed as (a - b) - c
```

Assignment operators are right-associative:

```cst
x = y = 0;                        // parsed as x = (y = 0)
```
