# Type Compatibility

Type compatibility determines which types can be used interchangeably and which require explicit conversion. Caustic has a strict type system with very few implicit conversions.

## Type Identity: Pointer Equality

Two types are the same type if and only if they are the **same pointer**. For primitive types, this means both must refer to the same global singleton object (`type_i32`, `type_i64`, etc.). Creating a new `Type` struct with `kind = TY_I32` instead of using the `type_i32` singleton breaks all pointer-equality checks:

```
// These are equal (same singleton pointer):
type_a = type_i32
type_b = type_i32
type_a == type_b   // true

// These are NOT equal (different objects, even if structurally identical):
type_a = type_i32
type_b = new Type { kind: TY_I32 }
type_a == type_b   // false
```

This is an internal compiler invariant. All code that constructs or substitutes types (including generics) must use the global singletons for primitives.

## Exact Match

Within the same type kind, compatibility requires the same size. No implicit conversions occur between different primitive types, even when one could hold all values of the other:

```cst
let is i32 as a = 10;
let is i64 as b = cast(i64, a);   // explicit cast required
let is u32 as c = cast(u32, a);   // explicit cast required
```

## Integer Literal Narrowing

The one implicit conversion Caustic allows is **literal narrowing**. Integer literals are initially typed as `i64`. When a literal appears in a context where a narrower integer type is expected, the compiler silently adjusts the literal's type -- no cast is needed:

```cst
let is i32 as x  = 42;       // i64 literal narrows to i32
let is u8  as b  = 255;      // i64 literal narrows to u8
let is i16 as s  = 1000;     // i64 literal narrows to i16
```

This narrowing applies only to **literals**, not to variables or computed expressions:

```cst
let is i64 as big = 42;
// let is i32 as x = big;   // ERROR: i64 variable does not narrow implicitly
let is i32 as x = cast(i32, big);  // OK
```

Narrowing also applies to function call arguments:

```cst
fn process(val as i32) as void { }
process(100);   // 100 (i64 literal) narrows to i32 automatically
```

## Float Literal Narrowing

Float literals are initially typed as `f64`. They narrow to `f32` when the destination type is `f32`:

```cst
let is f32 as x = 2.5;   // f64 literal narrows to f32
```

No implicit narrowing occurs for float variables (`cast` is required for `f64` to `f32` when using variables).

## No Implicit Integer Widening

There is no implicit widening in Caustic. An `i32` variable cannot be assigned to an `i64` variable without an explicit cast, even though every `i32` value fits in `i64`:

```cst
let is i32 as small = 10;
// let is i64 as big = small;    // ERROR
let is i64 as big = cast(i64, small);  // OK
```

## No Implicit Float/Integer Conversion

Assigning an integer to a float variable (or vice versa) always requires an explicit cast:

```cst
let is f64 as f = cast(f64, some_i32);
let is i32 as i = cast(i32, some_f64);
```

## Pointer Compatibility

A pointer type `*T` is compatible only with another `*T` of the same base type. Different pointer types require an explicit cast:

```cst
let is *i32 as a = &x;
// let is *u8  as b = a;      // ERROR: *i32 is not *u8
let is *u8  as b = cast(*u8, a);   // OK
```

Multi-level pointers follow the same rule: `**i32` is not compatible with `**u8`.

There is no `*void` in Caustic. Use `*u8` as a generic raw pointer type, which is the convention used throughout the standard library.

## Array Type Compatibility

Two array types are compatible only when they have both the same element type **and** the same length:

```cst
let is [10]i32 as a;
let is [10]i32 as b;   // same type as a
// let is [5]i32  as c; // ERROR: different length
// let is [10]i64 as d; // ERROR: different element type
```

Arrays cannot be assigned to each other as whole values; they must be copied element by element.

## Struct Type Compatibility: Nominal Typing

Struct types use **nominal typing**: two structs are compatible only if they have the same name. Two structs with identical fields but different names are distinct types:

```cst
struct Vec2  { x as i32; y as i32; }
struct Point { x as i32; y as i32; }

let is Vec2 as v;
// let is Point as p = v;   // ERROR: Vec2 is not Point
```

Structs from different modules with the same name are also distinct, since they are different type objects.

## Enum Type Compatibility

Like structs, enum types are compared by name. Two enum types are never compatible even if they define the same variants:

```cst
enum Color { Red; Green; Blue; }
enum Light { Red; Green; Blue; }

let is Color as c = Color.Red;
// let is Light as l = c;   // ERROR
```

## Generic Instantiation Compatibility

Each generic instantiation produces a distinct named type. `Pair gen i32, i64` and `Pair gen i64, i32` are different types (`Pair_i32_i64` vs `Pair_i64_i32`):

```cst
let is Pair gen i32, i64 as a;
// a is type Pair_i32_i64; incompatible with Pair_i64_i32
```

## The cast() Expression

`cast(TargetType, expr)` is the universal escape hatch. It performs no runtime validation:

```cst
// Integer ↔ integer (any width/sign)
let is i64 as x = cast(i64, some_i32);

// Integer ↔ float
let is f64 as f = cast(f64, some_i32);
let is i32 as i = cast(i32, some_f64);

// Pointer ↔ pointer
let is *u8 as raw = cast(*u8, some_ptr);

// Integer ↔ pointer
let is *u8 as addr = cast(*u8, 0x1000);
let is i64 as val  = cast(i64, some_ptr);
```

Struct-to-struct casts are not supported through `cast`. Reinterpret a struct via a pointer cast instead.

## Compatibility Summary

| Conversion                  | Implicit | Literal only | Requires cast |
|-----------------------------|----------|-------------|---------------|
| Same type                   | Yes      | -           | -             |
| i64 literal -> narrower int | -        | Yes         | -             |
| f64 literal -> f32          | -        | Yes         | -             |
| i32 variable -> i64         | No       | -           | Yes           |
| int -> float (any)          | No       | -           | Yes           |
| float -> int (any)          | No       | -           | Yes           |
| *T -> *U (different T, U)   | No       | -           | Yes           |
| StructA -> StructB          | No       | -           | Not supported |
