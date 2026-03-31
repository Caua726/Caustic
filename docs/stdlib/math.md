## _module
math — Integer Math Utilities

Common math operations on i64 integers.

  math.abs(x)          — absolute value
  math.min(a, b)       — smaller of two
  math.max(a, b)       — larger of two
  math.pow(base, exp)  — base^exp (no overflow check)
  math.gcd(a, b)       — greatest common divisor
  math.lcm(a, b)       — least common multiple
  math.align_up(n, a)  — round up to multiple of a (power of 2)
  math.align_down(n,a) — round down to multiple of a

Note: Caustic has no float math library yet. For f64 operations
use inline asm or syscall-based approaches.
---
## abs
fn abs(x as i64) as i64

Absolute value of an integer.

Example:
  math.abs(0 - 5)  // 5
---
## min
fn min(a as i64, b as i64) as i64

Return the smaller of two integers.
---
## max
fn max(a as i64, b as i64) as i64

Return the larger of two integers.
---
## pow
fn pow(base as i64, exp as i64) as i64

Integer exponentiation (base^exp).

Warning: no overflow detection. Large results will wrap.
---
## gcd
fn gcd(a as i64, b as i64) as i64

Greatest common divisor using Euclidean algorithm.
---
## lcm
fn lcm(a as i64, b as i64) as i64

Least common multiple. Computed as (a * b) / gcd(a, b).
---
## align_up
fn align_up(n as i64, align as i64) as i64

Round n up to the next multiple of align. Align must be a power of 2.

Example:
  math.align_up(13, 8)  // 16
  math.align_up(16, 8)  // 16
---
## align_down
fn align_down(n as i64, align as i64) as i64

Round n down to the previous multiple of align. Align must be a power of 2.
---
