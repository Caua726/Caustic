# Generics

## Syntax

Caustic generics use the `gen` keyword to declare type parameters on functions, structs, and enums. Each type parameter is substituted with a concrete type at the call site, and the compiler generates a specialized version for each unique combination of type arguments (monomorphization).

### Single Type Parameter

```cst
fn identity gen T (val as T) as T {
    return val;
}
```

### Multiple Type Parameters

```cst
fn convert gen T, U (input as T, default as U) as U {
    return default;
}
```

### Where `gen` Can Appear

| Declaration | Syntax |
|-------------|--------|
| Function | `fn name gen T (params) as RetType { ... }` |
| Struct | `struct Name gen T { field as T; }` |
| Enum | `enum Name gen T { Variant as T; }` |
| Impl block | `impl Name gen T { ... }` |

### Instantiation Syntax

Type arguments are provided after `gen` at the usage site:

```cst
// Function call
let is i32 as x = identity gen i32 (42);

// Struct type
let is Pair gen i32, i64 as p;

// Enum variant
let is Option gen i32 as opt = Option gen i32 .Some(10);
```

### Naming Rules

Type parameter names are single uppercase identifiers by convention (`T`, `U`, `V`), but any valid identifier works. They must not collide with existing type names.

## Generic Functions

Generic functions declare type parameters after the function name with `gen`. The compiler generates a separate concrete function for each unique set of type arguments used across the program.

### Declaration

```cst
fn max gen T (a as T, b as T) as T {
    if a > b {
        return a;
    }
    return b;
}
```

### Calling

Specify the concrete type after `gen`:

```cst
let is i32 as m1 = max gen i32 (10, 20);
let is i64 as m2 = max gen i64 (100, 200);
```

The first call generates `max_i32`, the second generates `max_i64`. Both are independent functions with fully resolved types.

### Multiple Type Parameters

```cst
fn make_pair gen T, U (a as T, b as U) as Pair gen T, U {
    let is Pair gen T, U as p;
    p.first = a;
    p.second = b;
    return p;
}

let is Pair gen i32, i64 as p = make_pair gen i32, i64 (1, 2);
```

### Function Pointers to Generic Functions

Use `fn_ptr` with the concrete instantiation:

```cst
let is *u8 as cb = fn_ptr(max gen i32);
```

This points to the generated `max_i32` function.

### How It Works Internally

1. The parser stores the generic function as a template (not compiled).
2. When the semantic analyzer encounters `max gen i32 (...)`, it clones the template AST.
3. All occurrences of `T` in the clone are replaced with `i32`.
4. The clone is named `max_i32` and appended to the AST for IR generation.
5. Subsequent calls to `max gen i32` reuse the existing instantiation.

## Generic Structs

Generic structs declare type parameters after the struct name. Each unique combination of type arguments generates a distinct concrete struct with mangled name.

### Declaration

```cst
struct Pair gen T, U {
    first as T;
    second as U;
}
```

### Usage

Specify concrete types at the usage site:

```cst
let is Pair gen i32, i64 as p;
p.first = 10;
p.second = 200;
```

This generates a struct named `Pair_i32_i64` with fields `first as i32` and `second as i64`.

### Nested Generic Structs

A generic struct can contain another generic struct as a field, but you must specify concrete types (not type parameters from an outer context, unless inside a generic function/impl):

```cst
struct Wrapper gen T {
    inner as Pair gen T, T;
    count as i32;
}

let is Wrapper gen i64 as w;
// w.inner is Pair_i64_i64
```

### Pointer Fields

```cst
struct Node gen T {
    value as T;
    next as *Node gen T;
}
```

This generates `Node_i32` with `next as *Node_i32` when instantiated with `i32`.

### Sizeof

The size of a generic struct depends on its concrete types. Caustic structs are packed (no padding), so:

```cst
// Pair gen i32, i64 -> Pair_i32_i64
// sizeof = 4 + 8 = 12 bytes (packed, no alignment padding)
```

### Gotcha: Cross-Module Generic Structs

Generic structs defined in another module work through imports, but avoid using them directly as struct field types across modules. Use pointers (`*u8`) with accessor functions if needed.

## Generic Enums

Generic enums work like generic structs: each unique type argument combination generates a concrete enum with mangled name.

### Declaration

```cst
enum Option gen T {
    Some as T;
    None;
}

enum Result gen T, E {
    Ok as T;
    Err as E;
}
```

### Creating Values

