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

## Passing to Functions

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

## Using else (Default)

The `else` clause matches any variant not covered by a `case`:

```cst
match Color (c) {
    case Red { return 1; }
    else { return 0; }
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
