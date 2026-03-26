# std/math.cst

Integer math utility functions. Provides absolute value, min/max, clamping, exponentiation, GCD/LCM, sign, ceiling division, power-of-two checks, and alignment helpers. All functions are pure (no allocations, no side effects).

## Import

```cst
use "std/math.cst" as math;
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs_i64` | `fn abs_i64(n as i64) as i64` | Absolute value of a 64-bit integer. Returns `INT64_MAX` for `INT64_MIN` (safe saturation). |
| `abs_i32` | `fn abs_i32(n as i32) as i32` | Absolute value of a 32-bit integer. |
| `min_i64` | `fn min_i64(a as i64, b as i64) as i64` | Return the smaller of two i64 values. |
| `max_i64` | `fn max_i64(a as i64, b as i64) as i64` | Return the larger of two i64 values. |
| `min_i32` | `fn min_i32(a as i32, b as i32) as i32` | Return the smaller of two i32 values. |
| `max_i32` | `fn max_i32(a as i32, b as i32) as i32` | Return the larger of two i32 values. |
| `clamp_i64` | `fn clamp_i64(val as i64, lo as i64, hi as i64) as i64` | Clamp `val` to the range `[lo, hi]`. |
| `clamp_i32` | `fn clamp_i32(val as i32, lo as i32, hi as i32) as i32` | Clamp `val` to the range `[lo, hi]`. |
| `pow_i64` | `fn pow_i64(base as i64, exp as i64) as i64` | Integer exponentiation via binary exponentiation. Returns 0 for negative exponents, 1 for exponent 0. |
| `gcd` | `fn gcd(a as i64, b as i64) as i64` | Greatest common divisor (Euclidean algorithm). Works with negative inputs. |
| `lcm` | `fn lcm(a as i64, b as i64) as i64` | Least common multiple. Returns 0 if either input is 0. Computes via `(|a| / gcd) * |b|` to avoid overflow. |
| `sign_i64` | `fn sign_i64(n as i64) as i64` | Returns 1 if positive, -1 if negative, 0 if zero. |
| `sign_i32` | `fn sign_i32(n as i32) as i32` | Returns 1 if positive, -1 if negative, 0 if zero. |
| `div_ceil` | `fn div_ceil(a as i64, b as i64) as i64` | Ceiling division. Rounds toward positive infinity for same-sign operands. Returns 0 if `b` is 0. |
| `is_power_of_two` | `fn is_power_of_two(n as i64) as i64` | Returns 1 if `n` is a power of two, 0 otherwise. Non-positive values return 0. |
| `align_up` | `fn align_up(val as i64, align as i64) as i64` | Round `val` up to the nearest multiple of `align`. `align` **must** be a power of two. |
| `align_down` | `fn align_down(val as i64, align as i64) as i64` | Round `val` down to the nearest multiple of `align`. `align` **must** be a power of two. |

## Example

```cst
use "std/math.cst" as math;

fn main() as i32 {
    let is i64 as a = math.abs_i64(0 - 42);        // 42
    let is i64 as lo = math.min_i64(10, 20);        // 10
    let is i64 as hi = math.max_i64(10, 20);        // 20
    let is i64 as c = math.clamp_i64(100, 0, 50);   // 50
    let is i64 as p = math.pow_i64(2, 10);           // 1024
    let is i64 as g = math.gcd(12, 8);               // 4
    let is i64 as l = math.lcm(4, 6);                // 12
    let is i64 as s = math.sign_i64(0 - 7);          // -1
    let is i64 as d = math.div_ceil(7, 3);           // 3
    let is i64 as aligned = math.align_up(100, 64);  // 128

    return 0;
}
```

## Notes

- No heap allocation is required. These functions can be called before `mem.gheapinit()`.
- `align_up` and `align_down` use bitwise tricks and require the alignment to be a power of two. Passing a non-power-of-two produces undefined results.
- `abs_i64(INT64_MIN)` saturates to `INT64_MAX` (9223372036854775807) rather than overflowing.
- `pow_i64` uses O(log n) binary exponentiation. No overflow checking is performed.
