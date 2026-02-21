# Floating Point

Caustic supports IEEE 754 floating-point types using SSE registers.

| Type | Size | Precision |
|------|------|-----------|
| `f32` | 4 bytes | Single precision (~7 decimal digits) |
| `f64` | 8 bytes | Double precision (~15 decimal digits) |

## Declaration

Float literals **must** include a decimal point. Using `10` instead of `10.0` will be treated as an integer and cause a type error.

```cst
let is f64 as pi = 3.14159265358979;
let is f32 as ratio = 0.5;
let is f64 as zero = 0.0;
```

## Arithmetic

```cst
let is f64 as a = 10.0;
let is f64 as b = 3.0;

let is f64 as sum = a + b;     // 13.0
let is f64 as diff = a - b;    // 7.0
let is f64 as prod = a * b;    // 30.0
let is f64 as quot = a / b;    // 3.333...
```

## SSE Instructions

The compiler generates SSE instructions for float operations:
- `f64`: `addsd`, `subsd`, `mulsd`, `divsd` (scalar double)
- `f32`: `addss`, `subss`, `mulss`, `divss` (scalar single)

Float values are stored in XMM registers (xmm0-xmm15), separate from general-purpose integer registers.

## No Implicit Conversion

Integers and floats cannot be mixed without an explicit cast.

```cst
let is f64 as x = 10.0;
let is i64 as n = 5;

// ERROR: cannot add f64 and i64 directly
// let is f64 as result = x + n;

// Correct: cast the integer to f64
let is f64 as result = x + cast(f64, n);
```

## Float to Integer

```cst
let is f64 as val = 3.7;
let is i64 as truncated = cast(i64, val);    // 3 (truncates toward zero)
```
