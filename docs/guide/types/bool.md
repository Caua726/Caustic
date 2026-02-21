# Bool

The `bool` type represents a boolean value, stored as 1 byte.

## Declaration

```cst
let is bool as flag = true;
let is bool as done = false;
```

The values `true` and `false` are stored internally as `1` and `0`.

## Comparison Results

Comparison operators produce `bool` values.

```cst
let is i32 as x = 10;
let is i32 as y = 20;

let is bool as eq = x == y;     // false
let is bool as ne = x != y;     // true
let is bool as lt = x < y;      // true
let is bool as gt = x > y;      // false
let is bool as le = x <= y;     // true
let is bool as ge = x >= y;     // false
```

## Conditions

Booleans are used directly in `if` and `while` conditions.

```cst
let is bool as running = true;

if running {
    // ...
}

while running {
    // ...
}
```

## Logical Operators

```cst
let is bool as a = true;
let is bool as b = false;

if a && b {
    // both true
}

if a || b {
    // at least one true
}
```
