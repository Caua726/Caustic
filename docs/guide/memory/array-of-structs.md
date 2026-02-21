# Array of Structs

Arrays can hold struct values, stored contiguously in memory.

## Stack-Allocated

```cst
struct Point { x as i32; y as i32; }

let is [10]Point as points;
points[0].x = 1;
points[0].y = 2;
points[3].x = 10;
```

Elements are packed end-to-end. For `Point` (two i32 fields = 8 bytes), `points[3]` starts at byte offset `3 * 8 = 24`.

## Heap-Allocated

For dynamic or large arrays, allocate on the heap:

```cst
use "std/mem.cst" as mem;

struct Entity { id as i32; hp as i32; x as i64; y as i64; }

fn main() as i32 {
    mem.gheapinit(1048576);
    let is i64 as count = 100;
    let is *u8 as buf = mem.galloc(count * cast(i64, sizeof(Entity)));

    let is i64 as i = 0;
    while (i < count) {
        let is *Entity as e = cast(*Entity, cast(i64, buf) + i * cast(i64, sizeof(Entity)));
        e.id = cast(i32, i);
        e.hp = 100;
        e.x = i * 10;
        e.y = i * 20;
        i = i + 1;
    }

    mem.gfree(buf);
    return 0;
}
```

## Packed Layout

Caustic structs have **no padding**. Fields are packed sequentially.

```cst
struct Mixed { a as i32; b as i64; }
// sizeof(Mixed) = 12 (not 16)
// Mixed.a at offset 0, Mixed.b at offset 4
```

This differs from C, where the same struct would be 16 bytes due to alignment. Keep this in mind when computing element addresses manually.

## Accessing Nested Fields

```cst
struct Rect { origin as Point; size as Point; }

let is [5]Rect as rects;
rects[0].origin.x = 0;
rects[0].origin.y = 0;
rects[0].size.x = 100;
rects[0].size.y = 50;
```
