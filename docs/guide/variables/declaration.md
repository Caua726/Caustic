# Variable Declaration

Variables in Caustic are declared with the `let` keyword, followed by `is TYPE as name`.

## Syntax

```cst
let is TYPE as name = value;
let is TYPE as name;            // uninitialized
```

The `is` keyword is required. The type always comes before `as name`.

## Examples

### Integer types

```cst
let is i32 as x = 10;
let is i64 as big = 100000;
let is u8 as byte = 255;
```

### Pointers

```cst
let is *u8 as ptr = cast(*u8, 0);
let is *i32 as data;
```

### Booleans

```cst
let is bool as flag = true;
let is bool as done = false;
```

### Arrays

```cst
let is [64]u8 as buffer;
let is [10]i32 as nums;
```

### Structs

```cst
let is Point as p;
let is Vec3 as origin;
```

## Multiple declarations

Each variable requires its own `let` statement. There is no comma-separated multi-declaration syntax.

```cst
let is i32 as x = 0;
let is i32 as y = 0;
let is i32 as z = 0;
```

## Uninitialized variables

Omitting `= value` leaves the variable uninitialized. The contents are whatever happened to be on the stack at that location.

```cst
let is i64 as result;       // undefined contents until assigned
result = compute();
```
