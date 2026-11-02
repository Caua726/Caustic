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
| `bubble_sort` | bubble sort of 500 integers from a 16-bit generator, redone 100 times |
| `cube_packing` | packs a 5x5x5 cube with 18 box-shaped pieces by depth-first search over a 512-cell grid, 100 times |
| `double_matrix_product` | two 40x40 f64 matrices multiplied through a per-entry inner-product call, 10 times |
| `eight_queens` | first eight-queens solution by backtracking with column and diagonal flags, 5000 times |
| `fasta_generate` | 51 MB of generated DNA text: single-precision random draws, linear search, buffered output |
| `fft_256_f32` | 20 chained single-precision FFTs of 256 points with a Taylor-series twiddle table, 10 times |
| `fib_recursive` | doubly recursive Fibonacci(43) on u64: call overhead |
| `find_duplicate` | Fisher-Yates shuffle of 500K ints, then XOR scans to find the repeat |
| `float_matrix_product` | two 40x40 f32 matrices multiplied through a per-entry inner-product call, 5000 times |
| `hanoi_cell_stacks` | Towers of Hanoi (14 discs) on pegs kept as linked stacks in a cell array, 100 times |
| `heapsort_doubles` | heapsort of 8M doubles: data-dependent branches and loads |
| `hello` | one line of output: start-up and exit cost |
| `int_matrix_product` | two 40x40 i32 matrices multiplied through a per-entry inner-product call, 10 times |
| `lcg_random` | 400M steps of a linear congruential generator: integer division and int-to-double |
| `lu_solve_f32_100` | f32 LU factorization and solve of a 100x100 system, about 10000 times, at two leading dimensions |
| `lu_solve_f64_2000` | one f64 LU factorization and solve of a 2000x2000 system stored by columns |
| `matmul_int_10` | 10x10 integer matrix product through row pointers, 3M times |
| `nbody_planets` | 5M steps of a five-body gravity simulation: f64 arithmetic and sqrt |
| `nested_loops` | six nested counting loops (46^6 increments of a wrapping i32) |
| `pancake_flips` | maximum prefix-reversal count over all permutations of 11 |
| `partial_sums` | nine series to 2.5M terms: pow, sqrt, sin, cos and f64 division |
| `planet_positions` | positions of eight planets from orbital elements for 730500 days: sin, cos, atan2, fmod |
| `queens_count_14` | counts all 365596 placements of 14 queens, walking ownership arrays with pointers |
| `quicksort_ints` | recursive quicksort of 5000 integers from a 16-bit generator, redone 100 times |
| `recursion_mix` | Ackermann, Fibonacci and Takeuchi on ints and doubles |
| `recursive_permute` | all permutations of 7 by recursive swaps, 500 times (4.3M calls) |
| `sieve_8192` | byte-per-number sieve to 8192, 170K times |
| `signature_register` | two 10-bit linked-list signature registers fed noisy inputs, 100000 trials of a 48-bit generator |
| `spectral_norm` | power method on a 2000x2000 implicit matrix: nested f64 loops |

`programs/` — data structures, strings and dispatch through function pointers:

| benchmark | measures |
|---|---|
| `binary_tree_sort` | binary search tree of 5000 keys built by recursive insertion and checked, 100 times |
| `bounce_balls` | 100 balls bouncing in a box for 50 steps, 15000 times: small-record updates and branches |
| `chomp_game_tree` | solves 8x7 chomp over linked lists of positions and moves with linear-search lookups, then plays it |
| `collision` | collision detection for 250 aircraft over 200 frames, 10 times: red-black trees, voxel flood fill, records in vectors, callbacks, f64 |
| `constraints` | incremental constraint solver on chains and projections of 12000, 10 times: method tables, vectors, visitor callbacks, hashing |
| `expression_search` | iterative-deepening search for an arithmetic expression over three numbers |
| `hash_table_strings` | chained hash table of 3.5M string keys: hashing, allocation, string compares |
| `huffman_roundtrip` | Huffman encode and decode of 10 MB of text, 30 times: heaps, bit packing, table-driven decode |
| `json_parse` | parses a 25 KB JSON document 1000 times: character tests, string copies, vectors of values |
| `linked_lists` | building, copying, draining and reversing 100-node doubly linked lists, 3M times |
| `list_tail` | the Takeuchi function on linked lists, 15000 times: allocation and recursive list walks |
| `loopfind` | loop recognition on a 5213-block control-flow graph, 1551 passes, 10 times: union-find, sets, hash dictionary, callbacks |
| `mandelbrot_sum` | 500x500 Mandelbrot set with packed escape bits, 10 times: f64 inner loop |
| `method_calls` | 1G calls through function pointers stored in objects |
| `nbody_energy` | 250000 steps of the five-body simulation through accessor functions, 10 times |
| `object_alloc` | 140M allocate-initialise-free cycles of small objects |
| `permute_count` | all permutations of 6 by recursive swaps in a heap array, 10000 times |
| `queens_solve` | first eight-queens solution with heap-allocated flag arrays, 100000 times |
| `record_ops_double` | the record and string rounds of `record_ops_int` with doubles for every integer, 100M times |
| `record_ops_int` | synthetic integer workload of record fields, short strings, small arrays and calls, 100M rounds |
| `scheduler` | operating-system task scheduler simulation, 1000 times: linked packet queues, task functions called through pointers |
| `sieve_count` | sieve to 5000 in a fresh stack array, 30000 times |
| `storage_tree` | trees of heap arrays seven levels deep, built and freed 10000 times: allocator throughput |
| `string_append` | 10M appends to a doubling heap buffer |
| `towers_moves` | Towers of Hanoi with heap-allocated disks on linked piles, 6000 times |

## Deviations

Where a program follows an object-oriented original, the language's lack of
closures and virtual methods is bridged as follows; the work done is meant to
stay the same.

- A closure passed to `forEach` and friends is a function pointer plus a
  pointer to a small context record, called with `call()` once per element.
- Virtual methods are function pointers: `constraints` gives each kind of
  constraint a table of them, and `scheduler` keeps the task function in the
  control block. `json_parse`'s values carry a kind tag instead, tested with
  an `if`, and so do `collision`'s red-black trees for their key comparison,
  which the original specialises per key type at compile time. Entry
  matching in the identity dictionaries is a direct call, as both kinds of
  entry match the same way.
- Sets are vectors searched linearly through a callback that compares
  elements by identity, where the original calls an equality method that
  compares the same thing.
- `json_parse` keeps every captured string as a heap copy (no small-string
  buffer), stores object names by pointer instead of copying them again, and
  holds the current character as a one-character slice of the input.
- `sieve_count` passes its flag array by pointer, not by value.
- Objects the original frees through an object tracker are freed explicitly
  at the same point, and a red-black tree node is freed when it is removed.
