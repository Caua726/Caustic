# Enum Types

Enums in Caustic are tagged unions: each value carries a discriminant tag identifying which variant is active, followed by variant-specific payload data.

## Type Kind

Enums use `TY_ENUM` (kind value 17). The `Type` struct stores:

```
Type {
    kind: TY_ENUM (17)
    name_ptr: *u8           -- enum name
    name_len: i64           -- length of name
    size: i64               -- total size (tag + max_payload)
    fields: *Node           -- linked list of variants
    field_count: i64        -- synonym for variant_count in practice
    variant_count: i64      -- number of variants
    payload_offset: i64     -- byte offset where payload begins (after tag)
    max_payload: i64        -- size of largest variant's payload
    gen_params: ...         -- generic parameters (if template)
    gen_param_count: i64
    gen_args: ...
    gen_arg_count: i64
}
```

## Declaration

### Simple Enums (No Payload)

```cst
enum Color {
    Red;
    Green;
    Blue;
}
```

Simple enums have only a tag (4 bytes) and no payload. Each variant is assigned a discriminant value starting from 0:

- `Red` = 0
- `Green` = 1
- `Blue` = 2

Total size: 4 bytes (tag only, `max_payload` = 0).

### Enums with Payload

Variants can carry data:

```cst
enum Shape {
    Circle(radius as f64);
    Rect(width as f64, height as f64);
    Point;
}
```

The compiler determines the maximum payload size across all variants:

- `Circle`: payload = 8 bytes (one f64)
- `Rect`: payload = 16 bytes (two f64)
- `Point`: payload = 0 bytes

`max_payload` = 16 bytes. Total size = 4 (tag) + 16 (max payload) = 20 bytes.

All variants occupy the same total size, regardless of their individual payload size. Unused payload space is simply ignored.

## Memory Layout

```
Offset 0:                  Tag (i32, 4 bytes)
Offset payload_offset:     Payload data (max_payload bytes)
```

The `payload_offset` is the byte offset where payload data begins. For standard enums, this equals the tag size (4 bytes).

Example layout for `Shape`:

```
Bytes 0-3:    tag (i32)
Bytes 4-19:   payload (16 bytes max)

Circle:  [0x00000000] [radius: f64] [unused: 8 bytes]
Rect:    [0x00000001] [width: f64] [height: f64]
Point:   [0x00000002] [unused: 16 bytes]
```

## Usage

### Construction

```cst
let is Shape as s = Shape.Circle(3.14);
let is Color as c = Color.Red;
```

### Match Expressions

Pattern matching is used to inspect enum values and extract payload data:

```cst
match s {
    Circle(r) => {
        // r is f64, bound to the radius
    }
    Rect(w, h) => {
        // w and h are f64
    }
    Point => {
        // no payload to bind
    }
}
```

The `match` expression checks the tag to determine which variant is active, then binds payload fields to local variables in the matched arm.

## Discriminant Values

Variants are assigned sequential integer values starting from 0, in declaration order:

```cst
enum Status {
    Pending;    // 0
    Active;     // 1
    Closed;     // 2
}
```

The discriminant is stored as an `i32` (4 bytes) at the beginning of the enum value.

## Size Calculation

```
enum.size = payload_offset + max_payload
```

For enums with no payload variants, `max_payload` is 0, so the size is just the tag size (4 bytes).

```cst
sizeof(Color)    // 4  (tag only)
sizeof(Shape)    // 20 (4 + 16)
```

## Generic Enums

Enums support generic parameters:

```cst
enum Option gen T {
    Some(value as T);
    None;
}

enum Result gen T, E {
    Ok(value as T);
    Err(error as E);
}
```

Generic enums are monomorphized like generic structs:

```cst
let is Option gen i32 as x = Option gen i32 .Some(42);
// Produces concrete type: Option_i32
// size = 4 (tag) + 4 (i32 payload) = 8 bytes
```

## Variant Storage

Variants are stored as the enum's `fields` linked list. Each variant node contains:

- The variant name
- A discriminant value (sequential integer)
- Payload field declarations (if any)

The `variant_count` field tracks the total number of variants.

## Limitations

- **No associated methods on enum variants**: Impl blocks apply to the enum type, not individual variants.
- **Exhaustive match**: The compiler may or may not enforce exhaustive matching depending on implementation status. Unmatched variants can cause undefined behavior.
- **Fixed tag size**: The tag is always `i32` (4 bytes), even for enums with very few variants.
- **No explicit discriminant values**: Discriminants are always sequential starting from 0. Custom discriminant values are not supported.
