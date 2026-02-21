# Associated Functions

Associated functions are functions inside an `impl` block that do not take `self` as a parameter. They are called on the type name, not on an instance.

## Declaration

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

## Calling

Use the type name followed by dot and the function name:

```cst
let is Point as p = Point.new(3, 4);
let is Point as o = Point.origin();
```

## Common Pattern: Constructors

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

## How It Works

The compiler resolves `Point.new(3, 4)` by checking if `Point` is a known struct/enum name. If so, it looks up `Point_new` and emits a regular function call. The distinction from method calls is that no instance address is passed.

## Distinguishing from Member Access

The semantic analyzer differentiates between:

- `p.sum()` -- `p` is a variable with struct type, so this is a method call
- `Point.new()` -- `Point` is a type name, so this is an associated function call
