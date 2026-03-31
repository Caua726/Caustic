## _module
random — Random Number Generation

Fast pseudo-random numbers using xoshiro256** algorithm.
NOT cryptographically secure — use linux.getrandom() for crypto.

You MUST call seed() before generating numbers:
  random.seed(time.now_ns());  // seed from clock
  // or: seed from system entropy
  let is [8]u8 as e; linux.getrandom(&e, 8, 0);
  random.seed(cast(*i64, &e)[0]);

Generating numbers:
  random.next()         — full-range i64
  random.range(1, 7)    — integer in [1, 7) — i.e. 1 through 6
  random.range(0, 100)  — integer in [0, 100)

range() uses rejection sampling for uniform distribution.
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
