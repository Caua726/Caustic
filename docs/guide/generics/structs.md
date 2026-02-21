# Generic Structs

Generic structs declare type parameters after the struct name. Each unique combination of type arguments generates a distinct concrete struct with mangled name.

## Declaration

```cst
struct Pair gen T, U {
    first as T;
    second as U;
}
```

## Usage

Specify concrete types at the usage site:

```cst
let is Pair gen i32, i64 as p;
p.first = 10;
p.second = 200;
```

This generates a struct named `Pair_i32_i64` with fields `first as i32` and `second as i64`.

## Nested Generic Structs

A generic struct can contain another generic struct as a field, but you must specify concrete types (not type parameters from an outer context, unless inside a generic function/impl):

```cst
struct Wrapper gen T {
    inner as Pair gen T, T;
    count as i32;
}

let is Wrapper gen i64 as w;
// w.inner is Pair_i64_i64
```

## Pointer Fields

```cst
struct Node gen T {
    value as T;
    next as *Node gen T;
}
```

This generates `Node_i32` with `next as *Node_i32` when instantiated with `i32`.

## Sizeof

The size of a generic struct depends on its concrete types. Caustic structs are packed (no padding), so:

```cst
// Pair gen i32, i64 → Pair_i32_i64
// sizeof = 4 + 8 = 12 bytes (packed, no alignment padding)
```

## Gotcha: Cross-Module Generic Structs

Generic structs defined in another module work through imports, but avoid using them directly as struct field types across modules. Use pointers (`*u8`) with accessor functions if needed.
