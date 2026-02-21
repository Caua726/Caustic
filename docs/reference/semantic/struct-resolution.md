# Struct Resolution

Struct resolution computes the memory layout of all struct types: field offsets and total size. This happens during the `resolve_fields` pass, before function bodies are analyzed.

## Packed Layout

Caustic structs use **packed layout** with no alignment padding. Each field is placed immediately after the previous field, regardless of its alignment requirements.

```cst
struct Example {
    a as i8;     // offset 0, size 1
    b as i64;    // offset 1, size 8   (no padding!)
    c as i32;    // offset 9, size 4
}
// total size = 13 bytes
```

This differs from C, where `Example` would be 24 bytes due to alignment padding. When interfacing with C code via `extern fn`, struct layouts must be manually matched by inserting padding fields.

## Field Offset Computation

The `resolve_fields` pass iterates through each struct declaration and computes offsets:

```
offset = 0
for each field in struct.fields:
    field.offset = offset
    offset = offset + sizeof(field.type)
struct.size = offset
```

Fields are stored as a linked list of nodes. Each field node has its `offset` value set during this pass.

## Nested Structs

When a struct contains a field of another struct type, the inner struct must be resolved first. The `resolve_fields` pass handles this by processing imports recursively (module bodies are visited before the current file's structs):

```cst
struct Point {
    x as i32;    // offset 0, size 4
    y as i32;    // offset 4, size 4
}
// size = 8

struct Rect {
    origin as Point;   // offset 0, size 8 (inline, packed)
    width as i32;      // offset 8, size 4
    height as i32;     // offset 12, size 4
}
// size = 16
```

## Generic Template Structs

Structs with generic parameters (`generic_param_count > 0`) are **skipped** during `resolve_fields`. Their fields contain unresolved type parameters that have no concrete size.

Resolution happens later, when the struct is instantiated with concrete types. At that point, the cloned struct goes through `resolve_fields` with all type parameters replaced by concrete types:

```cst
struct Wrapper gen T {
    value as T;
    count as i32;
}

// Skipped during initial resolve_fields.
// When Wrapper gen i64 is used:
//   Wrapper_i64 { value as i64 (offset 0, size 8); count as i32 (offset 8, size 4) }
//   size = 12
```

## Field Lookup

At compile time, member access (`obj.field_name`) resolves fields by linear scan of the struct's field linked list:

```
fn lookup_field(struct_type, name):
    field = struct_type.fields
    while field != null:
        if field.name == name:
            return field
        field = field.next
    error("no such field")
```

The resolved field provides both the type (for type checking) and the offset (for code generation).

## Struct Size Access

The `sizeof()` expression on a struct type returns the total packed size:

```cst
struct Point { x as i32; y as i32; }

let is i64 as s = sizeof(Point);   // s = 8
```

## Common Pitfalls

### Packed Layout Surprises

Because there is no padding, mixing field sizes produces layouts that differ from C:

```cst
struct Mixed {
    a as i64;    // offset 0, size 8
    b as i32;    // offset 8, size 4
    c as i64;    // offset 12, size 8
}
// size = 20 (not 24 as in C)
```

For safety when field ordering matters (e.g., matching hardware layouts or C FFI), use uniformly-sized fields:

```cst
struct SafeLayout {
    a as i64;    // offset 0
    b as i64;    // offset 8   (use i64 even if only i32 needed)
    c as i64;    // offset 16
}
// size = 24, matches C with natural alignment
```

### Cross-Module Struct Fields

Using a struct type from another module as a field type (`field as mod.StructType`) is not supported. Workarounds:

- Use `*u8` pointers with accessor functions that cast internally.
- Flatten the struct's fields into the parent struct.
