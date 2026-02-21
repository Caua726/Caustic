# Enum Memory Layout

Understanding how enums are stored in memory.

## Simple Enum (No Data)

A simple enum is just a tag, stored as `i32` (4 bytes):

```cst
enum Color { Red; Green; Blue; }
// sizeof(Color) = 4
// Red=0, Green=1, Blue=2
```

## Tagged Union Layout

Enums with data use a fixed layout:

```
+--------+------------------+
|  tag   |     payload      |
| (i32)  | (max variant sz) |
+--------+------------------+
  4 bytes    N bytes
```

- **Tag** (offset 0): `i32` identifying the active variant (0, 1, 2, ...)
- **Payload** (offset after tag): sized to the **largest** variant's data

The total size is `4 + max_payload_size`, regardless of which variant is active.

## Worked Example

```cst
enum Shape {
    Circle as i32;           // tag=0, payload: 4 bytes
    Rect as i32, i32;        // tag=1, payload: 8 bytes
    Triangle as i32, i32, i32; // tag=2, payload: 12 bytes
    None;                    // tag=3, payload: 0 bytes
}
```

Layout breakdown:
- Tag: 4 bytes
- Max payload: 12 bytes (Triangle has 3 x i32)
- **Total sizeof(Shape) = 16 bytes**

Memory for `Shape.Circle(5)`:
```
offset 0:  [00 00 00 00]   tag = 0 (Circle)
offset 4:  [05 00 00 00]   radius = 5
offset 8:  [?? ?? ?? ??]   unused
offset 12: [?? ?? ?? ??]   unused
```

Memory for `Shape.Rect(10, 20)`:
```
offset 0:  [01 00 00 00]   tag = 1 (Rect)
offset 4:  [0A 00 00 00]   width = 10
offset 8:  [14 00 00 00]   height = 20
offset 12: [?? ?? ?? ??]   unused
```

## Mixed-Type Payloads

When variants have different-sized types, the payload is still sized to the largest:

```cst
enum Value {
    Byte as u8;     // tag=0, payload: 1 byte
    Int as i64;     // tag=1, payload: 8 bytes
    Pair as i32, i32; // tag=2, payload: 8 bytes
}
// Max payload = 8, sizeof(Value) = 12
```

## Payload Offset

The payload starts immediately after the tag (4 bytes). Since Caustic structs are packed, there is no alignment padding between the tag and payload:

```
tag at offset 0    (4 bytes)
payload at offset 4 (max_payload bytes)
```

## Generic Enum Layout

Generic enums follow the same rules after monomorphization:

```cst
enum Option gen T {
    Some as T;
    None;
}

// Option gen i32:  tag(4) + i32(4) = 8 bytes
// Option gen i64:  tag(4) + i64(8) = 12 bytes
// Option gen *u8:  tag(4) + *u8(8) = 12 bytes
```

## Sizeof

Use `sizeof(EnumName)` to get the total size:

```cst
let is *u8 as raw = mem.galloc(sizeof(Shape));
let is *Shape as s = cast(*Shape, raw);
```
