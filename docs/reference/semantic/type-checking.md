# Type Checking

Type checking validates that all operations in the program use compatible types. It is performed during the `walk()` pass of semantic analysis. Every node kind has specific type rules that are enforced.

## Binary Operations

Both operands of a binary expression must have compatible types. The result type depends on the operator and operand types.

### Arithmetic Operators (`+`, `-`, `*`, `/`, `%`)

Both operands must be the same numeric type. The result is that same type.

```cst
let is i32 as a = 10;
let is i32 as b = 20;
let is i32 as c = a + b;    // OK: i32 + i32 = i32

let is f64 as x = 1.5;
let is f64 as y = x * 2.0;  // OK: f64 * f64 = f64
```

Mixing integer and float types without an explicit cast is an error:

```cst
let is i32 as a = 10;
let is f64 as b = 1.5;
// let is f64 as c = a + b;  // ERROR: type mismatch i32 vs f64
```

### Comparison Operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)

Both operands must be the same type (integer, float, char, bool, or pointer). The result type is always `bool`.

```cst
let is i32 as a = 10;
let is bool as result = a > 5;   // OK: i32 > i32 = bool
```

### Logical Operators (`&&`, `||`)

Both operands must be `bool`. The result is `bool`.

```cst
let is bool as a = true;
let is bool as b = false;
let is bool as c = a && b;   // OK
```

### Bitwise Operators (`&`, `|`, `^`, `<<`, `>>`)

Both operands must be the same integer type. The result is that integer type.

```cst
let is u32 as flags = 0xFF;
let is u32 as mask = flags & 0x0F;   // OK: u32 & u32 = u32
```

## Assignment

The right-hand side type must match the left-hand side type. Integer literal narrowing is the one exception (see [Type Inference](type-inference.md)).

```cst
let is i32 as x = 10;    // OK: i64 literal narrows to i32
x = 20;                   // OK: i64 literal narrows to i32
// x = 1.5;               // ERROR: f64 cannot assign to i32
```

For pointer assignments, the pointed-to types must match exactly:

```cst
let is *i32 as p = cast(*i32, addr);
// let is *i64 as q = p;  // ERROR: *i32 is not *i64
```

## Function Calls

Each argument's type must match the corresponding parameter's type. The number of arguments must equal the number of parameters.

```cst
fn add(a as i32, b as i32) as i32 {
    return a + b;
}

fn main() as i32 {
    let is i32 as r = add(1, 2);     // OK
    // let is i32 as r = add(1);      // ERROR: wrong number of arguments
    // let is i32 as r = add(1, 2.0); // ERROR: f64 does not match i32
    return r;
}
```

## Return Statements

The expression in a `return` statement must match the function's declared return type:

```cst
fn foo() as i32 {
    return 42;    // OK: i64 literal narrows to i32
    // return 1.5; // ERROR: f64 does not match i32
}
```

A function declared `as void` must not return a value (bare `return;` is permitted).

## Array Indexing

The index expression must be an integer type. The result type is the array's element type:

```cst
let is [10]i32 as arr;
let is i32 as val = arr[3];   // OK: index is integer, result is i32
```

## Member Access

The left-hand side must be a struct type (or pointer to struct). The named field must exist in that struct:

```cst
struct Point {
    x as i32;
    y as i32;
}

fn main() as i32 {
    let is Point as p;
    p.x = 10;     // OK: Point has field x of type i32
    // p.z = 10;   // ERROR: Point has no field z
    return p.x;
}
```

When accessing through a pointer, automatic dereference is applied:

```cst
fn set_x(p as *Point) as void {
    p.x = 42;   // auto-deref: (*p).x = 42
}
```

## Address-of and Dereference

The address-of operator (`&`) produces a pointer to the operand's type. The dereference operator (`*`) requires a pointer operand and produces the pointed-to type:

```cst
let is i32 as x = 10;
let is *i32 as p = &x;     // &i32 = *i32
let is i32 as y = *p;      // *(*i32) = i32
```

## Cast Expressions

`cast(TargetType, expr)` performs an explicit type conversion. The result type is `TargetType`. No implicit validation of the cast's safety is performed -- this is the programmer's responsibility:

```cst
let is i64 as addr = 0x1000;
let is *u8 as ptr = cast(*u8, addr);   // i64 to *u8
let is i32 as small = cast(i32, addr); // i64 to i32 (truncation)
```

## Sizeof

`sizeof(Type)` returns the size of a type in bytes. The result type is `i64`:

```cst
let is i64 as s = sizeof(i32);   // 4
let is i64 as s2 = sizeof(Point); // sum of field sizes (packed)
```
