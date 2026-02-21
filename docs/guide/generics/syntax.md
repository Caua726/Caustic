# Generic Syntax

Caustic generics use the `gen` keyword to declare type parameters on functions, structs, and enums. Each type parameter is substituted with a concrete type at the call site, and the compiler generates a specialized version for each unique combination of type arguments (monomorphization).

## Single Type Parameter

```cst
fn identity gen T (val as T) as T {
    return val;
}
```

## Multiple Type Parameters

```cst
fn convert gen T, U (input as T, default as U) as U {
    return default;
}
```

## Where `gen` Can Appear

| Declaration | Syntax |
|-------------|--------|
| Function | `fn name gen T (params) as RetType { ... }` |
| Struct | `struct Name gen T { field as T; }` |
| Enum | `enum Name gen T { Variant as T; }` |
| Impl block | `impl Name gen T { ... }` |

## Instantiation Syntax

Type arguments are provided after `gen` at the usage site:

```cst
// Function call
let is i32 as x = identity gen i32 (42);

// Struct type
let is Pair gen i32, i64 as p;

// Enum variant
let is Option gen i32 as opt = Option gen i32 .Some(10);
```

## Naming Rules

Type parameter names are single uppercase identifiers by convention (`T`, `U`, `V`), but any valid identifier works. They must not collide with existing type names.
