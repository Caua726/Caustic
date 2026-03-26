# Enums

## Simple Enums

Simple enums define a set of named integer constants.

### Declaration

```cst
enum Color {
    Red;
    Green;
    Blue;
}
```

Variants are assigned integer values starting from 0:
- `Red` = 0
- `Green` = 1
- `Blue` = 2

### Creating an Enum Value

Use `EnumName.Variant` syntax:

```cst
let is Color as c = Color.Blue;
```

### Comparison

Compare enum values with `==` and `!=`:

```cst
if (c == Color.Red) {
    // handle red
}

if (c != Color.Green) {
    // not green
}
```

### Storage

Simple enums are stored as `i32` (4 bytes). The value is the variant's index.

### Passing to Functions

```cst
fn color_code(c as Color) as i32 {
    match Color (c) {
        case Red { return 1; }
        case Green { return 2; }
        case Blue { return 3; }
    }
    return 0;
}
```

### Complete Example

```cst
enum Direction {
    Up;
    Down;
    Left;
    Right;
}

fn is_vertical(d as Direction) as bool {
    if (d == Direction.Up) { return true; }
    if (d == Direction.Down) { return true; }
    return false;
}

fn main() as i32 {
    let is Direction as d = Direction.Up;
    if (is_vertical(d)) {
        return 1;
    }
    return 0;
}
```

## Tagged Unions

Enum variants can carry data, making them tagged unions (sum types).

### Declaration

Each variant can hold zero or more fields:

```cst
enum Shape {
    Circle as i32;
    Rect as i32, i32;
    None;
}
```

- `Circle` carries one `i32` (radius)
- `Rect` carries two `i32` values (width, height)
- `None` carries no data

### Mixed Types

Variants can carry different types:

```cst
enum Value {
    Int as i64;
    Float as f64;
    Str as *u8;
    Bool as bool;
    Nil;
}

let is Value as v = Value.Int(42);

match Value (v) {
    case Int(n) { /* use n as i64 */ }
    case Float(f) { /* use f as f64 */ }
    case Str(s) { /* use s as *u8 */ }
    case Bool(b) { /* use b as bool */ }
    case Nil { /* no data */ }
}
```

### Complete Example

```cst
enum Token {
    Number as i64;
    Plus;
    Minus;
    End;
}

fn describe(tok as Token) as i32 {
    match Token (tok) {
        case Number(val) {
            return cast(i32, val);
        }
        case Plus { return 43; }   // '+'
        case Minus { return 45; }  // '-'
        case End { return 0; }
    }
    return 0;
}

fn main() as i32 {
    let is Token as t = Token.Number(7);
    return describe(t);  // exit code 7
}
```

## Construction

### Simple Enum (No Data)

Use `EnumName.Variant` with no parentheses:

```cst
enum Color { Red; Green; Blue; }

let is Color as c = Color.Red;
```

### With Data

Pass arguments in parentheses:

```cst
enum Result {
    Ok as i32;
    Err as *u8;
}

let is Result as r1 = Result.Ok(42);
let is Result as r2 = Result.Err("something failed");
```

Multiple fields are comma-separated:

```cst
enum Msg {
    Move as i32, i32;
    Resize as i32, i32, i32, i32;
    Quit;
}

let is Msg as m = Msg.Move(10, 20);
let is Msg as r = Msg.Resize(0, 0, 800, 600);
let is Msg as q = Msg.Quit;
```

### Assigning to Existing Variables

```cst
let is Color as c = Color.Red;
c = Color.Blue;  // reassign
```

### In Function Arguments

Construct directly in a function call:

```cst
fn process(r as Result) as i32 {
    match Result (r) {
        case Ok(val) { return val; }
        case Err(msg) { return 1; }
    }
    return 0;
}

let is i32 as code = process(Result.Ok(0));
```

### Return from Functions

```cst
fn divide(a as i32, b as i32) as Result {
    if (b == 0) {
        return Result.Err("division by zero");
    }
    return Result.Ok(a / b);
}
```

## Pattern Matching

The `match` statement requires the **enum type name** followed by the expression in parentheses. Each arm uses `case VariantName`. An optional `else` clause handles any unmatched variants.

```cst
match Color (c) {
    case Red {
        syscall(1, 1, "red\n", 4);
    }
    case Green {
        syscall(1, 1, "green\n", 6);
    }
    case Blue {
        syscall(1, 1, "blue\n", 5);
    }
}
```

