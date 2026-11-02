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
/tmp/bench-run            # every benchmark: -O2 fastest of 5 runs, -O0 run once
/tmp/bench-run 3          # fastest of 3
/tmp/bench-run 5 kernels/ # only paths containing "kernels/"
/tmp/bench-run --no-O0    # -O2 only, the quickest full pass
/tmp/bench-run --full     # -O0 timed as the fastest of N runs too
```

`run.cst` builds every `.cst` in the subdirectories of `benchmarks/` (except
`lib/`, which holds support modules) at -O0 and -O2 into `/tmp/caustic-bench/`, runs it with its output discarded, and
prints the fastest wall-clock time of N runs at -O2, the time of the single
-O0 run, the -O2/-O0 ratio, and the geometric means of the -O2 times and of
the ratios. Several benchmarks take a minute or more at -O0, so by default
the -O0 build only runs once, which is enough to check its result; `--full`
times it like -O2 (the ratios are then fastest against fastest), and
`--no-O0` leaves -O0 out. A benchmark that
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
| `array_divide` | `a[i] /= b[i]` over 2048 f64s, 131072 times: chains of dependent divisions |
| `ascii_lowercase_u16` | lower-casing UTF-16 strings of 0 to 31 characters with an all-ASCII test, 10M times each |
| `bit_reverse` | 32- and 64-bit bit reversal by group swaps for 16M words, checked against a byte table |
| `bit_sieve` | sieve to 40.96M with one bit per number: shifts, masks, test-and-flip |
| `bubble_sort` | bubble sort of 500 integers from a 16-bit generator, redone 100 times |
| `cube_packing` | packs a 5x5x5 cube with 18 box-shaped pieces by depth-first search over a 512-cell grid, 100 times |
| `double_matrix_product` | two 40x40 f64 matrices multiplied through a per-entry inner-product call, 10 times |
| `eight_queens` | first eight-queens solution by backtracking with column and diagonal flags, 5000 times |
| `f32_dot_widen` | 500000 dot products of 2048 f32s accumulated in f64: float arithmetic and conversions |
| `fasta_generate` | 51 MB of generated DNA text: single-precision random draws, linear search, buffered output |
| `fft_256_f32` | 20 chained single-precision FFTs of 256 points with a Taylor-series twiddle table, 10 times |
| `fft_2d_roundtrip` | 63 forward and inverse 256x256 complex FFTs, multi-dimensional radix-2, f64 |
| `fft_correlation_1024` | 150000 correlations of 1024 complex points through a split radix-4 FFT and its inverse |
| `fib_recursive` | doubly recursive Fibonacci(43) on u64: call overhead |
| `find_duplicate` | Fisher-Yates shuffle of 500K ints, then XOR scans to find the repeat |
| `float_matrix_product` | two 40x40 f32 matrices multiplied through a per-entry inner-product call, 5000 times |
| `fp_mix_loops` | ten synthetic loop modules: f64 scalars and arrays, branches, integer indexing, atan/sin/cos, calls, sqrt/exp/log |
| `gradient_noise_3d` | 42.7M samples of 3-D gradient noise: floor, permutation lookups, fade curves, blending |
| `hanoi_cell_stacks` | Towers of Hanoi (14 discs) on pegs kept as linked stacks in a cell array, 100 times |
| `heapsort_doubles` | heapsort of 8M doubles: data-dependent branches and loads |
| `hello` | one line of output: start-up and exit cost |
| `int_matrix_product` | two 40x40 i32 matrices multiplied through a per-entry inner-product call, 10 times |
| `lcg_random` | 400M steps of a linear congruential generator: integer division and int-to-double |
| `lens_ray_trace` | 1M ray traces through a four-surface lens with sin, asin, tan and cos, checked to 11 decimals |
| `lu_solve_f32_100` | f32 LU factorization and solve of a 100x100 system, about 10000 times, at two leading dimensions |
| `lu_solve_f64_2000` | one f64 LU factorization and solve of a 2000x2000 system stored by columns |
| `mandelbrot_ascii` | a 78x78 picture of the Mandelbrot set, 65536 steps per inside point |
| `mandelbrot_complex_sum` | 10 Mandelbrot steps on each of 25M points with a hypot escape test, final values summed |
| `matmul_4x4_f64` | a 4x4 f64 matrix product through a local result array, 50M times |
| `matmul_int_10` | 10x10 integer matrix product through row pointers, 3M times |
| `monte_carlo_pi` | pi from 40M points of two small f32/integer congruential generators |
| `nbody_planets` | 5M steps of a five-body gravity simulation: f64 arithmetic and sqrt |
| `nested_loops` | six nested counting loops (46^6 increments of a wrapping i32) |
| `pancake_flips` | maximum prefix-reversal count over all permutations of 11 |
| `partial_sums` | nine series to 2.5M terms: pow, sqrt, sin, cos and f64 division |
| `planet_positions` | positions of eight planets from orbital elements for 730500 days: sin, cos, atan2, fmod |
| `poisson_jacobi_3d` | 64 Jacobi sweeps of a 19-point stencil on a 64x64x128 f32 grid with per-point coefficients |
| `queens_count_14` | counts all 365596 placements of 14 queens, walking ownership arrays with pointers |
| `quicksort_ints` | recursive quicksort of 5000 integers from a 16-bit generator, redone 100 times |
| `recursion_mix` | Ackermann, Fibonacci and Takeuchi on ints and doubles |
| `recursive_permute` | all permutations of 7 by recursive swaps, 500 times (4.3M calls) |
| `reed_solomon_codec` | (204, 188) Reed-Solomon over GF(2^8) on 150000 random blocks with up to 127 byte errors each: table lookups, Berlekamp, Chien search |
| `sieve_8192` | byte-per-number sieve to 8192, 170K times |
| `signature_register` | two 10-bit linked-list signature registers fed noisy inputs, 100000 trials of a 48-bit generator |
| `spectral_norm` | power method on a 2000x2000 implicit matrix: nested f64 loops |
| `stream_cipher_20r` | 33.6M blocks of a 20-round add-rotate-xor cipher core on u32 words, drawn one word at a time |

`kernels/fp_modules/` — eight long f64 loops, each integrating a function
whose exact value is known, with sin and cos as truncated polynomial series
(shared code in `lib/fp_modules.cst`). Each prints only the sign of its error,
next to timing columns kept at zero, so the output does not depend on the
machine.

| benchmark | measures |
|---|---|
| `fp_modules/rational_integral` | a rational function over [0, 1] in 312.5M steps, one division each |
| `fp_modules/pi_series` | pi from 156.25M terms of the atan(1) series |
| `fp_modules/sine_integral` | the sin series by the trapezoidal rule, 312.5M steps |
| `fp_modules/cosine_integral` | the cos series, 156.25M steps |
| `fp_modules/tangent_integral` | sin over cos, one division a step, 156.25M steps |
| `fp_modules/sine_cosine_integral` | sin times cos, 156.25M steps |
| `fp_modules/reciprocal_integrals` | three reciprocals of polynomials, three divisions a step, 156.25M steps |
| `fp_modules/sine_cosine_squared_integral` | sin times cos squared, 156.25M steps |
| `fp_modules/all_modules` | all eight in a row, the first doubling its step count from 31250 to 512M, the others 62.9M steps each |

Dense-array kernels, in `kernels/stats/`, `kernels/linalg/`,
`kernels/solver/`, `kernels/image/`, `kernels/dp/` and `kernels/stencil/`.
They share `lib/arrays.cst`: zeroed arrays from the page allocator, and an
output that prints each result array to stderr, 16 characters per element
(the low four bits of each byte of the double, plus `'0'`, twice), or as
`%0.2f` text for a few. Sizes run from tens to thousands per dimension; most
are dominated by n^3 loops over row-major `f64` arrays. Several compute their
result twice, into two copies, and check that the copies agree within 1e-5
before printing one.

| benchmark | measures |
|---|---|
| `stats/correlation` | correlation matrix of 1200 variables over 1400 samples: column statistics, sqrt, then column dot products |
| `stats/covariance` | covariance matrix of 80 variables over 100 samples |
| `linalg/gemm` | C = aAB + bC, 1000x1200 by 1200x1100, i-k-j order, done twice |
| `linalg/gemver` | rank-two update of a 2000x2000 matrix, then two matrix-vector products (one down columns) |
| `linalg/gesummv` | y = aAx + bBx on 90x90 matrices |
| `linalg/symm` | C = aAB + bC with A symmetric (lower triangle), 60x80 |
| `linalg/syr2k` | symmetric rank-2k update of a 1200x1200 lower triangle, k = 1000, done twice |
| `linalg/syrk` | symmetric rank-k update of a 1200x1200 lower triangle, k = 1000, done twice |
| `linalg/trmm` | B = aA'B with A 1000x1000 unit lower triangular, B 1000x1200 |
| `linalg/two_matmuls` | D = aABC + bD through a temporary, 40 to 80 per side |
| `linalg/three_matmuls` | G = (AB)(CD) with sides from 800 to 1200, inner loop down a column |
| `linalg/normal_matvec` | y = A'(Ax) for A 1900x2100, one row at a time |
| `linalg/bicg_step` | s = A'r and q = Ap for A 2100x1900 in one sweep |
| `linalg/tensor_contract` | each 160-entry row of a 150x140x160 array times a 160x160 matrix, done twice |
| `linalg/matvec_pair` | x1 += Ay1 along rows and x2 += A'y2 down columns, 2000x2000, done twice |
| `solver/cholesky` | Cholesky factorization of a 2000x2000 matrix built as LL' by a triple loop, factorized twice |
| `solver/levinson_durbin` | Levinson-Durbin recursion for a Toeplitz system of order 2000, done twice |
| `solver/gram_schmidt` | modified Gram-Schmidt QR of a 1000x1200 matrix; prints 2.6M values as text |
| `solver/lu_substitute` | LU factorization of a 2000x2000 matrix, then forward and back substitution, matrix built and solved twice |
| `solver/lu_doolittle` | LU factorization in place of a 2000x2000 matrix, matrix built and factorized twice |
| `solver/forward_substitution` | lower triangular solve, 2000x2000 |
| `image/deriche` | Deriche recursive edge filter on a 4096x2160 `f32` image: four directional recursions, done twice |
| `dp/floyd_warshall` | all-pairs shortest paths on 2800 nodes of `i32` weights, done twice |
| `dp/nussinov` | RNA base-pair maximization by dynamic programming over a 2500x2500 `i32` table, done twice |
| `stencil/adi` | alternating-direction implicit heat solver, 1000x1000, 500 steps of tridiagonal solves |
| `stencil/fdtd_2d` | 2-D finite-difference time-domain fields, 1000x1200, 500 steps, done twice |
| `stencil/heat_3d` | 7-point heat stencil on 120^3, 500 steps |
| `stencil/jacobi_1d` | 3-point Jacobi stencil on 2000 points, 500 steps, done twice |
| `stencil/jacobi_2d` | 5-point Jacobi stencil on 1300x1300, 500 steps, done twice |
| `stencil/seidel_2d` | 9-point Gauss-Seidel stencil in place on 2000x2000, 500 steps, done twice |

`programs/` — data structures, strings and dispatch through function pointers:

| benchmark | measures |
|---|---|
| `binary_tree_sort` | binary search tree of 5000 keys built by recursive insertion and checked, 100 times |
| `bounce_balls` | 100 balls bouncing in a box for 50 steps, 15000 times: small-record updates and branches |
| `chomp_game_tree` | solves 8x7 chomp over linked lists of positions and moves with linear-search lookups, then plays it |
| `collision` | collision detection for 250 aircraft over 200 frames, 10 times: red-black trees, voxel flood fill, records in vectors, callbacks, f64 |
| `constraints` | incremental constraint solver on chains and projections of 12000, 10 times: method tables, vectors, visitor callbacks, hashing |
| `device_scheduler` | the task scheduler simulation in systems-language style for 10M idle steps: global registers, a state switch, packet queues headed by registers |
| `dispatch_interpreter` | 2047-command bytecode run 100000 times through a table of handler pointers |
| `expression_search` | iterative-deepening search for an arithmetic expression over three numbers |
| `hash_table_strings` | chained hash table of 3.5M string keys: hashing, allocation, string compares |
| `huffman_roundtrip` | Huffman encode and decode of 10 MB of text, 30 times: heaps, bit packing, table-driven decode |
| `json_parse` | parses a generated 25 KB JSON document 1000 times: character tests, string copies, vectors of values |
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
closures and virtual methods is bridged as follows, and a few others differ
from their originals where noted; the work done is meant to stay the same.

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
- `dispatch_interpreter` jumps from command to command through a table of
  function pointers, one call per command, where its original jumps through
  a table of label addresses inside one function.
- `fp_modules/all_modules` always doubles its first module's step count up
  to the 512M cap; the original stops early once a pass takes a second,
  which no pass does on current machines. Like the original, it prints its
  timing columns scaled to zero.
