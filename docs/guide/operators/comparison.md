# Comparison Operators

## Operators

| Operator | Description      | Example     |
|----------|-----------------|-------------|
| `==`     | Equal            | `a == b`    |
| `!=`     | Not equal        | `a != b`    |
| `<`      | Less than        | `a < b`     |
| `<=`     | Less or equal    | `a <= b`    |
| `>`      | Greater than     | `a > b`     |
| `>=`     | Greater or equal | `a >= b`    |

## Usage in Conditions

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

## Integer Comparison

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

## Float Comparison

Comparisons work on `f32` and `f64` using SSE compare instructions.

```cst
let is f64 as x = 3.14;
let is f64 as y = 2.71;

if (x > y) {
    // true
}
```

## Pointer Comparison

Pointers can be compared for equality or ordering (address comparison).

```cst
let is *u8 as p = cast(*u8, 0);

if (p == cast(*u8, 0)) {
    // null check
}
```

## Result Type

Comparison operators produce an integer result: `1` for true, `0` for false. They can be used in expressions:

```cst
let is i32 as is_positive = x > 0;  // 1 or 0
```
