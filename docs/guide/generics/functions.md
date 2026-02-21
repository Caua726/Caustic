# Generic Functions

Generic functions declare type parameters after the function name with `gen`. The compiler generates a separate concrete function for each unique set of type arguments used across the program.

## Declaration

```cst
fn max gen T (a as T, b as T) as T {
    if a > b {
        return a;
    }
    return b;
}
```

## Calling

Specify the concrete type after `gen`:

```cst
let is i32 as m1 = max gen i32 (10, 20);
let is i64 as m2 = max gen i64 (100, 200);
```

The first call generates `max_i32`, the second generates `max_i64`. Both are independent functions with fully resolved types.

## Multiple Type Parameters

```cst
fn make_pair gen T, U (a as T, b as U) as Pair gen T, U {
    let is Pair gen T, U as p;
    p.first = a;
    p.second = b;
    return p;
}

let is Pair gen i32, i64 as p = make_pair gen i32, i64 (1, 2);
```

## Function Pointers to Generic Functions

Use `fn_ptr` with the concrete instantiation:

```cst
let is *u8 as cb = fn_ptr(max gen i32);
```

This points to the generated `max_i32` function.

## How It Works Internally

1. The parser stores the generic function as a template (not compiled).
2. When the semantic analyzer encounters `max gen i32 (...)`, it clones the template AST.
3. All occurrences of `T` in the clone are replaced with `i32`.
4. The clone is named `max_i32` and appended to the AST for IR generation.
5. Subsequent calls to `max gen i32` reuse the existing instantiation.
