# Type Coercion

Caustic has minimal implicit type coercion. Most conversions require an explicit `cast()`.

## Integer Literal Narrowing

The one implicit coercion: integer literals (which are `i64` by default) automatically narrow to smaller integer types when assigned.

```cst
let is i32 as a = 42;      // i64 literal narrows to i32
let is i16 as b = 1000;    // i64 literal narrows to i16
let is i8 as c = 100;      // i64 literal narrows to i8
let is u8 as d = 255;      // i64 literal narrows to u8
```

This only works for literals, not for variables or expressions.

```cst
let is i64 as big = 42;
// ERROR: cannot implicitly narrow a variable
// let is i32 as x = big;

// Correct: use cast
let is i32 as x = cast(i32, big);
```

## Explicit Cast

The `cast(TargetType, expr)` builtin converts between types.

### Integer widening and narrowing

```cst
let is i32 as small = 100;
let is i64 as wide = cast(i64, small);     // widen: 100
let is i8 as narrow = cast(i8, wide);      // narrow: 100 (truncates if too large)
```

### Signed/unsigned conversion

```cst
let is i32 as signed = 0 - 1;
let is u32 as unsigned = cast(u32, signed);    // bit pattern preserved
```

### Integer/float conversion

```cst
let is i64 as n = 42;
let is f64 as f = cast(f64, n);     // 42.0

let is f64 as pi = 3.14;
let is i64 as truncated = cast(i64, pi);    // 3
```

### Pointer casts

```cst
let is *u8 as raw = mem.galloc(32);
let is *i32 as typed = cast(*i32, raw);

let is u64 as addr = cast(u64, raw);       // pointer to integer
let is *u8 as ptr = cast(*u8, addr);       // integer to pointer
```

## No Implicit Widening

Unlike some languages, Caustic does not implicitly widen integers in expressions.

```cst
let is i32 as a = 10;
let is i64 as b = 20;

// ERROR: type mismatch, i32 vs i64
// let is i64 as c = a + b;

// Correct: cast to matching type
let is i64 as c = cast(i64, a) + b;
```

## No Implicit Int/Float Mixing

```cst
let is f64 as x = 10.0;
let is i64 as y = 5;

// ERROR: cannot add f64 and i64
// let is f64 as z = x + y;

// Correct
let is f64 as z = x + cast(f64, y);
```
