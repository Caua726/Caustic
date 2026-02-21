# Nested Structs

A struct field can be of another struct type. The inner struct is stored inline (not as a pointer).

## Declaration

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

## Access

Use chained dot notation to reach nested fields:

```cst
let is Rect as r;
r.origin.x = 0;
r.origin.y = 0;
r.size.x = 100;
r.size.y = 50;
```

## Memory Layout

The inner struct is embedded directly. Since Caustic structs are packed:

```cst
struct Rect {
    origin as Point;   // offset 0, size 8 (two i32s)
    size as Point;     // offset 8, size 8
}
// sizeof(Rect) = 16
```

## Multiple Levels

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

## Passing Nested Structs

When passing by pointer, nested field access works through the pointer:

```cst
fn get_width(r as *Rect) as i32 {
    return r.size.x;
}
```

## Limitation

You cannot use a struct from another module directly as a field type. Use a pointer instead:

```cst
// This may fail:
// struct Wrapper { s as mod.SomeStruct; }

// Use a pointer:
struct Wrapper {
    s as *u8;  // cast to/from *mod.SomeStruct
}
```
