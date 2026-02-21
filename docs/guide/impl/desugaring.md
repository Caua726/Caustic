# Impl Block Desugaring

Impl blocks are syntactic sugar. The parser transforms them into top-level functions at parse time. By the time the semantic analyzer runs, impl methods are regular functions with mangled names.

## Transformation Rules

### Methods

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

### Associated Functions

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

## Call Site Desugaring

The semantic analyzer transforms call sites:

| Source | Desugared To |
|--------|-------------|
| `p.sum()` | `Point_sum(&p)` |
| `p.scale(2)` | `Point_scale(&p, 2)` |
| `Point.new(3, 4)` | `Point_new(3, 4)` |
| `ptr.sum()` (ptr is `*Point`) | `Point_sum(ptr)` |

## When Does This Happen?

1. **Parse time**: The parser reads `impl Point { fn sum(...) }` and emits a top-level `fn Point_sum(...)` node.
2. **Semantic time**: When the analyzer sees `p.sum()`, it checks if `p` has a struct type with a method named `sum`. If so, it rewrites the call to `Point_sum(&p)`.
3. **IR/Codegen**: Sees only regular function calls. No special impl logic needed.

## Multiple Impl Blocks

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

## Direct Calls

Since methods are just functions, you can call them directly by their mangled name if needed:

```cst
let is i32 as s = Point_sum(&p);  // same as p.sum()
```
