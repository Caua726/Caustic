# Dereferencing

The `*` operator reads or writes through a pointer.

## Reading

```cst
let is i32 as x = 10;
let is *i32 as p = &x;
let is i32 as val = *p;   // val == 10
```

## Writing

```cst
let is i32 as x = 10;
let is *i32 as p = &x;
*p = 20;                   // x is now 20
```

## Struct Pointer Access

Caustic automatically dereferences struct pointers when accessing fields. There is no `->` operator.

```cst
struct Point { x as i32; y as i32; }

let is Point as pt;
pt.x = 5;

let is *Point as p = &pt;
p.x = 10;    // automatic dereference, same as (*p).x
let is i32 as val = p.y;  // reads through pointer
```

## Chained Access

```cst
struct Node { value as i32; next as *u8; }

let is *Node as n = cast(*Node, mem.galloc(sizeof(Node)));
n.value = 42;
n.next = cast(*u8, 0);

// follow the chain
let is *Node as next = cast(*Node, n.next);
```

## Works With Any Pointer Type

```cst
let is *u8 as bytes = mem.galloc(8);
*bytes = 0;

let is *i64 as nums = cast(*i64, bytes);
*nums = 123456;
```
