# If/Else

Conditional branching. The condition must be in parentheses and braces are required around the body.

## Basic If

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
}
```

## If/Else

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
} else {
    syscall(1, 1, "non-positive\n", 13);
}
```

## Else If Chains

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
} else if (x == 0) {
    syscall(1, 1, "zero\n", 5);
} else {
    syscall(1, 1, "negative\n", 9);
}
```

## Condition Semantics

The condition evaluates to an integer. Zero is false, any non-zero value is true.

```cst
let is i64 as flags = 3;
if (flags) {
    // executes because flags != 0
}
```

## Nesting

If/else blocks can be nested arbitrarily.

```cst
if (x > 0) {
    if (x < 100) {
        syscall(1, 1, "small positive\n", 15);
    } else {
        syscall(1, 1, "large positive\n", 15);
    }
}
```

## Rules

- Parentheses around the condition are required.
- Braces around the body are required (no single-statement shorthand).
- `else if` is not a special keyword -- it is an `else` followed by another `if`.
