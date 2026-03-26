# std/random.cst

Pseudo-random number generation using the xoshiro256** algorithm. Period: 2^256 - 1. High quality, fast, non-cryptographic. Supports both explicit state via the `Rng` struct and global convenience functions that operate on a module-level default RNG. Seeding uses the Linux `getrandom` syscall (syscall 318).

## Import

```cst
use "std/random.cst" as rng;
```

## Struct

```cst
struct Rng {
    s0 as i64;    // state word 0
    s1 as i64;    // state word 1
    s2 as i64;    // state word 2
    s3 as i64;    // state word 3
}
```

The four 64-bit state words form the 256-bit internal state of the xoshiro256** generator. At least one word must be non-zero.

## Explicit-State Functions (Rng struct)

These functions take a pointer to an `Rng` struct, allowing multiple independent RNG instances.

| Function | Signature | Description |
|----------|-----------|-------------|
| `rng_init` | `fn rng_init(r as *Rng) as void` | Seed the RNG from the `getrandom` syscall (32 bytes of kernel entropy). Falls back to fixed constants if the syscall fails. |
| `rng_seed` | `fn rng_seed(r as *Rng, a as i64, b as i64, c as i64, d as i64) as void` | Manually set all four state words. If all four are zero, fixed fallback constants are used instead. |
| `rng_next` | `fn rng_next(r as *Rng) as i64` | Generate the next 64-bit pseudo-random value (full range, including negative). This is the xoshiro256** core. |
| `rng_next_u8` | `fn rng_next_u8(r as *Rng) as u8` | Generate a random byte (0-255). |
| `rng_range` | `fn rng_range(r as *Rng, min as i64, max as i64) as i64` | Generate a random integer in `[min, max)`. Uses rejection sampling to eliminate modulo bias. Fast path for power-of-two spans. Returns `min` if `max <= min`. |
| `rng_bool` | `fn rng_bool(r as *Rng) as i64` | Generate a random boolean (0 or 1). |
| `rng_shuffle_i64` | `fn rng_shuffle_i64(r as *Rng, arr as *i64, count as i64) as void` | Fisher-Yates shuffle of an i64 array in place. |
| `rng_shuffle_i32` | `fn rng_shuffle_i32(r as *Rng, arr as *i32, count as i64) as void` | Fisher-Yates shuffle of an i32 array in place. |
| `rng_fill_i64` | `fn rng_fill_i64(r as *Rng, arr as *i64, count as i64, min as i64, max as i64) as void` | Fill an i64 array with random values in `[min, max)`. |
| `rng_fill_i32` | `fn rng_fill_i32(r as *Rng, arr as *i32, count as i64, min as i32, max as i32) as void` | Fill an i32 array with random values in `[min, max)`. |

## Global Convenience Functions

These functions use a module-level global `Rng` instance (`_grng`). Call `seed()` once before using them.

| Function | Signature | Description |
|----------|-----------|-------------|
| `seed` | `fn seed() as void` | Initialize the global RNG from `getrandom`. Call this once at program start. |
| `seed_with` | `fn seed_with(a as i64, b as i64, c as i64, d as i64) as void` | Manually seed the global RNG with four state words. Useful for reproducible sequences. |
| `next_i64` | `fn next_i64() as i64` | Generate a random 64-bit integer (full range). |
| `next_u8` | `fn next_u8() as u8` | Generate a random byte (0-255). |
| `range` | `fn range(min as i64, max as i64) as i64` | Random integer in `[min, max)` with no modulo bias. |
| `range_max` | `fn range_max(max as i64) as i64` | Random integer in `[0, max)`. Shorthand for `range(0, max)`. |
| `next_bool` | `fn next_bool() as i64` | Random boolean (0 or 1). |
| `shuffle_i64` | `fn shuffle_i64(arr as *i64, count as i64) as void` | Fisher-Yates shuffle of an i64 array. |
| `shuffle_i32` | `fn shuffle_i32(arr as *i32, count as i64) as void` | Fisher-Yates shuffle of an i32 array. |
| `fill_i64` | `fn fill_i64(arr as *i64, count as i64, min as i64, max as i64) as void` | Fill an i64 array with random values in `[min, max)`. |
| `fill_i32` | `fn fill_i32(arr as *i32, count as i64, min as i32, max as i32) as void` | Fill an i32 array with random values in `[min, max)`. |
| `fill_bytes` | `fn fill_bytes(buf as *u8, count as i64) as void` | Fill a byte buffer with cryptographic-quality random bytes directly from the `getrandom` syscall. This bypasses the xoshiro256** PRNG entirely. |

## Example

### Global convenience API

```cst
use "std/random.cst" as rng;

fn main() as i32 {
    rng.seed();

    let is i64 as dice = rng.range(1, 7);        // 1..6 inclusive
    let is i64 as coin = rng.next_bool();         // 0 or 1
    let is i64 as big  = rng.range_max(1000000);  // 0..999999

    let is [10]i64 as arr;
    rng.fill_i64(&arr, 10, 0, 100);  // fill with values in [0, 100)
    rng.shuffle_i64(&arr, 10);       // shuffle in place

    return 0;
}
```

### Explicit Rng struct (multiple independent generators)

```cst
use "std/random.cst" as rng;

fn main() as i32 {
    let is rng.Rng as r1;
    let is rng.Rng as r2;

    rng.rng_init(&r1);
    rng.rng_seed(&r2, 111, 222, 333, 444);  // deterministic

    let is i64 as a = rng.rng_range(&r1, 0, 100);
    let is i64 as b = rng.rng_range(&r2, 0, 100);  // reproducible

    return 0;
}
```

## Notes

- **Always call `seed()` (or `rng_init`) before generating numbers.** The global RNG is zero-initialized and will produce degenerate output if unseeded.
- `rng_range` uses rejection sampling to eliminate modulo bias. For power-of-two spans, it uses a fast bitwise path instead.
- `fill_bytes` uses the `getrandom` syscall directly and does not use the xoshiro256** PRNG. It handles partial reads by looping until the full buffer is filled.
- `rng_init` reads 32 bytes from the kernel via `getrandom(buf, 32, 0)`. If the syscall returns fewer than 32 bytes (e.g., early boot), fixed fallback constants are used.
- xoshiro256** is not cryptographically secure. Do not use it for security-sensitive applications. Use `fill_bytes` for cryptographic-quality randomness.
