# Generic Instantiation

Caustic implements generics through **monomorphization**: each use of a generic function or struct with specific type arguments produces a fully specialized copy. There is no type erasure or runtime dispatch.

## Declaration Syntax

### Generic Functions

```cst
fn max gen T (a as T, b as T) as T {
    if a > b {
        return a;
    }
    return b;
}
```

### Generic Structs

```cst
struct Pair gen T, U {
    first as T;
    second as U;
}
```

### Generic Impl Blocks

```cst
impl Wrapper gen T {
    fn get(self as *Wrapper gen T) as T {
        return self.value;
    }
}
```

When a generic impl block is parsed, each function inside receives a copy of the generic parameters from the impl header.

## Usage Syntax

Generic type arguments are provided at the call site:

```cst
let is i32 as result = max gen i32 (10, 20);

let is Pair gen i32, i64 as p;
p.first = 42;
p.second = 100;
```

There is no type inference for generic arguments. They must always be stated explicitly.

## Instantiation Process

When the semantic walker encounters a generic usage (e.g., `max gen i32`), it performs the following steps:

### 1. Locate the Template

The compiler searches for the generic template by its base name (`max`). Templates are stored in a linked list of generic declarations, separate from already-instantiated functions.

### 2. Check for Existing Instantiation

The compiler checks whether this specific instantiation already exists by looking for the mangled name (`max_i32`). If found, the existing instantiation is reused.

### 3. Clone the AST

If no existing instantiation is found, the entire AST subtree of the template is deep-cloned. This includes:

- The function node itself (or struct declaration)
- All parameter nodes
- The entire function body (all statements and expressions)
- All child nodes recursively

### 4. Substitute Type Parameters

The cloned AST is traversed, and every reference to a type parameter (`T`, `U`, etc.) is replaced with the concrete type argument (`i32`, `i64`, etc.).

The substitution **must** use global singleton type pointers:

```
// CORRECT:
if param_name == "T":
    node.ty = type_i32    // global singleton

// WRONG:
if param_name == "T":
    node.ty = new_type(TY_I32)   // breaks pointer equality
```

For generic structs used as types (e.g., `Wrapper gen T`), substitution must also process `generic_args` on the struct type reference, substituting each argument and re-mangling the struct name.

### 5. Re-Walk the Cloned AST

The cloned and substituted AST is run through `walk()` for full semantic analysis. This validates all type constraints with concrete types.

**Critical**: The `stack_offset` and `current_scope` global variables must be saved before this walk and restored afterward. Without this, the caller's stack layout is corrupted:

```
saved_offset = stack_offset
saved_scope = current_scope
walk(cloned_fn)
stack_offset = saved_offset
current_scope = saved_scope
```

### 6. Append to AST

The fully analyzed clone is appended to `ast_tail`, making it available for IR generation and code emission.

## Name Mangling

Instantiated generics receive mangled names formed by appending type arguments separated by underscores:

| Template | Arguments | Mangled Name |
|----------|-----------|-------------|
| `max gen T` | `i32` | `max_i32` |
| `max gen T` | `f64` | `max_f64` |
| `Pair gen T, U` | `i32, i64` | `Pair_i32_i64` |
| `Wrapper gen T` | `Point` | `Wrapper_Point` |

This name is used in assembly output and for deduplication.

## Skip Rules

Generic templates (uninstantiated) must be skipped in several passes:

- **resolve_fields**: Skip structs with `generic_param_count > 0`.
- **analyze_bodies (walk)**: Skip functions with `generic_param_count > 0`.
- **gen_ir**: Skip functions with `generic_param_count > 0`.

Only the instantiated copies (with concrete types and mangled names) are processed by these passes.

## Example: Full Pipeline

```cst
fn identity gen T (val as T) as T {
    return val;
}

fn main() as i32 {
    let is i32 as a = identity gen i32 (42);
    let is i64 as b = identity gen i64 (100);
    return a;
}
```

After instantiation, the AST effectively contains:

```cst
fn identity_i32(val as i32) as i32 {
    return val;
}

fn identity_i64(val as i64) as i64 {
    return val;
}

fn main() as i32 {
    let is i32 as a = identity_i32(42);
    let is i64 as b = identity_i64(100);
    return a;
}
```
