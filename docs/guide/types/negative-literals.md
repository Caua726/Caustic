# Negative Literals

Caustic does not have negative literal syntax (no unary minus on numbers). Negative values are expressed using subtraction.

## Local Variables

Negative values work fine in local variable declarations.

```cst
let is i32 as x = 0 - 1;        // -1
let is i64 as y = 0 - 100;      // -100
let is i32 as min = 0 - 128;    // -128
```

## Immutable Globals: The Gotcha

Immutable globals (`with imut`) require a compile-time constant expression. Subtraction like `0 - 1` is not considered constant, so this fails:

```cst
// ERROR: not a constant expression
// let is i32 as NEGATIVE with imut = 0 - 1;
```

### Workaround 1: Use positive sentinel values

```cst
let is i32 as NOT_FOUND with imut = 99;
let is i32 as UNSET with imut = 255;
let is i32 as INVALID with imut = 99999;
```

### Workaround 2: Use mutable globals

```cst
let is i32 as NEGATIVE with mut = 0 - 1;    // works, but mutable
```

## Function Arguments and Returns

Negative values work normally in expressions, function calls, and return statements.

```cst
fn negate(x as i32) as i32 {
    return 0 - x;
}

fn main() as i32 {
    let is i32 as result = negate(42);    // -42
    return 0;
}
```

## Byte Extraction from Negative Numbers

When extracting bytes from negative numbers, avoid division (it truncates toward zero). Use bitwise operations instead.

```cst
let is i64 as val = 0 - 1;

// WRONG: division truncates toward zero for negatives
// let is i64 as high_byte = val / 256;

// CORRECT: bitwise shift and mask
let is i64 as high_byte = (val >> 8) & 255;
```
