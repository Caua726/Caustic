# Linux AArch64 Backend

Select the backend with:

```sh
./caustic --target=linux-aarch64 program.cst -o program-aarch64
qemu-aarch64 ./program-aarch64
```

The production pipeline is fully internal:

```text
Caustic IR -> AArch64 assembly -> ELF64 AArch64 .o -> static ELF64 executable
```

No external assembler, compiler, or linker is invoked.

## Code generation

`src/codegen/backend.cst` dispatches by target architecture.
`src/codegen/aarch64/driver.cst` implements the scalar backend:

- LP64, little-endian AArch64;
- AAPCS64 register and stack argument layout, with eight integer argument registers;
- integer, pointer, scalar floating-point, cast, memory and control-flow IR;
- direct and indirect calls, variadic register-save/overflow areas;
- Linux syscalls through `x8` plus `x0` through `x5` and `svc #0`;
- 32-bit and 64-bit atomics using acquire/release exclusive loops;
- raw-clone threads and futex synchronization.

The first implementation deliberately uses stack homes for virtual registers.
This keeps values correct across calls and makes the ABI path straightforward;
an AArch64 register allocator can replace it without changing IR or object
formats.

## Assembler and object format

`caustic-assembler/aarch64/assembler.cst` is a two-pass fixed-width assembler.
It handles local branches, scalar integer/FP instructions, loads/stores,
exclusive atomics, inline-assembly instructions used by the standard library,
data directives, symbols and custom sections.

Objects are ELF64 little-endian with `e_machine = EM_AARCH64`. The assembler
emits these RELA types:

- `R_AARCH64_ABS64`
- `R_AARCH64_ADR_PREL_PG_HI21`
- `R_AARCH64_ADD_ABS_LO12_NC`
- `R_AARCH64_CONDBR19`
- `R_AARCH64_JUMP26`
- `R_AARCH64_CALL26`

## Static linker

The shared linker core merges multiple AArch64 objects, dead-strips unreachable
functions, resolves local/global symbols, applies AArch64 instruction
relocations and emits an AArch64 Linux process-entry stub. Static executables
use the same two-load-segment layout as the x86_64 linker.

Shared objects, dynamic executables, PLT/GOT imports and NEON/vector IR are not
part of the current AArch64 target. Requests for shared/dynamic linking are
rejected instead of silently producing an invalid file.

## Validation

Run:

```sh
./caustic-mk run test-aarch64
```

The suite covers scalar arithmetic and floating point, calls with stack
arguments, indirect calls, variadics, globals/relocations, Linux syscalls,
atomics, threads/futexes, multi-object linking, the separate
compiler/assembler/linker CLI path, and execution under `qemu-aarch64`.
It also builds the Caustic compiler itself as a static AArch64 executable, runs
that compiler under QEMU, and uses it to produce another AArch64 program.

When GNU AArch64 binutils are installed, the suite additionally assembles a
corpus with GNU `as` and compares its `.text` bytes with the internal
assembler. GNU tools are test oracles only.
