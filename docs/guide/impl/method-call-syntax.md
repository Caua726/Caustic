# Method Call Syntax

Caustic uses dot syntax for method calls and associated function calls. The compiler resolves these based on whether the left-hand side is a variable or a type name.

## Method Calls

When the left side of the dot is a variable with a struct type, the compiler treats it as a method call:

```cst
let is Point as p;
p.x = 3;
p.y = 4;
let is i32 as s = p.sum();     // Point_sum(&p)
p.scale(2);                     // Point_scale(&p, 2)
```

The compiler automatically takes the address of `p` and passes it as the `self` argument.

## Pointer Method Calls

If the variable is already a pointer to a struct, it is passed directly without taking its address:

```cst
let is *Point as ptr = cast(*Point, addr);
let is i32 as s = ptr.sum();   // Point_sum(ptr) — no extra &
```

## Associated Function Calls

When the left side of the dot is a type name (struct or enum), the compiler treats it as an associated function call:

```cst
let is Point as p = Point.new(3, 4);   // Point_new(3, 4)
let is Point as o = Point.origin();     // Point_origin()
```

## Resolution Logic

The semantic analyzer follows this process for `a.b(args)`:

1. Look up `a` in the current scope as a variable.
2. If `a` is a variable with struct type `S`, look for a method `S_b`. If found, emit `S_b(&a, args)`.
3. If `a` is not a variable, check if `a` is a struct or enum name. If so, look for `a_b`. If found, emit `a_b(args)`.
4. If neither matches, report an error.

## Limitations

**No method chaining:**

```cst
// NOT supported:
// let is i32 as x = builder.set_x(3).set_y(4).build();

// Split into separate statements:
builder.set_x(3);
builder.set_y(4);
let is i32 as x = builder.build();
```

**No member access on cast result:**

```cst
// NOT supported:
// cast(*Point, addr).sum()

// Split into separate statements:
let is *Point as p = cast(*Point, addr);
let is i32 as s = p.sum();
```

**No dot syntax for module functions:**

Module function calls use the same dot syntax but are resolved differently. `mod.func()` calls function `func` from module `mod`, not a method on a variable named `mod`.

```cst
use "std/mem.cst" as mem;
let is *u8 as ptr = mem.galloc(1024);  // module function, not method
```

The compiler distinguishes these by checking whether `mem` is a module alias or a variable.
