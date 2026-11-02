# Benchmarks

Programs written in Caustic that time the compiler's output. Each one checks
its own result and exits non-zero if it is wrong, so every benchmark is also a
test of -O0 against -O2.

The selection and methodology were inspired by the llvm-test-suite and Are We
Fast Yet suites, and the geometric-mean scoring by SPEC CPU; the programs and
their algorithms are our own.

## Running

From the repository root:

```sh
./caustic benchmarks/run.cst -o /tmp/bench-run
/tmp/bench-run            # every benchmark, fastest of 5 runs
/tmp/bench-run 3          # fastest of 3
/tmp/bench-run 5 kernels/ # only paths containing "kernels/"
```

`run.cst` builds every `.cst` in the subdirectories of `benchmarks/` at -O0
and -O2 into `/tmp/caustic-bench/`, runs it with its output discarded, and
prints the fastest wall-clock time of N runs at each level, the -O2/-O0 ratio,
and the geometric means of the -O2 times and of the ratios. A benchmark that
fails to build or exits non-zero is reported and left out of the means; the
runner's exit code is the number of such failures.

A single benchmark runs on its own like any program:

```sh
./caustic -O2 benchmarks/kernels/nbody_planets.cst -o /tmp/nbody && /tmp/nbody
```

## What each one measures

`kernels/` — loops, recursion, arithmetic and arrays:

| benchmark | measures |
|---|---|
| `ackermann` | Ackermann A(3, 8): deep recursion with tiny frames |
| `array_add` | `y[i] += x[i]` over 1.5M i32s, backwards, 1000 times: a memory-bound loop |
| `bit_sieve` | sieve to 40.96M with one bit per number: shifts, masks, test-and-flip |
| `fasta_generate` | 51 MB of generated DNA text: single-precision random draws, linear search, buffered output |
| `fib_recursive` | doubly recursive Fibonacci(43) on u64: call overhead |
| `find_duplicate` | Fisher-Yates shuffle of 500K ints, then XOR scans to find the repeat |
| `heapsort_doubles` | heapsort of 8M doubles: data-dependent branches and loads |
| `hello` | one line of output: start-up and exit cost |
| `lcg_random` | 400M steps of a linear congruential generator: integer division and int-to-double |
| `matmul_int_10` | 10x10 integer matrix product through row pointers, 3M times |
| `nbody_planets` | 5M steps of a five-body gravity simulation: f64 arithmetic and sqrt |
| `nested_loops` | six nested counting loops (46^6 increments of a wrapping i32) |
| `pancake_flips` | maximum prefix-reversal count over all permutations of 11 |
| `partial_sums` | nine series to 2.5M terms: pow, sqrt, sin, cos and f64 division |
| `recursion_mix` | Ackermann, Fibonacci and Takeuchi on ints and doubles |
| `sieve_8192` | byte-per-number sieve to 8192, 170K times |
| `spectral_norm` | power method on a 2000x2000 implicit matrix: nested f64 loops |

`programs/` — data structures, strings and dispatch through function pointers:

| benchmark | measures |
|---|---|
| `hash_table_strings` | chained hash table of 3.5M string keys: hashing, allocation, string compares |
| `linked_lists` | building, copying, draining and reversing 100-node doubly linked lists, 3M times |
| `method_calls` | 1G calls through function pointers stored in objects |
| `object_alloc` | 140M allocate-initialise-free cycles of small objects |
| `string_append` | 10M appends to a doubling heap buffer |
