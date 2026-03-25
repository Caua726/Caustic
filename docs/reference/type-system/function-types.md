# Function Types (TY_FN)

Function types represent the signature of a function pointer created by `fn_ptr()`. They enable compile-time type checking of indirect calls via `call()`.

## Type Kind

`TY_FN = 18` — the 19th and last type kind.

## Type Structure

```cst
// Fields used by TY_FN in the Type struct:
kind        = TY_FN
size        = 8              // always pointer-sized
base        = *Type          // return type (or null for void)
fields      = *u8            // packed array of parameter types (*Type, 8 bytes each)
field_count = i32            // number of parameters
```

## Creation

`fn_ptr()` creates a TY_FN type from a function's semantic signature:

```cst
fn compare(a as i64, b as i64) as i32 { ... }

let is *u8 as cmp = fn_ptr(compare);
// cmp has type TY_FN with:
//   base = type_i32 (return type)
//   fields = [type_i64, type_i64] (parameter types)
//   field_count = 2
```

## Type Checking

When `call(fn_ptr, args)` is used and the fn_ptr has type TY_FN:

1. **Argument count**: must match `field_count`
2. **Argument types**: each argument's type is checked against the corresponding parameter type
3. **Return type**: the `call()` expression has the type `base` (the function's return type)

If the fn_ptr is an untyped `*u8` (not created by `fn_ptr()`), no type checking occurs and the return type defaults to `i64`.

## Examples

```cst
// Typed function pointer — full type checking
let is *u8 as fp = fn_ptr(my_func);
let is i32 as r = call(fp, 10, 20);  // checked: arg count, types, return type

// Untyped pointer — no checking, returns i64
let is *u8 as raw = cast(*u8, some_address);
let is i64 as r = call(raw, 10, 20);  // unchecked
```

## Interaction with Generics

Generic function pointers resolve to the monomorphized instantiation:

```cst
fn max gen T (a as T, b as T) as T { ... }

let is *u8 as fp = fn_ptr(max gen i32);
// TY_FN with base=type_i32, fields=[type_i32, type_i32], field_count=2
```
