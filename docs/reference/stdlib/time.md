# std/time.cst -- Time

Monotonic clock, wall-clock timestamps, sleep, and elapsed-time measurement. All functions use raw Linux syscalls (`clock_gettime`, `nanosleep`) with no libc dependency.

## Dependencies

```cst
use "linux.cst" as linux;
```

## Import

```cst
use "std/time.cst" as time;
```

## Functions

### Current Time (Monotonic)

These use `CLOCK_MONOTONIC` -- they never jump backwards and are suitable for measuring durations.

| Function | Signature | Description |
|----------|-----------|-------------|
| `now_ns` | `fn now_ns() as i64` | Current monotonic time in nanoseconds |
| `now_us` | `fn now_us() as i64` | Current monotonic time in microseconds |
| `now_ms` | `fn now_ms() as i64` | Current monotonic time in milliseconds |
| `now_s` | `fn now_s() as i64` | Current monotonic time in seconds |

### Wall Clock (Real Time)

These use `CLOCK_REALTIME` -- Unix timestamps that correspond to calendar time. Subject to NTP adjustments.

| Function | Signature | Description |
|----------|-----------|-------------|
| `unix_time` | `fn unix_time() as i64` | Seconds since Unix epoch (1970-01-01) |
| `unix_time_ms` | `fn unix_time_ms() as i64` | Milliseconds since Unix epoch |

### Sleep

Block the current process for the specified duration.

| Function | Signature | Description |
|----------|-----------|-------------|
| `sleep_s` | `fn sleep_s(secs as i64) as void` | Sleep for `secs` seconds |
| `sleep_ms` | `fn sleep_ms(ms as i64) as void` | Sleep for `ms` milliseconds |
| `sleep_us` | `fn sleep_us(us as i64) as void` | Sleep for `us` microseconds |
| `sleep_ns` | `fn sleep_ns(ns as i64) as void` | Sleep for `ns` nanoseconds |

### Elapsed Measurement

All elapsed functions take a `start_ns` value (obtained from `now_ns()`) and return the time that has passed since that point.

| Function | Signature | Description |
|----------|-----------|-------------|
| `elapsed_ns` | `fn elapsed_ns(start_ns as i64) as i64` | Nanoseconds elapsed since `start_ns` |
| `elapsed_us` | `fn elapsed_us(start_ns as i64) as i64` | Microseconds elapsed since `start_ns` |
| `elapsed_ms` | `fn elapsed_ms(start_ns as i64) as i64` | Milliseconds elapsed since `start_ns` |
| `elapsed_s` | `fn elapsed_s(start_ns as i64) as i64` | Seconds elapsed since `start_ns` |

## Example

Measure how long an operation takes and print the result:

```cst
use "std/time.cst" as time;
use "std/io.cst" as io;

fn main() as i32 {
    let is i64 as start = time.now_ns();

    // ... do some work ...
    time.sleep_ms(150);

    let is i64 as ms = time.elapsed_ms(start);
    io.printf("elapsed: %d ms\n", ms);

    // Wall clock
    let is i64 as ts = time.unix_time();
    io.printf("unix timestamp: %d\n", ts);

    return 0;
}
```
