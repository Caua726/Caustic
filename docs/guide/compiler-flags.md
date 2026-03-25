# Compiler Flags

The `caustic` compiler accepts the following command-line flags.

## Compilation

### `-c`

Compile only — no main function required. Produces an object file (`.o`) instead of an executable. Used for building libraries or multi-file projects.

### `-o <path>`

Output path. When specified, the compiler runs the **full pipeline**: compile + assemble + link into an executable. Without `-o`, only the `.s` assembly file is generated.

### `-O0`

No optimization (default). Uses the linear scan register allocator. Fast compilation.

### `-O1`

Full optimization pipeline:
- SSA construction (mem2reg with phi nodes)
- Constant and copy propagation
- Store-load forwarding
- Immediate folding
- Loop-invariant code motion (LICM)
- Strength reduction (div/mod to shift, MUL to pointer increment)
- Dead code elimination (DCE)
- Function inlining (small non-recursive functions)
- Graph coloring register allocator (Iterative Register Coalescing)
- Peephole optimizer (MOV fusion, chain bypass, dead MOV elimination)

Slower compilation (~9x for self-compile), but generated code runs ~3.4x faster.

### `-l<name>`

Link a system library dynamically. Passed to the linker. Example: `-lc` links libc.

## Caching

### `--cache <dir>`

Cache directory for serialized tokens, AST, and IR. On rebuild, unchanged imports are loaded from cache instead of re-parsed. Commonly used as `--cache .caustic`.

### `--emit-interface`

Generate a `.csti` interface file containing type and function declarations. Implies `-c` (no main required).

### `--emit-tokens`

Serialize the token list to a `.tokens` binary cache file.

### `--emit-ast`

Serialize the parsed AST to an `.ast` binary cache file.

### `--emit-ir`

Serialize the generated IR to an `.ir` binary cache file.

## Runtime Configuration

### `--max-ram <MB>`

Limit heap memory in megabytes. The compiler normally sizes its heap based on source file complexity. Use `0` for unlimited (default).

### `--path <dir>`

Additional search path for module imports (`use` statements). Can be specified multiple times (max 16 paths). Used to find dependencies in non-standard locations.

### `--profile`

Show per-phase timing breakdown: lex, parse, semantic, IR generation, optimization, codegen, assembler, linker. Also shows per-function codegen profiling and compilation statistics (source bytes, tokens, functions, IR instructions, assembly output, object size).

## Debug

### `-debuglexer`

Print debug output from the tokenizer.

### `-debugparser`

Print debug output from the AST parser.

### `-debugir`

Print IR instructions. At `-O0`: prints after generation. At `-O1`: prints before and after optimization, then exits.

## Examples

```bash
# Full pipeline (compile + assemble + link + run)
./caustic program.cst -o program && ./program

# Library compilation (no main required)
./caustic -c lib.cst -o lib.o

# Optimized build
./caustic -O1 program.cst -o program

# Profile compilation
./caustic --profile program.cst -o program

# Cache for fast rebuilds
./caustic --cache .caustic program.cst -o program

# Dynamic linking with libc
./caustic program.cst -o program -lc

# Multi-file compilation
./caustic main.cst lib.cst -o program

# Debug IR output
./caustic -debugir -O1 program.cst

# Quick test one-liner
./caustic test.cst -o /tmp/t && /tmp/t; echo "Exit: $?"
```

## Multi-File Compilation

When not using `-o`, you can manually run each pipeline stage:

```bash
# 1. Compile library (no main)
./caustic -c utils.cst

# 2. Compile main program
./caustic main.cst

# 3. Assemble both
./caustic-as utils.cst.s
./caustic-as main.cst.s

# 4. Link together
./caustic-ld utils.cst.s.o main.cst.s.o -o program
```

Use `extern fn` to declare cross-object function signatures:

```cst
// main.cst
extern fn add(a as i32, b as i32) as i32;

fn main() as i32 {
    return add(10, 20);
}
```
