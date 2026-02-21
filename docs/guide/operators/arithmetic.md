# Arithmetic Operators

## Operators

| Operator | Description    | Example      |
|----------|---------------|--------------|
| `+`      | Addition       | `a + b`      |
| `-`      | Subtraction    | `a - b`      |
| `*`      | Multiplication | `a * b`      |
| `/`      | Division       | `a / b`      |
| `%`      | Modulo         | `a % b`      |

## Integer Arithmetic

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

## Float Arithmetic

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

## Unary Minus

Negate a value with the `-` prefix operator.

```cst
let is i32 as x = 5;
let is i32 as neg = 0 - x;  // -5
```

## Pointer Arithmetic

Adding an integer to a pointer advances it by that many elements (scaled by the pointed-to type size).

```cst
let is *i32 as p = &arr[0];
let is *i32 as next = p + 1;  // points to arr[1]
```
