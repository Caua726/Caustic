# Match

Pattern matching on enum values.

## Syntax

```cst
match EnumType (value) {
    case Variant1 { ... }
    case Variant2 { ... }
    else { ... }
}
```

The enum type name comes after `match`, and the value to match is in parentheses.

## Simple Enum Matching

```cst
enum Color { Red; Green; Blue; }

fn describe(c as Color) as i32 {
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
    return 0;
}
```

## Else Branch

The `else` branch handles any variant not explicitly listed.

```cst
match Color (c) {
    case Red {
        return 1;
    }
    else {
        return 0;
    }
}
```

## Matching Enums with Data

Enum variants can carry payloads. Use destructuring to extract the data (see [Destructuring](destructuring.md)).

```cst
enum Shape {
    Circle as i32;
    Rect as i32, i32;
    None;
}

let is Shape as s = Shape.Circle(5);
match Shape (s) {
    case Circle(r) {
        // r is bound to the payload value
        return r;
    }
    case Rect(w, h) {
        return w + h;
    }
    else {
        return 0;
    }
}
```

## Rules

- The enum type name is required after `match`.
- Braces are required around each case body.
- Case variant names are written without the enum prefix (e.g. `case Red`, not `case Color.Red`).
- The `else` branch is optional but recommended to handle unexpected variants.
