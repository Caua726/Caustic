# Struct Return

Functions can return structs. The calling convention depends on the struct size.

## Small Structs (8 bytes or less)

Returned directly in the `rax` register. No special handling needed:

```cst
struct Pair {
    a as i32;
    b as i32;
}
// sizeof(Pair) = 8, fits in rax

fn make_pair(x as i32, y as i32) as Pair {
    let is Pair as p;
    p.a = x;
    p.b = y;
    return p;
}

fn main() as i32 {
    let is Pair as p = make_pair(10, 20);
    return p.a + p.b; // 30
}
```

## Large Structs (more than 8 bytes)

The compiler uses SRET (struct return) automatically. The caller allocates stack space and passes a hidden pointer as the first argument. This is transparent to the programmer:

```cst
struct Vec3 {
    x as i64;
    y as i64;
    z as i64;
}
// sizeof(Vec3) = 24, uses SRET

fn make_vec(x as i64, y as i64, z as i64) as Vec3 {
    let is Vec3 as v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

fn main() as i32 {
    let is Vec3 as v = make_vec(1, 2, 3);
    return cast(i32, v.x + v.y + v.z); // 6
}
```

## How SRET Works (Internals)

For structs larger than 8 bytes:

1. The caller allocates space on the stack for the return value
2. A hidden pointer to that space is passed as the first argument (`rdi`)
3. The callee writes the struct through that pointer
4. The callee returns the pointer in `rax`

This is handled automatically by the compiler. You write `return my_struct;` and it works.

## With Impl Methods

Struct return works the same with impl methods:

```cst
struct Point { x as i32; y as i32; }

impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }
}

let is Point as p = Point.new(10, 20);
```
