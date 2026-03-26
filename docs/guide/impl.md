# Impl Blocks

## Methods

Impl blocks attach methods to struct and enum types. Methods receive the instance as their first parameter (`self`).

### Declaration

```cst
struct Point {
    x as i32;
    y as i32;
}

impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }

    fn scale(self as *Point, factor as i32) as void {
        self.x = self.x * factor;
        self.y = self.y * factor;
    }
}
```

The `self` parameter must be a pointer to the impl type (`*Point`). It is always the first parameter.

### Calling Methods

Use dot syntax on an instance. The compiler automatically takes the address of the instance and passes it as `self`:

```cst
let is Point as p;
p.x = 3;
p.y = 4;

let is i32 as s = p.sum();    // calls Point_sum(&p)
p.scale(2);                    // calls Point_scale(&p, 2)
```

### Calling on Pointers

If you already have a pointer, dot syntax passes it directly (no extra `&`):

```cst
let is *Point as ptr = cast(*Point, addr);
let is i32 as s = ptr.sum();   // calls Point_sum(ptr)
```

### Accessing Fields

Methods access fields through the `self` pointer:

```cst
impl Point {
    fn set(self as *Point, x as i32, y as i32) as void {
        self.x = x;
        self.y = y;
    }

    fn magnitude_sq(self as *Point) as i32 {
        return self.x * self.x + self.y * self.y;
    }
}
```

### Limitations

- Cannot chain method calls: `a.method1().method2()` is not supported. Split into separate statements.
- `self` must be a pointer (`*Type`), not a value.
- Methods cannot be called without an instance (use associated functions for that).

## Associated Functions

Associated functions are functions inside an `impl` block that do not take `self` as a parameter. They are called on the type name, not on an instance.

### Declaration

```cst
struct Point {
    x as i32;
    y as i32;
}

impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }

    fn origin() as Point {
        return Point.new(0, 0);
    }
}
```

### Calling

Use the type name followed by dot and the function name:

```cst
let is Point as p = Point.new(3, 4);
let is Point as o = Point.origin();
```

### Common Pattern: Constructors

Associated functions are typically used as constructors since Caustic has no special constructor syntax:

```cst
struct Buffer {
    ptr as *u8;
    len as i64;
    cap as i64;
}

impl Buffer {
    fn new(capacity as i64) as Buffer {
        let is Buffer as b;
        b.ptr = cast(*u8, mem.galloc(capacity));
        b.len = 0;
        b.cap = capacity;
        return b;
    }

    fn free(self as *Buffer) as void {
        mem.gfree(self.ptr);
    }
}

let is Buffer as buf = Buffer.new(1024);
defer buf.free();
```

### How It Works

The compiler resolves `Point.new(3, 4)` by checking if `Point` is a known struct/enum name. If so, it looks up `Point_new` and emits a regular function call. The distinction from method calls is that no instance address is passed.

### Distinguishing from Member Access

The semantic analyzer differentiates between:

- `p.sum()` -- `p` is a variable with struct type, so this is a method call
- `Point.new()` -- `Point` is a type name, so this is an associated function call

## Desugaring

Impl blocks are syntactic sugar. The parser transforms them into top-level functions at parse time. By the time the semantic analyzer runs, impl methods are regular functions with mangled names.

### Transformation Rules

**Methods:**

```cst
// Source:
impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

// Becomes:
fn Point_sum(self as *Point) as i32 {
    return self.x + self.y;
}
```

**Associated Functions:**

```cst
// Source:
impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x; p.y = y;
        return p;
    }
}

// Becomes:
fn Point_new(x as i32, y as i32) as Point {
    let is Point as p;
    p.x = x; p.y = y;
    return p;
}
```

### Call Site Desugaring

The semantic analyzer transforms call sites:

| Source | Desugared To |
|--------|-------------|
| `p.sum()` | `Point_sum(&p)` |
| `p.scale(2)` | `Point_scale(&p, 2)` |
| `Point.new(3, 4)` | `Point_new(3, 4)` |
| `ptr.sum()` (ptr is `*Point`) | `Point_sum(ptr)` |

### When Does This Happen?

