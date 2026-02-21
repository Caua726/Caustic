# Struct Declaration

Structs group related data into a single type with named fields.

## Syntax

```cst
struct Point {
    x as i32;
    y as i32;
}
```

Fields are separated by semicolons. Each field has a name and a type.

## Creating a Struct

Declare a variable of the struct type, then assign each field:

```cst
let is Point as p;
p.x = 10;
p.y = 20;
```

There is no struct literal syntax -- you assign fields individually after declaration.

## Multiple Fields

Structs can have any number of fields with any types:

```cst
struct Person {
    age as i32;
    height as i64;
    active as bool;
    name as *u8;
}
```

## Packed Layout

Caustic structs are **packed** with no alignment padding. The size of a struct is the sum of its field sizes. See [packed-layout.md](packed-layout.md) for details.

## Passing to Functions

Structs can be passed by value or by pointer:

```cst
fn print_point(p as Point) as void {
    // p is a copy
}

fn move_point(p as *Point, dx as i32) as void {
    p.x = p.x + dx;
}
```

## Complete Example

```cst
struct Rect {
    x as i32;
    y as i32;
    w as i32;
    h as i32;
}

fn area(r as Rect) as i32 {
    return r.w * r.h;
}

fn main() as i32 {
    let is Rect as r;
    r.x = 0;
    r.y = 0;
    r.w = 100;
    r.h = 50;
    return area(r); // 5000 (truncated to exit code 0-255)
}
```