## Destructuring

Use `match` with the **enum type name** to destructure variants and bind their data. Each arm uses `case VariantName` or `case VariantName(bindings)`:

```cst
fn area(s as Shape) as i32 {
    match Shape (s) {
        case Circle(r) {
            return r * r * 3;  // approximate
        }
        case Rect(w, h) {
            return w * h;
        }
        case None {
            return 0;
        }
    }
    return 0;
}
```

The variable names in the match arm (`r`, `w`, `h`) bind to the variant's payload fields.

## Else (Default)

The `else` clause matches any variant not covered by a `case`:

```cst
match Color (c) {
    case Red { return 1; }
    else { return 0; }
}
```

```cst
match Shape (s) {
    case Rect(w, h) { return w * h; }
    else { return 0; }
}
```

## Generic Enums

Generic enums require type parameters at both declaration and construction:

```cst
enum Option gen T {
    Some as T;
    None;
}

let is Option gen i32 as x = Option gen i32 .Some(42);
let is Option gen i32 as y = Option gen i32 .None;
```

Note the space before `.Some` -- this is required syntax when using generics.

```cst
enum Either gen L, R {
    Left as L;
    Right as R;
}

let is Either gen i32, *u8 as e = Either gen i32, *u8 .Left(10);
```

## Memory Layout

### Simple Enum (No Data)

A simple enum is just a tag, stored as `i32` (4 bytes):

```cst
enum Color { Red; Green; Blue; }
// sizeof(Color) = 4
// Red=0, Green=1, Blue=2
```

### Tagged Union Layout

Enums with data use a fixed layout:

```
+--------+------------------+
|  tag   |     payload      |
| (i32)  | (max variant sz) |
+--------+------------------+
  4 bytes    N bytes
```

- **Tag** (offset 0): `i32` identifying the active variant (0, 1, 2, ...)
- **Payload** (offset after tag): sized to the **largest** variant's data

The total size is `4 + max_payload_size`, regardless of which variant is active.

### Worked Example

```cst
enum Shape {
    Circle as i32;           // tag=0, payload: 4 bytes
    Rect as i32, i32;        // tag=1, payload: 8 bytes
    Triangle as i32, i32, i32; // tag=2, payload: 12 bytes
    None;                    // tag=3, payload: 0 bytes
}
```

Layout breakdown:
- Tag: 4 bytes
- Max payload: 12 bytes (Triangle has 3 x i32)
- **Total sizeof(Shape) = 16 bytes**

Memory for `Shape.Circle(5)`:
```
offset 0:  [00 00 00 00]   tag = 0 (Circle)
offset 4:  [05 00 00 00]   radius = 5
offset 8:  [?? ?? ?? ??]   unused
offset 12: [?? ?? ?? ??]   unused
```

Memory for `Shape.Rect(10, 20)`:
```
offset 0:  [01 00 00 00]   tag = 1 (Rect)
offset 4:  [0A 00 00 00]   width = 10
offset 8:  [14 00 00 00]   height = 20
offset 12: [?? ?? ?? ??]   unused
```

### Mixed-Type Payloads

When variants have different-sized types, the payload is still sized to the largest:

```cst
enum Value {
    Byte as u8;     // tag=0, payload: 1 byte
    Int as i64;     // tag=1, payload: 8 bytes
    Pair as i32, i32; // tag=2, payload: 8 bytes
}
// Max payload = 8, sizeof(Value) = 12
```

### Payload Offset

The payload starts immediately after the tag (4 bytes). Since Caustic structs are packed, there is no alignment padding between the tag and payload:

```
tag at offset 0    (4 bytes)
payload at offset 4 (max_payload bytes)
```

### Generic Enum Layout

Generic enums follow the same rules after monomorphization:

```cst
enum Option gen T {
    Some as T;
    None;
}

// Option gen i32:  tag(4) + i32(4) = 8 bytes
// Option gen i64:  tag(4) + i64(8) = 12 bytes
// Option gen *u8:  tag(4) + *u8(8) = 12 bytes
```

### Sizeof

Use `sizeof(EnumName)` to get the total size:

```cst
let is *u8 as raw = mem.galloc(sizeof(Shape));
let is *Shape as s = cast(*Shape, raw);
```
