# Address-Of

The `&` operator returns a pointer to a variable, array element, or struct field.

## Local Variables

```cst
let is i32 as x = 42;
let is *i32 as p = &x;
// p now points to x on the stack
```

The result type is `*T` where `T` is the type of the operand.

## Array Elements

```cst
let is [10]i32 as arr;
let is *i32 as first = &arr[0];
let is *i32 as fifth = &arr[4];
```

This is how you pass arrays to functions -- take the address of element 0:

```cst
fn sum(data as *i32, len as i64) as i32 { ... }

let is [5]i32 as nums;
sum(&nums[0], 5);
```

## Struct Fields

```cst
struct Point { x as i32; y as i32; }

let is Point as p;
let is *i32 as px = &p.x;
let is *i32 as py = &p.y;
```

## Restrictions

- Cannot take the address of global variables (they use RIP-relative addressing internally).
- Cannot take the address of a literal or expression result -- only named storage locations.
