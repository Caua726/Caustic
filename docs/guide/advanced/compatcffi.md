# C Interop Types (compatcffi)

The `std/compatcffi.cst` module provides helpers for building C-compatible struct layouts with proper alignment padding.

## Import

```cst
use "std/compatcffi.cst" as cffi;
```

## Building C-Compatible Struct Layouts

Caustic structs are packed (no padding), but C structs have alignment padding. `CStruct` lets you describe a C struct layout and access fields correctly.

### Define a Layout

```cst
// Equivalent to C: struct { int a; double b; }
let is cffi.CStruct as cs = cffi.new_struct();
cffi.add_i32(&cs);   // field 0: int (4 bytes + 4 padding)
cffi.add_f64(&cs);   // field 1: double (8 bytes)
cffi.finish(&cs);
// Total size: 16 bytes (matching C layout)
```

### Allocate and Access Fields

```cst
use "std/mem.cst" as mem;

let is *u8 as buf = mem.galloc(cffi.struct_size(&cs));

// Set field 0 (i32) to 42
cffi.set_i32(buf, &cs, 0, 42);

// Set field 1 (f64) to 3.14
cffi.set_f64(buf, &cs, 1, 3.14);

// Read back
let is i32 as a = cffi.get_i32(buf, &cs, 0);
let is f64 as b = cffi.get_f64(buf, &cs, 1);
```

## C Union Layouts

```cst
let is cffi.CUnion as cu = cffi.new_union();
cffi.union_add_i64(&cu);
cffi.union_add_f64(&cu);
cffi.union_finish(&cu);
// Size = max(sizeof(i64), sizeof(f64)) = 8
```

## C String Helpers

```cst
// Convert Caustic string to C string (null-terminated, allocated)
let is *u8 as cstr = cffi.to_cstr("hello");

// Convert C string back (reads until null byte)
let is *u8 as s = cffi.from_cstr(cstr);
```

## Typed Pointer Arithmetic

```cst
// Advance pointer by index * element_size
let is *u8 as base = mem.galloc(40);
let is *u8 as third = cffi.ptr_add(base, 2, sizeof(i64));
// third points to base + 16
```

## Complete Example: Calling C with a Struct

```cst
use "std/compatcffi.cst" as cffi;
use "std/mem.cst" as mem;

extern fn process_point(p as *u8) as i32;

fn main() as i32 {
    // Build layout for C struct { int x; int y; double z; }
    let is cffi.CStruct as cs = cffi.new_struct();
    cffi.add_i32(&cs);
    cffi.add_i32(&cs);
    cffi.add_f64(&cs);
    cffi.finish(&cs);

    let is *u8 as buf = mem.galloc(cffi.struct_size(&cs));
    defer mem.gfree(buf);

    cffi.set_i32(buf, &cs, 0, 10);
    cffi.set_i32(buf, &cs, 1, 20);
    cffi.set_f64(buf, &cs, 2, 3.14);

    let is i32 as result = process_point(buf);
    return result;
}
```
