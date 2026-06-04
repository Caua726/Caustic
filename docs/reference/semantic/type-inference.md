# Type Inference

Caustic does not perform full type inference. All variable declarations require explicit type annotations. However, the compiler does perform **literal narrowing**, which allows integer and float literals to adapt to their target type without explicit casts.

## Explicit Type Annotations

Every variable must declare its type. There is no `auto` or inferred type:

```cst
let is i32 as x = 10;       // type must be stated
let is *u8 as ptr;           // even for uninitialized variables
let is [64]u8 as buf;        // arrays too
```

Function parameters and return types must also be explicitly annotated:

```cst
fn add(a as i32, b as i32) as i32 {
    return a + b;
}
```

## Literal Narrowing

Integer literals in Caustic are initially typed as `i64` by the lexer. When an integer literal is used in a context that expects a smaller integer type, the compiler automatically narrows the literal's type to match.

### How It Works

During type checking, when the compiler encounters an assignment or argument where:

1. The source is an integer literal with type `i64`
2. The target type is a smaller integer type (`i32`, `i16`, `i8`, `u8`, `u16`, `u32`, `u64`)

The compiler changes the literal's type from `i64` to the target type. This comparison uses **pointer equality** against global singleton type pointers:

```
if node.ty == type_i64 and node is literal:
    node.ty = target_type   // reassign to singleton pointer
```

### Examples

```cst
let is i32 as x = 42;       // 42 starts as i64, narrows to i32
let is u8 as byte = 255;    // 255 starts as i64, narrows to u8
let is i16 as s = 1000;     // 1000 starts as i64, narrows to i16
```

This applies in function call arguments as well:

```cst
fn process(val as i32) as void { }

fn main() as i32 {
    process(100);   // 100 narrows from i64 to i32
    return 0;
}
```

### Why Pointer Equality Matters

The narrowing check uses pointer comparison (`node.ty == type_i32`) rather than structural comparison. The global singleton `type_i32` is a single `Type` object that all i32-typed nodes point to.

If code creates a new `Type` struct with `kind = TY_I32` instead of using the `type_i32` singleton, pointer comparison will fail, and narrowing will not trigger. This is a common source of bugs in generic instantiation, where type substitution must always use the singleton pointers.

```
// CORRECT: use singleton
node.ty = type_i32;

// WRONG: creates a new Type, breaks pointer equality
let is Type as t;
t.kind = TY_I32;
node.ty = &t;
```

## Float Literal Narrowing

Float literals are initially typed as `f64`. When assigned to an `f32` variable, the literal narrows to `f32`:

```cst
let is f64 as x = 3.14;    // stays f64
let is f32 as y = 2.5;     // 2.5 narrows from f64 to f32
```

## Implicit Conversions

Beyond literal narrowing, Caustic allows a small, fixed set of implicit conversions:

- **Same-signedness integer widening** (e.g. `i32` -> `i64`, `u16` -> `u64`).
- **Function pointer <-> `*u8`** (both are 8-byte addresses).

Everything else requires an explicit `cast()`:

- Integer to float (or float to integer)
- Signed to unsigned, or vice versa — even when widening
- Integer to pointer (or pointer to integer)
- Narrowing a non-literal to a smaller integer

```cst
let is i32 as x = 10;
let is i64 as y = x;                // OK: same-sign widening, no cast
let is f64 as z = cast(f64, x);     // explicit cast required (int -> float)
let is u32 as w = cast(u32, x);     // explicit cast required (sign change)
```

> Different pointer types (`*T` -> `*U`) are currently accepted implicitly as well — a known, temporary loosening; see [Type Compatibility › Pointer Compatibility](../type-system/type-compatibility.md#pointer-compatibility).

## Limitations

- No generic type inference: generic type arguments must always be stated explicitly (`func gen i32(args)`, not `func(args)` with inferred `T = i32`).
- No return type inference: functions must declare their return type.
- No structural subtyping: two structs with identical fields but different names are distinct types.
