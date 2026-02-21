# Destructuring

Extract data from enum variant payloads inside `match` expressions.

## Syntax

```cst
match EnumType (value) {
    case VariantName(var1, var2) {
        // var1 and var2 are bound to the variant's payload fields
    }
}
```

Variables in parentheses after the variant name are bound to the payload values in order.

## Example

```cst
enum Shape {
    Circle as i32;
    Rect as i32, i32;
    None;
}

fn area(s as Shape) as i32 {
    match Shape (s) {
        case Circle(r) {
            return r * r * 3;
        }
        case Rect(w, h) {
            return w * h;
        }
        else {
            return 0;
        }
    }
    return 0;
}

fn main() as i32 {
    let is Shape as s = Shape.Rect(10, 20);
    return area(s);  // returns 200
}
```

## Different Payload Sizes

Each variant can have a different number of fields. Variants without data need no parentheses.

```cst
enum Result {
    Ok as i64;
    Pair as i64, i64;
    Err;
}

match Result (val) {
    case Ok(n) {
        // single field
        return n;
    }
    case Pair(a, b) {
        // two fields
        return a + b;
    }
    case Err {
        // no payload, no parentheses
        return 0;
    }
}
```

## Rules

- The number of variables must match the number of fields in the variant.
- Destructured variables are scoped to the case block -- they do not exist outside it.
- Variants without data are matched without parentheses.
- Variable names are arbitrary and independent of the field type names in the enum definition.
