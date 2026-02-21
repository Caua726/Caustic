# Simple Enums

Simple enums define a set of named integer constants.

## Declaration

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

## Creating an Enum Value

Use `EnumName.Variant` syntax:

```cst
let is Color as c = Color.Blue;
```

## Comparison

Compare enum values with `==` and `!=`:

```cst
if (c == Color.Red) {
    // handle red
}

if (c != Color.Green) {
    // not green
}
```

## Storage

Simple enums are stored as `i32` (4 bytes). The value is the variant's index.

## Using with Match

```cst
match (c) {
    Color.Red => {
        syscall(1, 1, "red\n", 4);
    }
    Color.Green => {
        syscall(1, 1, "green\n", 6);
    }
    Color.Blue => {
        syscall(1, 1, "blue\n", 5);
    }
}
```

## Passing to Functions

```cst
fn color_code(c as Color) as i32 {
    match (c) {
        Color.Red => { return 1; }
        Color.Green => { return 2; }
        Color.Blue => { return 3; }
    }
    return 0;
}
```

## Complete Example

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
