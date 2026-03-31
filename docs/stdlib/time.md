## _module
std/time.cst — Time measurement

Monotonic clock for performance measurement and sleep.

  now_ns()          — current time in nanoseconds
  now_us()          — current time in microseconds
  now_ms()          — current time in milliseconds
  sleep_ms(ms)      — sleep for milliseconds
  elapsed_ms(start) — elapsed ms since timestamp

Usage:
  use "std/time.cst" as time;
  let is i64 as t0 = time.now_ns();
---
## now_ns
fn now_ns() as i64

Get current monotonic time in nanoseconds.

Uses CLOCK_MONOTONIC — not affected by system clock changes.
Best for measuring elapsed time.

Example:
  let is i64 as start = time.now_ns();
  // ... work ...
  let is i64 as elapsed = time.now_ns() - start;
---
## now_us
fn now_us() as i64

Get current monotonic time in microseconds.
---
## now_ms
fn now_ms() as i64

Get current monotonic time in milliseconds.
---
## sleep_ms
fn sleep_ms(ms as i64) as void

Sleep for the given number of milliseconds.

Example:
  time.sleep_ms(500);  // sleep 500ms
---
## sleep_us
fn sleep_us(us as i64) as void

Sleep for the given number of microseconds.
---
## elapsed_ms
fn elapsed_ms(start_ns as i64) as i64

Calculate elapsed milliseconds since a start timestamp.

Example:
  let is i64 as t0 = time.now_ns();
  // ... work ...
  io.write_int(linux.STDOUT, time.elapsed_ms(t0));
---
