# For Loop

A loop with init, condition, and step expressions.

## Syntax

```cst
for (init; condition; step) {
    // body
}
```

- **init** -- executed once before the loop starts.
- **condition** -- checked before each iteration. Loop exits when false (zero).
- **step** -- executed after each iteration body.

## Basic Example

```cst
for (let is i32 as i = 0; i < 10; i += 1) {
    // runs 10 times with i = 0..9
}
```

## Iterating Over an Array

```cst
let is [5]i32 as arr;
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
arr[3] = 40;
arr[4] = 50;

let is i64 as sum = 0;
for (let is i32 as i = 0; i < 5; i += 1) {
    sum = sum + arr[i];
}
```

## Nested Loops

```cst
for (let is i32 as i = 0; i < 3; i += 1) {
    for (let is i32 as j = 0; j < 3; j += 1) {
        // 9 iterations total
    }
}
```

## Rules

- Parentheses around the three clauses are required.
- Braces around the body are required.
- The init clause can declare a variable with `let`.
- `break` and `continue` work inside for loops.