1. **Parse time**: The parser reads `impl Point { fn sum(...) }` and emits a top-level `fn Point_sum(...)` node.
2. **Semantic time**: When the analyzer sees `p.sum()`, it checks if `p` has a struct type with a method named `sum`. If so, it rewrites the call to `Point_sum(&p)`.
3. **IR/Codegen**: Sees only regular function calls. No special impl logic needed.

### Multiple Impl Blocks

You can have multiple impl blocks for the same type. They all produce top-level functions:

```cst
impl Point {
    fn sum(self as *Point) as i32 { return self.x + self.y; }
}

impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x; p.y = y;
        return p;
    }
}
// Both Point_sum and Point_new exist as top-level functions
```

### Direct Calls

Since methods are just functions, you can call them directly by their mangled name if needed:

```cst
let is i32 as s = Point_sum(&p);  // same as p.sum()
```

## Generic Impl

Impl blocks can be generic, applying methods to a generic struct or enum. The generic parameters from the impl block are copied to each method inside it.

### Declaration

```cst
struct Wrapper gen T {
    value as T;
    count as i32;
}

impl Wrapper gen T {
    fn get(self as *Wrapper gen T) as T {
        return self.value;
    }

    fn set(self as *Wrapper gen T, val as T) as void {
        self.value = val;
        self.count = self.count + 1;
    }

    fn new(val as T) as Wrapper gen T {
        let is Wrapper gen T as w;
        w.value = val;
        w.count = 0;
        return w;
    }
}
```

### Usage

```cst
let is Wrapper gen i32 as w = Wrapper gen i32 .new(42);
let is i32 as v = w.get();
w.set(100);
```

### How It Works

1. The parser sees `impl Wrapper gen T { ... }` and copies the generic parameter `T` to each function inside the block.
2. Each function is desugared to a top-level generic function: `fn Wrapper_get gen T (self as *Wrapper gen T) as T`.
3. When `w.get()` is called on a `Wrapper gen i32`, the compiler instantiates `Wrapper_get gen i32`, producing `Wrapper_i32_get` (or `Wrapper_get_i32` depending on mangling order).
4. Type substitution replaces `Wrapper gen T` with `Wrapper_i32` in the self parameter and all usages within the method body.

### Multiple Type Parameters

```cst
struct Map gen K, V {
    keys as *K;
    vals as *V;
    len as i32;
}

impl Map gen K, V {
    fn get(self as *Map gen K, V, key as K) as V {
        // ...lookup logic...
    }
}
```

### Gotcha: substitute_type and generic_args

When the compiler substitutes types during generic instantiation, it must handle `generic_args` on struct type references. For example, `Wrapper gen T` in the self parameter has a `generic_args` list containing `T`. The substitution must replace each arg and re-mangle the struct name to produce `Wrapper_i32`.

## Method Call Syntax

Caustic uses dot syntax for method calls and associated function calls. The compiler resolves these based on whether the left-hand side is a variable or a type name.

### Method Calls

When the left side of the dot is a variable with a struct type, the compiler treats it as a method call:

```cst
let is Point as p;
p.x = 3;
p.y = 4;
let is i32 as s = p.sum();     // Point_sum(&p)
p.scale(2);                     // Point_scale(&p, 2)
```

The compiler automatically takes the address of `p` and passes it as the `self` argument.

### Pointer Method Calls

If the variable is already a pointer to a struct, it is passed directly without taking its address:

```cst
let is *Point as ptr = cast(*Point, addr);
let is i32 as s = ptr.sum();   // Point_sum(ptr) -- no extra &
```

### Associated Function Calls

When the left side of the dot is a type name (struct or enum), the compiler treats it as an associated function call:

```cst
let is Point as p = Point.new(3, 4);   // Point_new(3, 4)
let is Point as o = Point.origin();     // Point_origin()
```

### Resolution Logic

The semantic analyzer follows this process for `a.b(args)`:

1. Look up `a` in the current scope as a variable.
2. If `a` is a variable with struct type `S`, look for a method `S_b`. If found, emit `S_b(&a, args)`.
3. If `a` is not a variable, check if `a` is a struct or enum name. If so, look for `a_b`. If found, emit `a_b(args)`.
4. If neither matches, report an error.

### Limitations

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
