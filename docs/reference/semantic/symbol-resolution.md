# Symbol Resolution

Symbol resolution is the process of connecting each identifier in the source code to its declaration. In Caustic, this happens during the `walk()` pass, with different strategies depending on how the identifier is used.

## Simple Identifiers

A bare identifier (e.g., `x`, `foo`) is looked up in the scope stack from innermost to outermost. The first matching declaration is used:

```cst
let is i32 as x = 10;

fn foo() as i32 {
    let is i32 as x = 20;  // shadows global x
    return x;               // resolves to local x (20)
}
```

If no matching declaration is found in any scope, the compiler reports an "undeclared identifier" error.

## Module Member Access

When an identifier has the form `alias.name`, the compiler:

1. Looks up `alias` in the current scope to find the module.
2. Searches the module's cached symbol table for `name`.
3. Returns the resolved node from the module.

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    let is *u8 as ptr = mem.galloc(1024);   // resolve galloc in mem module
    mem.gfree(ptr);                          // resolve gfree in mem module
    return 0;
}
```

Module members are not imported into the local scope. The qualified form (`alias.name`) must always be used.

## Struct and Enum Type Names

Type names appearing in declarations (`let is TypeName as x`) are resolved through a type registry separate from the variable scope. Struct and enum declarations register their names during the `register_vars` pass:

```cst
struct Point {
    x as i32;
    y as i32;
}

let is Point as p;   // Point resolved via type registry
```

## Method Calls (Impl Resolution)

When the compiler encounters `instance.method(args)`, it follows this resolution process:

1. Determine the type of `instance` (must be a struct or pointer-to-struct).
2. Look up `method` in the struct's impl block.
3. Rewrite the call as `StructName_method(&instance, args)`.

The `self` parameter is automatically inserted as a pointer to the instance:

```cst
struct Point {
    x as i32;
    y as i32;
}

impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

fn main() as i32 {
    let is Point as p;
    p.x = 3;
    p.y = 4;
    return p.sum();   // rewritten to: Point_sum(&p)
}
```

## Associated Functions

When a function call has the form `TypeName.func(args)`, the compiler checks whether `TypeName` refers to a struct or enum (using `lookup_struct` / `lookup_enum`). If it does, the call is rewritten as `TypeName_func(args)` without any `self` insertion:

```cst
impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }
}

fn main() as i32 {
    let is Point as p = Point.new(3, 4);  // rewritten to: Point_new(3, 4)
    return p.sum();
}
```

The distinction between a method call and an associated function call is the presence of `self` as the first parameter in the impl function.

## Function Pointer Resolution

The `fn_ptr()` expression resolves a function name to its address. Several forms are supported:

```cst
// Local function
let is *u8 as cb = fn_ptr(my_func);

// Module function
let is *u8 as cb = fn_ptr(mod.func);

// Generic function (specific instantiation)
let is *u8 as cb = fn_ptr(max gen i32);

// Impl method
let is *u8 as cb = fn_ptr(Point_sum);
```

The compiler verifies that the named function exists and resolves it to its mangled name for code generation.

## Extern Functions

Functions declared with `extern fn` are registered in the global scope like regular functions, but have no body. They are resolved normally during lookup:

```cst
extern fn printf(fmt as *u8) as i32;

fn main() as i32 {
    printf("hello\n");  // resolved from global scope, linked externally
    return 0;
}
```

## Resolution Order Summary

When the walker encounters an identifier or dotted name, it applies these checks in order:

1. **Variable lookup**: search scope stack for a local/global variable.
2. **Module lookup**: if the form is `alias.name`, search the module's exports.
3. **Type lookup**: if used in a type position, search the type registry.
4. **Impl lookup**: if the form is `expr.method()`, check if `expr`'s type has an impl with that method.
5. **Associated function lookup**: if the form is `Name.func()`, check if `Name` is a struct or enum type.
