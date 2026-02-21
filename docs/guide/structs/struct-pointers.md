# Struct Pointers

## Declaring a Struct Pointer

```cst
let is *Point as ptr;
```

## Getting a Pointer to a Local Struct

Use the address-of operator `&`:

```cst
let is Point as p;
p.x = 10;
p.y = 20;
let is *Point as ptr = &p;
```

## Heap Allocation

Allocate struct memory on the heap using `mem.galloc` and cast:

```cst
use "std/mem.cst" as mem;

let is *Point as p = cast(*Point, mem.galloc(sizeof(Point)));
p.x = 42;
p.y = 99;

// Free when done
mem.gfree(cast(*u8, p));
```

## Field Access Through Pointers

Caustic uses the same dot notation for pointer and value access. No `->` operator exists:

```cst
ptr.x = 10;           // writes through pointer
let is i32 as v = ptr.y;  // reads through pointer
```

## Passing to Functions

Pass struct pointers to avoid copying and to allow mutation:

```cst
fn reset(p as *Point) as void {
    p.x = 0;
    p.y = 0;
}

let is Point as pt;
pt.x = 5;
reset(&pt);
// pt.x is now 0
```

## Pointer Arithmetic

Cast to `*u8` for byte-level pointer math, then cast back:

```cst
// Array of structs on the heap
let is i64 as count = 10;
let is *u8 as raw = mem.galloc(sizeof(Point) * count);
let is *Point as arr = cast(*Point, raw);

// Access second element (offset by sizeof(Point))
let is *Point as second = cast(*Point, cast(*u8, arr) + sizeof(Point));
second.x = 100;
```

## Null Check Pattern

Caustic has no null keyword. Use 0 cast to a pointer:

```cst
let is *Point as p = cast(*Point, 0);

// Check before use
if (cast(i64, p) != 0) {
    p.x = 10;
}
```
