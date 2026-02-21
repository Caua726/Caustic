# Cast

Caustic requires explicit type conversions using `cast(TargetType, expression)`. There are no implicit casts between different types (except literal narrowing, e.g. an `i64` literal assigned to an `i32` variable).

## Syntax

```cst
cast(TargetType, expression)
```

## Integer Size Conversions

Widen or narrow between integer types:

```cst
let is i64 as big = 1000;
let is i32 as small = cast(i32, big);
let is i64 as back = cast(i64, small);
```

## Integer to Pointer

Convert a raw address to a typed pointer:

```cst
let is i64 as addr = 0x7fff0000;
let is *u8 as ptr = cast(*u8, addr);
```

## Pointer to Integer

Convert a pointer back to an integer for arithmetic or comparison:

```cst
let is *u8 as ptr = mem.galloc(64);
let is i64 as addr = cast(i64, ptr);
```

## Between Pointer Types

Reinterpret a pointer as a different type:

```cst
let is *u8 as raw = mem.galloc(sizeof(Point));
let is *Point as p = cast(*Point, raw);
```

## Common Pattern: Typed Heap Allocation

```cst
use "std/mem.cst" as mem;

struct Node {
    value as i64;
    next as *u8;
}

fn new_node(val as i64) as *Node {
    let is *u8 as raw = mem.galloc(sizeof(Node));
    let is *Node as n = cast(*Node, raw);
    n.value = val;
    n.next = cast(*u8, 0);
    return n;
}
```

## Rules

- `cast` does not perform any runtime checks -- it is a raw reinterpretation.
- You cannot cast between floats and integers directly. Assign through intermediate variables if needed.
- The result of `cast` cannot be used with member access directly. Split into two statements:

```cst
// WRONG: cast(*Point, raw).x
// RIGHT:
let is *Point as p = cast(*Point, raw);
p.x = 10;
```
