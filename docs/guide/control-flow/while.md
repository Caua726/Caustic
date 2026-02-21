# While Loop

Repeats a block while the condition is true (non-zero).

## Syntax

```cst
while (condition) {
    // body
}
```

## Counting Loop

```cst
let is i32 as i = 0;
while (i < 10) {
    // do work
    i = i + 1;
}
```

## Infinite Loop

```cst
while (1) {
    // runs forever until break or return
}
```

## Using Break and Continue

```cst
let is i32 as i = 0;
while (i < 100) {
    i = i + 1;
    if (i == 50) {
        break;
    }
    if (i == 25) {
        continue;
    }
    // skipped when i == 25, stops when i == 50
}
```

## Rules

- Parentheses around the condition are required.
- Braces around the body are required.
- The condition is checked before each iteration. If false on the first check, the body never executes.
