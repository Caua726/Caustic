# Logical Operators

## Operators

| Operator | Description | Example        |
|----------|------------|----------------|
| `&&`     | Logical AND | `a && b`       |
| `\|\|`   | Logical OR  | `a \|\| b`     |
| `!`      | Logical NOT | `!a`           |

## Logical AND

Returns true (non-zero) only if both operands are true. Uses short-circuit evaluation: if the left operand is false, the right operand is not evaluated.

```cst
fn in_range(x as i32, lo as i32, hi as i32) as i32 {
    if (x >= lo && x <= hi) {
        return 1;
    }
    return 0;
}
```

## Logical OR

Returns true if either operand is true. Uses short-circuit evaluation: if the left operand is true, the right operand is not evaluated.

```cst
fn is_whitespace(c as char) as i32 {
    if (c == ' ' || c == '\n' || c == '\t') {
        return 1;
    }
    return 0;
}
```

## Logical NOT

Inverts a boolean value. Non-zero becomes 0, zero becomes 1.

```cst
let is i32 as found = 0;
if (!found) {
    // executes when found is 0
}
```

## Combining Conditions

Parentheses can clarify complex conditions.

```cst
if ((a > 0 && b > 0) || force) {
    // runs if both a,b positive OR force is set
}
```

## Short-Circuit Safety

Short-circuit evaluation makes it safe to guard dereferences:

```cst
if (ptr != cast(*u8, 0) && *ptr == 10) {
    // *ptr is only evaluated if ptr is non-null
}
```

See [Short-Circuit Evaluation](short-circuit.md) for more details.
