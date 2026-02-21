# std/compatcffi.cst -- C FFI Compatibility

`std/compatcffi.cst` provides tools for building and accessing C-compatible memory layouts from Caustic code. Its primary purpose is to bridge the gap between Caustic's packed structs (no alignment padding) and C's padded struct layout, which matters whenever you call `extern fn` functions that expect C structs.

## Why This Exists

Caustic structs are packed: fields are placed one after another with no gaps. C structs obey alignment rules (each field starts at a multiple of its alignment). When calling a C function that expects a pointer to a `struct sockaddr_in`, for example, a native Caustic struct will have the wrong layout.

`compatcffi.cst` lets you describe a C struct's layout at runtime, allocate a correctly-sized buffer, and read or write each field through typed getter/setter functions that apply the right byte offset.

## Dependencies

```cst
use "std/compatcffi.cst" as cffi;
use "std/mem.cst" as mem;   // required: must call mem.gheapinit before using cffi
```

---

## C Type Size Constants

All sizes are for x86_64 Linux:

```cst
let is i64 as C_CHAR_SIZE   with imut = 1;
let is i64 as C_SHORT_SIZE  with imut = 2;
let is i64 as C_INT_SIZE    with imut = 4;
let is i64 as C_LONG_SIZE   with imut = 8;
let is i64 as C_PTR_SIZE    with imut = 8;
let is i64 as C_FLOAT_SIZE  with imut = 4;
let is i64 as C_DOUBLE_SIZE with imut = 8;
let is i64 as C_SIZE_T_SIZE with imut = 8;
```

Corresponding alignment constants (`C_CHAR_ALIGN`, `C_INT_ALIGN`, etc.) follow the same pattern: alignment equals size for all primitive C types on x86_64.

---

## CStruct

`CStruct` describes a C struct layout. Fields are added sequentially; `finish()` computes total size with tail padding.

### Lifecycle

```cst
fn new_struct() as CStruct;
fn finish(cs as *CStruct) as void;
fn free_struct(cs as *CStruct) as void;
```

- `new_struct()` -- allocates field metadata (initial capacity: 8 fields).
- `finish()` -- adds tail padding so `total_size` is a multiple of `max_align`. Must be called before `alloc`, `offset`, or `size`.
- `free_struct()` -- frees internal field metadata. Does **not** free instances allocated with `alloc`.

### Adding Fields

```cst
fn add_i8(cs as *CStruct) as void;    // char, signed char
fn add_i16(cs as *CStruct) as void;   // short
fn add_i32(cs as *CStruct) as void;   // int
fn add_i64(cs as *CStruct) as void;   // long, long long, size_t
fn add_f32(cs as *CStruct) as void;   // float
fn add_f64(cs as *CStruct) as void;   // double
fn add_ptr(cs as *CStruct) as void;   // any pointer type
```

Each call appends one field at the correct aligned offset. The offset is computed automatically based on previous fields.

Array fields:

```cst
fn add_array_i8(cs as *CStruct, count as i64) as void;
fn add_array_i32(cs as *CStruct, count as i64) as void;
fn add_array_i64(cs as *CStruct, count as i64) as void;
// ... same pattern for i16, f32, f64, ptr
```

Nested struct / custom:

```cst
fn add_struct(cs as *CStruct, inner as *CStruct) as void;
fn add_bytes(cs as *CStruct, fsize as i64, falign as i64) as void;
```

### Instance Management

```cst
fn alloc(cs as *CStruct) as *u8;
fn offset(cs as *CStruct, idx as i64) as i64;
fn size(cs as *CStruct) as i64;
```

- `alloc()` -- allocates a zero-initialized buffer of `total_size` bytes. Returns `*u8`.
- `offset(cs, idx)` -- returns the byte offset of field `idx` (0-based) from the struct base.
- `size(cs)` -- returns `total_size` (same as `cs.total_size`).

### Scalar Getters / Setters

```cst
fn set_i32(buf as *u8, cs as *CStruct, idx as i64, val as i32) as void;
fn get_i32(buf as *u8, cs as *CStruct, idx as i64) as i32;

fn set_i64(buf as *u8, cs as *CStruct, idx as i64, val as i64) as void;
fn get_i64(buf as *u8, cs as *CStruct, idx as i64) as i64;

fn set_ptr(buf as *u8, cs as *CStruct, idx as i64, val as *u8) as void;
fn get_ptr(buf as *u8, cs as *CStruct, idx as i64) as *u8;
// also: i8, i16, f32, f64 variants
```

`buf` is the instance pointer from `alloc`. `idx` is the 0-based field index. The getter/setter computes the byte offset internally via `offset(cs, idx)`.

