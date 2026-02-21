# Short-Circuit Evaluation

Logical operators `&&` and `||` use short-circuit evaluation, meaning the right operand is only evaluated if necessary.

## Logical AND (`&&`)

If the left operand is false (0), the result is false regardless of the right operand. The right operand is **not evaluated**.

```cst
// safe_divide only divides if b is non-zero
fn safe_divide(a as i32, b as i32) as i32 {
    if (b != 0 && a / b > 10) {
        return 1;
    }
    return 0;
}
```

If `b` is 0, the expression `a / b` is never executed, avoiding a division by zero.

## Logical OR (`||`)

If the left operand is true (non-zero), the result is true regardless of the right operand. The right operand is **not evaluated**.

```cst
fn has_access(is_admin as i32, has_token as i32) as i32 {
    if (is_admin || has_token) {
        return 1;
    }
    return 0;
}
```

If `is_admin` is true, `has_token` is never checked.

## Null Pointer Guard

The most important use case is guarding pointer dereferences:

```cst
fn check(ptr as *i32) as i32 {
    if (ptr != cast(*i32, 0) && *ptr == 42) {
        return 1;
    }
    return 0;
}
```

If `ptr` is null, `*ptr` is never evaluated, preventing a segfault.

## Implementation

The compiler implements short-circuit evaluation using conditional jumps in the IR. For `&&`, if the left side is false, it jumps past the right side. For `||`, if the left side is true, it jumps past the right side. No temporary boolean values are created.

## Chaining

Short-circuit operators can be chained. Evaluation stops as soon as the result is determined:

```cst
if (a > 0 && b > 0 && c > 0) {
    // only reached if all three are positive
    // if a <= 0, neither b > 0 nor c > 0 is evaluated
}
```
