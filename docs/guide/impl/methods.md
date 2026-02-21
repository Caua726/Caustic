# Methods

Impl blocks attach methods to struct and enum types. Methods receive the instance as their first parameter (`self`).

## Declaration

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

## Calling Methods

Use dot syntax on an instance. The compiler automatically takes the address of the instance and passes it as `self`:

```cst
let is Point as p;
p.x = 3;
p.y = 4;

let is i32 as s = p.sum();    // calls Point_sum(&p)
p.scale(2);                    // calls Point_scale(&p, 2)
```

## Calling on Pointers

If you already have a pointer, dot syntax passes it directly (no extra `&`):

```cst
let is *Point as ptr = cast(*Point, addr);
let is i32 as s = ptr.sum();   // calls Point_sum(ptr)
```

## Accessing Fields

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

## Limitations

- Cannot chain method calls: `a.method1().method2()` is not supported. Split into separate statements.
- `self` must be a pointer (`*Type`), not a value.
- Methods cannot be called without an instance (use associated functions for that).