### Array Element Getters / Setters

```cst
fn set_array_i8(buf as *u8, cs as *CStruct, field_idx as i64, arr_idx as i64, val as i32) as void;
fn get_array_i8(buf as *u8, cs as *CStruct, field_idx as i64, arr_idx as i64) as i32;
// ... same pattern for i16, i32, i64, f32, f64, ptr
```

`field_idx` identifies the array field; `arr_idx` is the 0-based element within that array.

---

## CUnion

`CUnion` describes a C union. All variants share the same memory at offset 0. `total_size` is the largest variant's size, padded to `max_align`.

```cst
fn new_union() as CUnion;
fn union_add_i8(u as *CUnion) as void;
fn union_add_i32(u as *CUnion) as void;
fn union_add_i64(u as *CUnion) as void;
fn union_add_ptr(u as *CUnion) as void;
// ... i16, f32, f64 variants
fn union_add_variant(u as *CUnion, vsize as i64, valign as i64) as void;
fn union_finish(u as *CUnion) as void;
fn union_alloc(u as *CUnion) as *u8;
fn union_free(u as *CUnion) as void;
fn union_size(u as *CUnion) as i64;

// Getters/setters all operate at offset 0:
fn union_set_i32(buf as *u8, u as *CUnion, variant as i64, val as i32) as void;
fn union_get_i32(buf as *u8, u as *CUnion, variant as i64) as i32;
// ... same pattern for i8, i16, i64, f32, f64, ptr
```

The `variant` parameter is accepted for API consistency but currently has no effect on the offset.

---

## C String Conversion

```cst
fn to_cstr(s as string.String) as *u8;
fn from_cstr(cstr as *u8) as string.String;
fn from_cstr_len(cstr as *u8, len as i64) as string.String;
```

- `to_cstr` -- returns the underlying `*u8` of a Caustic `String`. Zero-cost; pointer remains owned by the original `String`.
- `from_cstr` -- allocates a new `String` by copying a null-terminated C string. Caller must free with `string.string_free`.
- `from_cstr_len` -- same but for a buffer of known length (not necessarily null-terminated).

---

## Typed Pointer Arithmetic

For working with C arrays or arbitrary raw buffers:

```cst
fn ptr_add(ptr as *u8, index as i64, elem_size as i64) as *u8;

fn ptr_get_i32(ptr as *u8, index as i64) as i32;
fn ptr_set_i32(ptr as *u8, index as i64, val as i32) as void;
fn ptr_get_i64(ptr as *u8, index as i64) as i64;
fn ptr_set_i64(ptr as *u8, index as i64, val as i64) as void;
fn ptr_get_ptr(ptr as *u8, index as i64) as *u8;
fn ptr_set_ptr(ptr as *u8, index as i64, val as *u8) as void;
// also: i8, i16, f32, f64 variants
```

`index` is an element index, not a byte offset. The stride is determined by the type suffix (1 for i8, 2 for i16, 4 for i32/f32, 8 for i64/f64/ptr).

---

## When to Use

Use `compatcffi.cst` whenever:

- Calling an `extern fn` that accepts a pointer to a C struct with mixed field sizes (e.g., `int` + `long`, `short` + `int`).
- Calling a C function that returns a struct whose layout differs from what Caustic would produce.
- Accessing C arrays of structs by index.
- Interoperating with POSIX APIs (`sockaddr_in`, `stat`, `iovec`, `pollfd`, etc.).

Do **not** use it for all-i64 structs or structs whose fields happen to be packed identically in C -- a plain Caustic struct will work for those.

---

## Example: sockaddr_in

```cst
use "std/compatcffi.cst" as cffi;
use "std/mem.cst" as mem;

fn setup_addr() as *u8 {
    // struct sockaddr_in { short sin_family; u16 sin_port; u32 sin_addr; char[8] pad; }
    let is cffi.CStruct as layout = cffi.new_struct();
    cffi.add_i16(&layout);          // field 0: sin_family
    cffi.add_i16(&layout);          // field 1: sin_port
    cffi.add_i32(&layout);          // field 2: sin_addr
    cffi.add_array_i8(&layout, 8);  // field 3: sin_zero[8]
    cffi.finish(&layout);

    let is *u8 as buf = cffi.alloc(&layout);
    cffi.set_i16(buf, &layout, 0, 2);      // AF_INET
    cffi.set_i16(buf, &layout, 1, 8080);   // port
    cffi.set_i32(buf, &layout, 2, 0);      // INADDR_ANY
    return buf;
}
```
