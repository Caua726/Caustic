# Structs

## Declaration

Structs group related data into a single type with named fields.

### Syntax

```cst
struct Point {
    x as i32;
    y as i32;
}
```

Fields are separated by semicolons. Each field has a name and a type.

### Creating a Struct

Declare a variable of the struct type, then assign each field:

```cst
let is Point as p;
p.x = 10;
p.y = 20;
```

There is no struct literal syntax -- you assign fields individually after declaration.

### Multiple Fields

Structs can have any number of fields with any types:

```cst
struct Person {
    age as i32;
    height as i64;
    active as bool;
    name as *u8;
}
```

### Passing to Functions

Structs can be passed by value or by pointer:

```cst
fn print_point(p as Point) as void {
    // p is a copy
}

fn move_point(p as *Point, dx as i32) as void {
    p.x = p.x + dx;
}
```

### Complete Example

```cst
struct Rect {
    x as i32;
    y as i32;
    w as i32;
    h as i32;
}

fn area(r as Rect) as i32 {
    return r.w * r.h;
}

fn main() as i32 {
    let is Rect as r;
    r.x = 0;
    r.y = 0;
    r.w = 100;
    r.h = 50;
    return area(r); // 5000 (truncated to exit code 0-255)
}
```

## Field Access

### Direct Access

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

### Through Pointers

Pointer-to-struct uses the same dot syntax. There is no `->` operator -- Caustic automatically dereferences the pointer:

```cst
fn set_x(p as *Point, val as i32) as void {
    p.x = val;    // automatic dereference, same as (*p).x
}

let is Point as pt;
let is *Point as ptr = &pt;
ptr.x = 42;       // works directly
```

### Assignment

Fields are assignable with `=`:

```cst
p.x = 10;
p.x = p.x + 1;
```

### In Expressions

Fields can be used anywhere an expression of their type is expected:

```cst
let is i32 as sum = p.x + p.y;
syscall(60, p.x);   // pass field as syscall arg
some_func(p.x, p.y);
```

### Array Fields

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

### Nested Field Access

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

## Packed Layout

Caustic structs are **packed** -- there is no alignment padding between fields. Fields are laid out sequentially in memory, and `sizeof(struct)` equals the sum of all field sizes.

### Layout Rules

- Fields are stored in declaration order
- No padding between fields
- No trailing padding
- `sizeof(struct)` = sum of all field sizes

### Example

```cst
struct Mixed {
    a as i64;   // offset 0, size 8
    b as i32;   // offset 8, size 4
    c as i64;   // offset 12, size 8
}
// sizeof(Mixed) = 20
```

In C with standard alignment, the equivalent struct would be 24 bytes (4 bytes padding after `b`). In Caustic, it is 20 bytes.

### Field Offsets

Offsets are computed by summing preceding field sizes:

```cst
struct Example {
    a as u8;    // offset 0, size 1
    b as i32;   // offset 1, size 4
    c as i64;   // offset 5, size 8
    d as u8;    // offset 13, size 1
}
// sizeof(Example) = 14
```

### C Interop Warning

Because C structs have alignment padding and Caustic structs do not, their layouts will differ for mixed-size fields. If you need to match a C struct layout:

- Use all-i64 fields (naturally aligned at any offset multiple of 8)
- Or manually insert padding fields:

```cst
struct CCompatible {
    a as i64;       // offset 0
    b as i32;       // offset 8
    _pad as i32;    // offset 12 (manual padding)
    c as i64;       // offset 16
}
// sizeof = 24, matches C layout
```

### Sizeof

Use `sizeof(StructName)` to get the total size at compile time:

```cst
struct Point { x as i32; y as i32; }

let is *Point as p = cast(*Point, mem.galloc(sizeof(Point)));
// allocates 8 bytes
```

## Nested Structs

A struct field can be of another struct type. The inner struct is stored inline (not as a pointer).

### Declaration

```cst
struct Point {
    x as i32;
    y as i32;
}

struct Rect {
    origin as Point;
    size as Point;
}
```

### Access

Use chained dot notation to reach nested fields:

```cst
let is Rect as r;
r.origin.x = 0;
r.origin.y = 0;
r.size.x = 100;
r.size.y = 50;
```

### Memory Layout

The inner struct is embedded directly. Since Caustic structs are packed:

```cst
struct Rect {
    origin as Point;   // offset 0, size 8 (two i32s)
    size as Point;     // offset 8, size 8
}
// sizeof(Rect) = 16
```

### Multiple Levels

Nesting can go multiple levels deep:

```cst
struct Color {
    r as u8;
    g as u8;
    b as u8;
}

struct Vertex {
    pos as Point;
    col as Color;
}

struct Triangle {
    v0 as Vertex;
    v1 as Vertex;
    v2 as Vertex;
}

let is Triangle as tri;
tri.v0.pos.x = 10;
tri.v0.col.r = 255;
```

### Passing Nested Structs

When passing by pointer, nested field access works through the pointer:

```cst
fn get_width(r as *Rect) as i32 {
    return r.size.x;
}
```

### Limitation

You cannot use a struct from another module directly as a field type. Use a pointer instead:

```cst
// This may fail:
// struct Wrapper { s as mod.SomeStruct; }

// Use a pointer:
struct Wrapper {
    s as *u8;  // cast to/from *mod.SomeStruct
}
```

## Struct Pointers

### Declaring a Struct Pointer

```cst
let is *Point as ptr;
```

### Getting a Pointer to a Local Struct

Use the address-of operator `&`:

```cst
let is Point as p;
p.x = 10;
p.y = 20;
let is *Point as ptr = &p;
```

### Heap Allocation

Allocate struct memory on the heap using `mem.galloc` and cast:

```cst
use "std/mem.cst" as mem;

let is *Point as p = cast(*Point, mem.galloc(sizeof(Point)));
p.x = 42;
p.y = 99;

// Free when done
mem.gfree(cast(*u8, p));
```

### Field Access Through Pointers

Caustic uses the same dot notation for pointer and value access. No `->` operator exists:

```cst
ptr.x = 10;           // writes through pointer
let is i32 as v = ptr.y;  // reads through pointer
```

### Passing to Functions

Pass struct pointers to avoid copying and to allow mutation:

```cst
fn reset(p as *Point) as void {
    p.x = 0;
    p.y = 0;
}

let is Point as pt;
pt.x = 5;
reset(&pt);
// pt.x is now 0
```

### Pointer Arithmetic

Cast to `*u8` for byte-level pointer math, then cast back:

```cst
// Array of structs on the heap
let is i64 as count = 10;
let is *u8 as raw = mem.galloc(sizeof(Point) * count);
let is *Point as arr = cast(*Point, raw);

// Access second element (offset by sizeof(Point))
let is *Point as second = cast(*Point, cast(*u8, arr) + sizeof(Point));
second.x = 100;
```

### Null Check Pattern

Caustic has no null keyword. Use 0 cast to a pointer:

```cst
let is *Point as p = cast(*Point, 0);

// Check before use
if (cast(i64, p) != 0) {
    p.x = 10;
}
```

## Struct Return

Functions can return structs. The calling convention depends on the struct size.

### Small Structs (8 bytes or less)

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

### Large Structs (more than 8 bytes)

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

### How SRET Works (Internals)

For structs larger than 8 bytes:

1. The caller allocates space on the stack for the return value
2. A hidden pointer to that space is passed as the first argument (`rdi`)
3. The callee writes the struct through that pointer
4. The callee returns the pointer in `rax`

This is handled automatically by the compiler. You write `return my_struct;` and it works.

### With Impl Methods

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
