# Build performance

Measured on an AMD Zen 3 with 12 threads, by `tools/build-metrics.py`: wall clock, and
the peak of the summed PSS of the compiler and its workers. "browser" is caustic-browser
at 3290cca, 195 542 lines in 497 modules, built `-j 4` (`--cpu=znver3` at -O2). "self-host"
is `src/main.cst`.

## Cold builds

| Build | Wall before | Wall after | PSS before | PSS after | Largest RSS after |
|---|---|---|---|---|---|
| browser -O0 | 16.25 s | 1.50 s | 550 MB | 156 MB | 152 MB |
| browser -O1 | 17.58 s | 2.00 s | 465 MB | 153 MB | 151 MB |
| browser -O2 | 23.03 s | 3.21 s | 537 MB | 174 MB | 156 MB |
| self-host -O0 | 0.87 s | 0.51 s | 227 MB | 65 MB | 69 MB |
| self-host -O1 | 1.17 s | 0.73 s | 209 MB | 72 MB | 65 MB |
| self-host -O2 | 2.58 s | 1.30 s | 209 MB | 96 MB | 71 MB |

Through `caustic-mk build browser` at -O2 with `-j 4` in the manifest, counting the maker
and the shell: 3.22 s and 177 MiB, which is 185 MB.

Figures are MiB, sampled, and move by a few between runs. More workers cost more memory:
each holds its own working set. The cgroup's `memory.peak`, which also counts page cache,
reads 202 for browser -O2.

## Warm builds

Browser at -O2 through `caustic-mk`, with `-j 4` in the manifest. Every warm binary here was
compared with a clean build of the same sources and is identical.

| Build | Wall | PSS |
|---|---|---|
| cold | 3.27 s | 189 MB |
| nothing changed, first time after a build | 0.01 s | 1 MB |
| one line in a function body | 1.40 s | 152 MB |
| a comment | 1.22 s | 149 MB |
| a new function | 1.21 s | 149 MB |

A build is up to date when the stamp in `.caustic/stamps/` still holds: the configuration
from `caustic --config-id`, the output, and the content of every module. An edit reuses the
object of each module whose key did not change (`--obj-cache`, in `.caustic/objs/<config>/`).

## What the time was

| Cause | Effect on browser -O2 |
|---|---|
| `emit_rodata` walked every global for every string, per module | 23.0 s to 9.0 s |
| Functions main does not reach were compiled, then dropped by the linker | 9.0 s to 6.1 s |
| Passes mapped and unmapped large arrays per function: 188 000 pairs, 7 million faults | 6.1 s to 3.3 s |

`_outside_callers` walked all the IR once per module, in each worker; it now runs once.

## What the memory was

At the end of IR generation on the browser, before: 204 MiB of function-body syntax tree,
131 MiB of IR, 55 MiB of tokens, and the rest declarations, symbols and sources.

| Change | Effect |
|---|---|
| A body is parsed, analyzed, lowered and dropped in turn | the tree of one function at a time |
| Tokens go when their module's last body is done | 55 MiB |
| IR is kept packed, about 12 bytes an instruction, and opened when read | 113 MiB to 12 MiB |
| The working pool is emptied per function | what passes dropped no longer accumulates |
| The assembler frees its buffers per module | about 100 MiB across a build |
| Declarations, types and symbols have no bin rounding or header | about 20 MiB |

Streaming and packing apply to the parallel path. x86_64 only packs; other paths build as before.
