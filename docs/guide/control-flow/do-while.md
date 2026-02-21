# Do-While Loop

Executes the body at least once, then repeats while the condition is true.

## Syntax

```cst
do {
    // body
} while (condition);
```

## Example

```cst
let is i32 as x = 0;
do {
    x = x + 1;
} while (x < 10);
// x is now 10
```

## Guaranteed First Execution

Unlike `while`, the body always runs at least once, even if the condition is initially false.

```cst
let is i32 as x = 100;
do {
    // this runs once even though x >= 10
    x = x + 1;
} while (x < 10);
// x is now 101
```

## Rules

- Braces around the body are required.
- Parentheses around the condition are required.
- The trailing semicolon after `while (condition)` is required.
- `break` and `continue` work inside do-while loops.
