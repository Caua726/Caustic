# Generic Constraints and Limitations

Caustic generics have no trait system or type constraints. Any type can be used as a generic argument, and errors are caught only when the instantiated code fails type checking.

## No Type Constraints

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

## No Specialization

You cannot provide a specialized implementation for a specific type. Every instantiation uses the same template.

## No Arithmetic on Generic Types

The compiler must know concrete sizes for arithmetic and memory operations. You cannot write:

```cst
// This won't work — T has unknown size at template time
fn zero gen T () as T {
    return 0;  // what is "0" for an arbitrary T?
}
```

## No Default Type Parameters

All type parameters must be explicitly provided at each usage site:

```cst
// Must write:
let is Pair gen i32, i32 as p;

// Cannot write:
// let is Pair gen i32 as p;  // ERROR: missing second type argument
```

## No Type Inference

The compiler does not infer generic type arguments from the values passed:

```cst
// Must write:
let is i32 as m = max gen i32 (10, 20);

// Cannot write:
// let is i32 as m = max(10, 20);  // ERROR: missing gen clause
```

## Error Reporting

Because there are no constraints, type errors reference the instantiated (mangled) function name, not the template. If `max gen Point` fails, the error will mention `max_Point`, not the generic definition.
