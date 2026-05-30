# Caustic Benchmarks

A cross-language micro-benchmark comparing **Caustic** against 13 other
languages / runtimes across 28 build configurations. Every implementation runs
the **exact same five workloads** with the **exact same parameters**, and all 28
agree bit-for-bit on every result (see [Validation](#validation)) — so the
numbers compare *implementations*, not different amounts of work.

> Reproduce everything with a single self-contained Caustic program:
> ```sh
> ./caustic examples/benchmark.cst -o /tmp/benchmark && /tmp/benchmark
> ```
> `examples/benchmark.cst` embeds every language's source, writes them to
> `/tmp/caustic_bench`, detects which toolchains are installed (skipping the
> missing ones), then compiles and runs each. No other-language files are
> checked into the repo — the harness *is* the source of truth.

## Test machine

| | |
|---|---|
| **Date** | 2026-05-30 |
| **CPU** | AMD Ryzen 5 5600X (6 cores / 12 threads, Zen 3) |
| **OS** | Arch Linux, kernel `7.0.10-zen1-1-zen` |
| **Pinning** | all runs pinned to core 2 via `taskset -c 2` |
| **Governor** | `powersave` (results are relative, not absolute peak) |

## Toolchain versions

| Language / runtime | Version | Released |
|--------------------|---------|----------|
| **Caustic** | 0.0.14 | — |
| GCC | 16.1.1 | 2026-04-30 |
| Clang / LLVM | 22.1.5 | — |
| TCC (TinyCC) | 0.9.28rc | 2026-01-17 |
| Rust | 1.97.0-nightly (`36ba2c771`) | 2026-04-23 |
| Zig | 0.17.0-dev.338+0d4f3cc67 | — |
| Go | 1.26.3 | — |
| C++ (g++) | 16.1.1 | 2026-04-30 |
| Java (OpenJDK) | 21.0.11 | 2026-04-21 |
| C# (.NET) | 10.0.108 | — |
| Node.js (V8) | 22.22.2 | — |
| Bun | 1.3.14 | — |
| Deno | 2.8.1 | — |
| LuaJIT | 2.1 (`1774896198`) | 2026 |
| Lua | 5.5.0 | 2025 |
| PHP | 8.5.6 | 2026-05-06 |
| Python (CPython) | 3.14.5 | — |

## The workloads

All five are integer-only, single-threaded, and allocation-light. Parameters are
fixed: `fib(38)`, `sieve(10_000_000)`, `quicksort(1_000_000)`, `matmul(64×64,
×100)`, `collatz(1..1_000_000)`.

| # | Benchmark | What it stresses |
|---|-----------|------------------|
| 1 | **fib(38)** — naive recursive Fibonacci | call overhead, recursion |
| 2 | **sieve(10M)** — Sieve of Eratosthenes | linear memory scan, byte array writes |
| 3 | **sort(1M)** — Lomuto quicksort on a pseudo-random `i64` permutation | branchy compares + swaps, recursion |
| 4 | **matmul(64, ×100)** — `i64` 64×64 matrix multiply, ikj order, 100× | tight nested loops, multiply-add |
| 5 | **collatz(1M)** — shortcut Collatz step-count for 1..1M | integer div/mul/branch in a hot loop |

Each program times **only the compute region** of each benchmark internally
(array setup is excluded for sort/matmul, matching the original `bench.cst`), and
prints microseconds per benchmark.

## Methodology

- **Per-benchmark times** are *compute only* (internal monotonic-clock timing
  inside each process), reported as the **minimum of 5 runs** (min = least
  interference). This excludes process/VM startup, which is the fair way to
  compare JIT/interpreted languages on the compute itself.
- **Wall (hyperfine)** is the full end-to-end process time measured by
  [`hyperfine`](https://github.com/sharkdp/hyperfine) (warmup + ≥6 runs), as
  `mean ± σ` in ms. It **includes** process startup (≈1 ms for native binaries,
  ~30–90 ms for VMs/interpreters) and the array-setup excluded above — so it is
  always a bit larger than the per-benchmark `Total`.
- **Compile** is best-of-2 wall time to produce the binary (AOT toolchains only).
- **Binary** is the size of the produced executable/object (`.class` for Java,
  `.dll` for C#; both still need their runtime installed).
- **Peak RAM** is the maximum resident set size (`ru_maxrss` via `wait4`).
- **Anti-folding:** native programs read the parameters from `argv` (or a
  `volatile` load for Zig) so the optimizer cannot constant-fold `fib(38)` etc.
  Matmul uses a memory barrier / `black_box` so the 100 iterations aren't
  collapsed.

## Validation

All 28 configurations produced **identical checksums** for every benchmark —
proof that each implementation does exactly the same work:

| Benchmark | Shared result (all 28 agree) |
|-----------|------------------------------|
| fib(38) | `39088169` |
| sieve(10M) | `664579` primes |
| sort(1M) | middle element `500000` |
| matmul | `c[0] = 95692` |
| collatz(1M) | max `524` steps |

## Results — runtime

Per-benchmark **compute time** (ms, best of 5) and end-to-end **wall** time
(`hyperfine`, mean ± σ). Sorted by total compute. **Caustic rows in bold.**

| Toolchain | Mode | fib | sieve | sort | matmul | collatz | **Total** | Wall (hyperfine) |
|-----------|------|----:|------:|-----:|-------:|--------:|----------:|-----------------:|
| GCC | -O3 | 40 | 12 | 52 | 7 | 100 | **211** | 219 ± 2 |
| C++ g++ | -O2 | 41 | 12 | 51 | 7 | 106 | **216** | 225 ± 2 |
| C++ g++ | -O3 | 39 | 15 | 52 | 6 | 105 | **216** | 229 ± 3 |
| GCC | -O2 | 43 | 12 | 51 | 7 | 106 | **218** | 226 ± 3 |
| Zig | ReleaseFast | 78 | 14 | 52 | 7 | 79 | **230** | 247 ± 13 |
| Clang | -O3 | 83 | 12 | 49 | 7 | 78 | **231** | 240 ± 4 |
| Rust | release (opt 3) | 70 | 16 | 55 | 12 | 78 | **231** | 244 ± 7 |
| Clang | -O2 | 83 | 12 | 50 | 7 | 79 | **231** | 242 ± 4 |
| Zig | ReleaseSmall | 87 | 17 | 61 | 10 | 79 | **254** | 263 ± 2 |
| GCC | -O1 | 142 | 18 | 50 | 9 | 120 | **339** | 354 ± 10 |
| Java | JIT (HotSpot) | 99 | 37 | 69 | 32 | 151 | **388** | 469 ± 21 |
| Go | go build | 145 | 19 | 57 | 23 | 151 | **395** | 405 ± 4 |
| **Caustic** | **-O1** | **172** | **47** | **121** | **36** | **173** | **550** | **569 ± 6** |
| GCC | -O0 | 171 | 50 | 120 | 65 | 274 | **680** | 697 ± 3 |
| C++ g++ | -O0 | 163 | 111 | 122 | 66 | 271 | **733** | 743 ± 6 |
| TCC | (default) | 187 | 54 | 126 | 49 | 377 | **792** | 820 ± 7 |
| Clang | -O0 | 158 | 77 | 133 | 51 | 385 | **804** | 830 ± 5 |
| **Caustic** | **-O0** | **197** | **133** | **207** | **91** | **390** | **1017** | **1035 ± 7** |
| C# | .NET 10 JIT | 203 | 22 | 160 | 17 | 624 | **1026** | 1085 ± 48 |
| LuaJIT | JIT | 303 | 188 | 236 | 25 | 417 | **1169** | 1221 ± 15 |
| Zig | Debug | 331 | 104 | 346 | 200 | 342 | **1322** | 1370 ± 7 |
| Bun | JIT (JSC) | 209 | 33 | 88 | 23 | 1001 | **1355** | 1435 ± 23 |
| Node.js | JIT (V8) | 312 | 31 | 108 | 30 | 1014 | **1496** | 1553 ± 7 |
| Deno | JIT (V8) | 294 | 30 | 100 | 30 | 1085 | **1538** | 1604 ± 3 |
| Rust | debug (opt 0) | 211 | 109 | 1193 | 170 | 365 | **2048** | 2181 ± 54 |
| PHP | 8.5 (interp) | 1418 | 468 | 1008 | 485 | 1838 | **5217** | 5311 ± 53 |
| Lua | 5.5 (interp) | 1842 | 498 | 1028 | 775 | 2230 | **6372** | 6420 ± 2 |
| Python | CPython | 3888 | 564 | 3405 | 1962 | 5784 | **15604** | 15565 ± 81 |

## Results — build & footprint

Same order. Binary size is the produced artifact; **Java/C# also require their
runtime**, and interpreted/JIT runtimes have no ahead-of-time binary.

| Toolchain | Mode | Compile | Binary | Peak RAM |
|-----------|------|--------:|-------:|---------:|
| GCC | -O3 | 208 ms | 20 KB | 17 MB |
| C++ g++ | -O2 | 813 ms | 20 KB | 17 MB |
| C++ g++ | -O3 | 907 ms | 20 KB | 17 MB |
| GCC | -O2 | 120 ms | 16 KB | 17 MB |
| Zig | ReleaseFast | 10.8 s¹ | 3.8 MB | 17 MB |
| Clang | -O3 | 99 ms | 16 KB | 17 MB |
| Rust | release | 208 ms | 4.2 MB | 17 MB |
| Clang | -O2 | 91 ms | 16 KB | 17 MB |
| Zig | ReleaseSmall | 1.7 s¹ | 136 KB | 17 MB |
| GCC | -O1 | 67 ms | 16 KB | 17 MB |
| Java | JIT | 363 ms | 2.8 KB² | 60 MB |
| Go | go build | 34 ms³ | 2.4 MB | 20 MB |
| **Caustic** | **-O1** | **138 ms** | **28 KB** | **17 MB** |
| GCC | -O0 | 46 ms | 16 KB | 17 MB |
| C++ g++ | -O0 | 900 ms | 30 KB | 17 MB |
| TCC | default | **6 ms** | **7.4 KB** | 17 MB |
| Clang | -O0 | 54 ms | 16 KB | 17 MB |
| **Caustic** | **-O0** | **89 ms** | **24 KB** | **16 MB** |
| C# | .NET 10 | 915 ms | 6 KB² | 35 MB |
| LuaJIT | JIT | — | — | 139 MB |
| Zig | Debug | 294 ms | 9.9 MB | 17 MB |
| Bun | JIT | — | — | 73 MB |
| Node.js | JIT | — | — | 65 MB |
| Deno | JIT | — | — | 86 MB |
| Rust | debug | 89 ms | 4.2 MB | 17 MB |
| PHP | 8.5 | — | — | 185 MB |
| Lua | 5.5 | — | — | 147 MB |
| Python | CPython | — | — | 52 MB |

¹ Zig's compile time includes building the optimized std for the chosen mode; it
uses a global build cache, so warm rebuilds are far faster.
² `.class` / `.dll` only — the JVM / .NET runtime must be installed separately.
³ Go uses a build cache; the number is a warm rebuild.

## Where Caustic lands

For a **self-hosted compiler with no LLVM, no libc, and no runtime**, the result
is genuinely competitive:

- **Caustic -O1 (550 ms compute)** beats `gcc -O0` (680), `g++ -O0` (733),
  **TCC** (792), `clang -O0` (804), **C#/.NET** (1026), **LuaJIT** (1169),
  **Node/Bun/Deno** (1355–1538), Rust *debug* (2048), and every interpreter.
- It trails **`gcc -O2`/`-O3`** (211–218) by ~2.5×, and sits a notch behind
  Java/Go — reasonable company for an optimizer that is itself ~23K lines of
  Caustic.
- **Tiny output:** a 28 KB static binary, in the same league as GCC/Clang
  (16–20 KB) and **100× smaller** than Go (2.4 MB), Rust (4.2 MB) or Zig (3.8 MB).
- **Minimal memory:** ~17 MB peak RSS — on par with C, and **4–11× leaner** than
  the JS runtimes (65–88 MB), LuaJIT/Lua (139–147 MB) or PHP (185 MB).
- **Fast to compile:** 138 ms end-to-end (compile + assemble + link), quicker
  than g++ (0.8 s), C# (0.9 s) or Zig ReleaseFast (10.8 s).

The remaining gap to `-O2` is mostly in `matmul` and `sieve` (vectorization) and
`collatz` (no magic-division on every form yet) — concrete targets for the
optimizer.

## Reproducing

```sh
# from the repo root
./caustic examples/benchmark.cst -o /tmp/benchmark && /tmp/benchmark
```

The harness prints compile time, binary size, per-benchmark time (best of 3) and
peak RSS for each toolchain it finds, and tells you which ones it skipped because
they aren't installed. Numbers will differ from the table above on other
hardware, but the *ordering* is stable. The `hyperfine` wall column above was
produced separately with `hyperfine --warmup 3` over the same binaries.
