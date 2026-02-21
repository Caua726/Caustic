# Tagged Unions

Enum variants can carry data, making them tagged unions (sum types).

## Declaration

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

## Construction

Pass data when creating a variant:

```cst
let is Shape as s1 = Shape.Circle(10);
let is Shape as s2 = Shape.Rect(100, 50);
let is Shape as s3 = Shape.None;
```

## Extracting Data with Match

Use `match` to destructure variants and bind their data:

```cst
fn area(s as Shape) as i32 {
    match (s) {
        Shape.Circle(r) => {
            return r * r * 3;  // approximate
        }
        Shape.Rect(w, h) => {
            return w * h;
        }
        Shape.None => {
            return 0;
        }
    }
    return 0;
}
```

The variable names in the match arm (`r`, `w`, `h`) bind to the variant's payload fields.

## Memory Layout

Tagged unions use a fixed layout regardless of the active variant:

- **Tag**: `i32` (4 bytes) at offset 0 -- identifies which variant is active
- **Payload**: starts after the tag, sized to fit the **largest** variant

For the `Shape` example:
- Tag: 4 bytes (Circle=0, Rect=1, None=2)
- Max payload: 8 bytes (Rect has 2 x i32 = 8 bytes)
- Total: 12 bytes

All `Shape` values occupy 12 bytes, even `Shape.None` which uses none of the payload.

## Mixed Types

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

match (v) {
    Value.Int(n) => { /* use n as i64 */ }
    Value.Float(f) => { /* use f as f64 */ }
    Value.Str(s) => { /* use s as *u8 */ }
    Value.Bool(b) => { /* use b as bool */ }
    Value.Nil => { /* no data */ }
}
```

## Complete Example

```cst
enum Token {
    Number as i64;
    Plus;
    Minus;
    End;
}

fn describe(tok as Token) as i32 {
    match (tok) {
        Token.Number(val) => {
            return cast(i32, val);
        }
        Token.Plus => { return 43; }   // '+'
        Token.Minus => { return 45; }  // '-'
        Token.End => { return 0; }
    }
    return 0;
}

fn main() as i32 {
    let is Token as t = Token.Number(7);
    return describe(t);  // exit code 7
}
```
