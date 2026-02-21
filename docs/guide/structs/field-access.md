# Field Access

## Direct Access

Use dot notation to read or write struct fields:

```cst
struct Point {
    x as i32;
    y as i32;
}

let is Point as p;
p.x = 10;           // write
let is i32 as v = p.x;  // read
```

## Through Pointers

Pointer-to-struct uses the same dot syntax. There is no `->` operator -- Caustic automatically dereferences the pointer:

```cst
fn set_x(p as *Point, val as i32) as void {
    p.x = val;    // automatic dereference, same as (*p).x
}

let is Point as pt;
let is *Point as ptr = &pt;
ptr.x = 42;       // works directly
```

## Assignment

Fields are assignable with `=`:

```cst
p.x = 10;
p.x = p.x + 1;
```

## In Expressions

Fields can be used anywhere an expression of their type is expected:

```cst
let is i32 as sum = p.x + p.y;
syscall(60, p.x);   // pass field as syscall arg
some_func(p.x, p.y);
```

## Array Fields

Access array elements inside structs:

```cst
struct Buffer {
    data as [64]u8;
    len as i32;
}

let is Buffer as buf;
buf.data[0] = 72;    // write to array element
buf.len = 1;
```

## Nested Field Access

When a struct contains another struct, chain dots:

```cst
struct Line {
    start as Point;
    end as Point;
}

let is Line as line;
line.start.x = 0;
line.end.x = 100;
```
