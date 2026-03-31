## _module
std/random.cst — Pseudo-random number generator

xoshiro256** PRNG with rejection sampling for ranges.

  seed(s)      — initialize PRNG state
  next()       — next random i64
  range(lo,hi) — random integer in [lo, hi)

Usage:
  use "std/random.cst" as random;
  random.seed(time.now_ns());
  let is i64 as dice = random.range(1, 7);
---
## seed
fn seed(s as i64) as void

Seed the PRNG (xoshiro256**).

Call once at program start. Use a time-based value or linux.getrandom()
for a good seed.

Example:
  let is [2]i64 as ts;
  linux.clock_gettime(1, cast(*u8, &ts));
  random.seed(ts[1]);
---
## next
fn next() as i64

Generate the next pseudo-random 64-bit integer.

Full range: 0 to 2^64-1 (unsigned interpretation).

Example:
  let is i64 as r = random.next();
---
## range
fn range(lo as i64, hi as i64) as i64

Generate a random integer in [lo, hi) using rejection sampling.

Parameters:
  lo - inclusive lower bound
  hi - exclusive upper bound

Example:
  let is i64 as dice = random.range(1, 7);  // 1-6
---
