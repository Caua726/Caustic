# Monomorphization

Caustic generics use monomorphization: each unique set of type arguments produces a separate, fully specialized copy of the generic code. There is no runtime dispatch or type erasure.

## How It Works

1. **Template storage**: The parser creates an AST node for the generic function/struct/enum and marks it as a template. Templates are skipped during IR generation.

2. **Instantiation**: When the semantic analyzer encounters a usage like `max gen i32 (...)`, it checks if `max_i32` already exists. If not, it clones the template AST.

3. **Type substitution**: All occurrences of the type parameter (`T`) in the cloned AST are replaced with the concrete type (`i32`), using global singleton type pointers for correct pointer equality.

4. **AST append**: The cloned, substituted AST is appended to the program's AST tail and analyzed normally.

5. **Code generation**: Each instantiation goes through IR and codegen as a regular function/struct.

## Name Mangling

The mangled name is constructed by joining the base name and concrete type names with underscores:

| Generic Usage | Mangled Name |
|---------------|-------------|
| `max gen i32` | `max_i32` |
| `max gen i64` | `max_i64` |
| `Pair gen i32, i64` | `Pair_i32_i64` |
| `Node gen *u8` | `Node_pu8` |

## Deduplication

If `max gen i32` is called in multiple places, only one `max_i32` is generated. The semantic analyzer checks for existing instantiations before cloning.

## Trade-offs

- **Pro**: Zero runtime overhead. Each instantiation is optimized for its concrete types.
- **Pro**: No boxing or vtable indirection.
- **Con**: Code size grows with each unique instantiation.
- **Con**: Type errors appear only when a specific instantiation fails, not at template definition time.

## Internal Detail: stack_offset

During generic instantiation, the compiler saves and restores the global `stack_offset` variable around the `walk()` call for the cloned function. Without this, the inner function's stack layout would corrupt the caller's variable allocation.
