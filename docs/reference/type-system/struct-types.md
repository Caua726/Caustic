# Struct Types

Structs are user-defined composite types that group named fields together. They use packed layout with no alignment padding.

## Type Kind

Structs use `TY_STRUCT` (kind value 16). The `Type` struct stores:

```
Type {
    kind: TY_STRUCT (16)
    name_ptr: *u8        -- struct name (e.g., "Point")
    name_len: i64        -- length of name string
    size: i64            -- total packed size
    fields: *Node        -- linked list of field declarations
    field_count: i64     -- number of fields
    gen_params: ...      -- generic parameters (if template)
    gen_param_count: i64 -- number of generic parameters
    gen_args: ...        -- concrete type arguments (if instantiated)
    gen_arg_count: i64   -- number of concrete type arguments
}
```

## Declaration

```cst
struct Point {
    x as i32;
    y as i32;
}

struct Person {
    name as *u8;
    age as i32;
    height as f64;
}
```

## Packed Memory Layout

Caustic structs are **packed**: fields are placed at consecutive byte offsets with no padding or alignment:

```cst
struct Example {
    a as i8;     // offset 0, size 1
    b as i64;    // offset 1, size 8
    c as i32;    // offset 9, size 4
}
// total size = 13 bytes
```

In C, the same struct would typically be 24 bytes due to alignment padding. This difference is critical when interfacing with C code.

### Offset Calculation

Field offsets are computed as the cumulative sum of preceding field sizes:

```
field[0].offset = 0
field[i].offset = field[i-1].offset + sizeof(field[i-1].type)
struct.size = last_field.offset + sizeof(last_field.type)
```

## Instantiation

Struct variables are declared with the struct name as the type:

```cst
let is Point as p;
p.x = 10;
p.y = 20;
```

Structs can also be returned from functions:

```cst
impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }
}
```

## Field Access

Fields are accessed with dot notation. The compiler resolves the field name to its offset and type:

```cst
let is i32 as val = p.x;   // read field
p.y = 30;                   // write field
```

Through a pointer, automatic dereference is applied:

```cst
fn modify(ptr as *Point) as void {
    ptr.x = 100;   // auto-deref, equivalent to (*ptr).x
}
```

## Nested Structs

Structs can contain other structs as fields. The inner struct is stored inline (not as a pointer):

```cst
struct Rect {
    origin as Point;    // 8 bytes inline (Point is packed)
    width as i32;       // offset 8
    height as i32;      // offset 12
}
// size = 16

let is Rect as r;
r.origin.x = 0;
r.origin.y = 0;
r.width = 100;
r.height = 50;
```

## Generic Structs

Structs can be parameterized with type parameters:

```cst
struct Pair gen T, U {
    first as T;
    second as U;
}

let is Pair gen i32, f64 as p;
p.first = 42;
p.second = 3.14;
```

Generic structs are monomorphized: `Pair gen i32, f64` produces a concrete type named `Pair_i32_f64` with fully resolved field types and sizes. See [Generic Instantiation](../semantic/generic-instantiation.md).

## Impl Blocks

Methods and associated functions are defined in `impl` blocks. The parser desugars these into top-level functions with mangled names:

```cst
impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }

    fn origin() as Point {
        let is Point as p;
        p.x = 0;
        p.y = 0;
        return p;
    }
}

// Desugared to:
// fn Point_sum(self as *Point) as i32 { ... }
// fn Point_origin() as Point { ... }
```

Method calls are rewritten during semantic analysis:

```cst
let is i32 as s = p.sum();        // becomes: Point_sum(&p)
let is Point as o = Point.origin(); // becomes: Point_origin()
```

## Type Identity

Struct types are compared by **name**, not by structure. Two structs with identical fields but different names are distinct types:

```cst
struct Vec2 { x as i32; y as i32; }
struct Pos  { x as i32; y as i32; }

let is Vec2 as v;
// let is Pos as p = v;   // ERROR: Vec2 is not Pos
```

## Size

The total size is the sum of all field sizes (packed, no padding):

```cst
sizeof(Point)    // 8  (4 + 4)
sizeof(Person)   // 20 (8 + 4 + 8)
sizeof(Example)  // 13 (1 + 8 + 4)
```

## Limitations

- **No default values**: Fields are uninitialized unless explicitly assigned.
- **No cross-module struct fields**: A field cannot have a type like `mod.StructType`. Use `*u8` with accessor functions as a workaround.
- **No anonymous structs**: All structs must have a name.
- **No bit fields**: Fields always occupy their full type size.
