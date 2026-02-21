# Pointer Types

Pointer types represent memory addresses pointing to a value of a specific type. All pointers are 8 bytes on x86_64.

## Type Kind

Pointers use `TY_PTR` (kind value 14). The `Type` struct's `base` field points to the `Type` of the pointed-to value.

```
Type {
    kind: TY_PTR (14)
    base: *Type  -- the pointed-to type
    size: 8
}
```

## Declaration Syntax

Pointer types are written as `*T` where `T` is the pointed-to type:

```cst
let is *i32 as p;           // pointer to i32
let is *u8 as bytes;        // pointer to u8 (commonly used as generic pointer)
let is *Point as sp;        // pointer to struct
let is **i32 as pp;         // pointer to pointer to i32
```

## Operations

### Address-Of (`&`)

The `&` operator produces a pointer to a variable:

```cst
let is i32 as x = 42;
let is *i32 as p = &x;    // p points to x
```

The result type is `*T` where `T` is the type of the operand.

### Dereference (`*`)

The `*` operator reads the value at the address a pointer holds:

```cst
let is *i32 as p = &x;
let is i32 as val = *p;   // val = 42
```

The result type is `T` where the pointer type is `*T`.

### Member Access Through Pointers

When accessing a struct member through a pointer, automatic dereference is applied. There is no `->` operator as in C:

```cst
struct Point { x as i32; y as i32; }

fn set(p as *Point) as void {
    p.x = 10;    // equivalent to (*p).x = 10
    p.y = 20;
}
```

### Pointer Arithmetic

Pointer arithmetic is performed through casts and integer operations:

```cst
let is *u8 as ptr = cast(*u8, base_addr);
let is *u8 as next = cast(*u8, cast(i64, ptr) + 8);
```

There is no built-in pointer addition syntax like `ptr + 1` that automatically scales by element size. All offset computation is manual.

## Multi-Level Pointers

Pointers to pointers are supported to arbitrary depth:

```cst
let is i32 as x = 42;
let is *i32 as p = &x;
let is **i32 as pp = &p;

let is i32 as val = **pp;   // val = 42
```

Each level of indirection adds another `TY_PTR` wrapper around the `base` type.

## Generic Pointer (*u8)

Since Caustic has no `*void` type, `*u8` is used as the generic pointer type. Memory allocators and low-level utilities return `*u8`:

```cst
let is *u8 as raw = mem.galloc(1024);
let is *Point as p = cast(*Point, raw);
```

## Null Pointers

There is no `null` keyword. A null pointer is represented as the integer value 0, cast to a pointer type:

```cst
let is *u8 as ptr = cast(*u8, 0);

// Check for null:
if cast(i64, ptr) == 0 {
    // ptr is null
}
```

## Type Compatibility

Pointer types are compared by their `base` type. Two pointer types are compatible only if they point to the same type:

```cst
let is *i32 as a = &x;
// let is *i64 as b = a;   // ERROR: *i32 is not *i64
let is *i64 as b = cast(*i64, a);   // OK with explicit cast
```

Assigning between different pointer types always requires an explicit `cast()`.

## Function Pointers

Function pointers use `*u8` as their type. The `fn_ptr()` expression obtains the address of a function:

```cst
fn callback(x as i32) as void {
    // ...
}

let is *u8 as cb = fn_ptr(callback);
```

Note: Caustic has no syntax for calling through a function pointer. Indirect calls require inline assembly or restructuring the code to use direct calls.

## Size

All pointer types are 8 bytes regardless of the pointed-to type:

```cst
sizeof(*i8)     // 8
sizeof(*i64)    // 8
sizeof(**i32)   // 8
sizeof(*Point)  // 8
```