```cst
let is Option gen i32 as some_val = Option gen i32 .Some(42);
let is Option gen i32 as none_val = Option gen i32 .None();

let is Result gen i32, i32 as ok = Result gen i32, i32 .Ok(100);
let is Result gen i32, i32 as err = Result gen i32, i32 .Err(1);
```

### Matching

```cst
match Option gen i32 (some_val) {
    case Some(val) {
        // val is i32
        syscall(60, val);
    }
    case None {
        syscall(60, 1);
    }
}
```

### Standard Library Types

`std/types.cst` provides `Option` and `Result`:

```cst
use "std/types.cst" as types;

fn find(arr as *i32, len as i32, target as i32) as Option gen i32 {
    let is i32 as i = 0;
    while i < len {
        if arr[i] == target {
            return Option gen i32 .Some(i);
        }
        i = i + 1;
    }
    return Option gen i32 .None();
}
```

### Name Mangling

| Usage | Generated Name |
|-------|---------------|
| `Option gen i32` | `Option_i32` |
| `Result gen i32, i64` | `Result_i32_i64` |
| `Option gen *u8` | `Option_pu8` (pointer prefix) |

## Monomorphization

Caustic generics use monomorphization: each unique set of type arguments produces a separate, fully specialized copy of the generic code. There is no runtime dispatch or type erasure.

### How It Works

1. **Template storage**: The parser creates an AST node for the generic function/struct/enum and marks it as a template. Templates are skipped during IR generation.

2. **Instantiation**: When the semantic analyzer encounters a usage like `max gen i32 (...)`, it checks if `max_i32` already exists. If not, it clones the template AST.

3. **Type substitution**: All occurrences of the type parameter (`T`) in the cloned AST are replaced with the concrete type (`i32`), using global singleton type pointers for correct pointer equality.

4. **AST append**: The cloned, substituted AST is appended to the program's AST tail and analyzed normally.

5. **Code generation**: Each instantiation goes through IR and codegen as a regular function/struct.

### Name Mangling

The mangled name is constructed by joining the base name and concrete type names with underscores:

| Generic Usage | Mangled Name |
|---------------|-------------|
| `max gen i32` | `max_i32` |
| `max gen i64` | `max_i64` |
| `Pair gen i32, i64` | `Pair_i32_i64` |
| `Node gen *u8` | `Node_pu8` |

### Deduplication

If `max gen i32` is called in multiple places, only one `max_i32` is generated. The semantic analyzer checks for existing instantiations before cloning.

### Trade-offs

- **Pro**: Zero runtime overhead. Each instantiation is optimized for its concrete types.
- **Pro**: No boxing or vtable indirection.
- **Con**: Code size grows with each unique instantiation.
- **Con**: Type errors appear only when a specific instantiation fails, not at template definition time.

### Internal Detail: stack_offset

During generic instantiation, the compiler saves and restores the global `stack_offset` variable around the `walk()` call for the cloned function. Without this, the inner function's stack layout would corrupt the caller's variable allocation.

## Constraints

Caustic generics have no trait system or type constraints. Any type can be used as a generic argument, and errors are caught only when the instantiated code fails type checking.

### No Type Constraints

Unlike Rust or Haskell, there is no way to restrict which types a generic parameter accepts:

```cst
// This compiles as a template, but will fail if T doesn't support ">"
fn max gen T (a as T, b as T) as T {
    if a > b {
        return a;
    }
    return b;
}

// Works: i32 supports >
let is i32 as m = max gen i32 (1, 2);

// Fails at instantiation: structs don't support >
// let is Point as p = max gen Point (p1, p2);  // ERROR
```

### No Specialization

You cannot provide a specialized implementation for a specific type. Every instantiation uses the same template.

### No Arithmetic on Generic Types

The compiler must know concrete sizes for arithmetic and memory operations. You cannot write:

```cst
// This won't work -- T has unknown size at template time
fn zero gen T () as T {
    return 0;  // what is "0" for an arbitrary T?
}
```

### No Default Type Parameters

All type parameters must be explicitly provided at each usage site:

```cst
// Must write:
let is Pair gen i32, i32 as p;

// Cannot write:
// let is Pair gen i32 as p;  // ERROR: missing second type argument
```

### No Type Inference

The compiler does not infer generic type arguments from the values passed:

```cst
// Must write:
let is i32 as m = max gen i32 (10, 20);

// Cannot write:
// let is i32 as m = max(10, 20);  // ERROR: missing gen clause
```

### Error Reporting

Because there are no constraints, type errors reference the instantiated (mangled) function name, not the template. If `max gen Point` fails, the error will mention `max_Point`, not the generic definition.
