# std/string.cst -- Dynamic Strings

Heap-allocated dynamic string type with search, comparison, conversion, and slicing operations. All strings are null-terminated for compatibility with syscalls that expect C strings.

## Dependencies

```cst
use "mem.cst" as mem;
use "linux.cst" as linux;
```

## Struct

### String

```cst
struct String {
    ptr as *u8;
    len as i32;
    cap as i32;
}
```

Dynamic string. `ptr` points to a heap-allocated, null-terminated byte buffer. `len` is the number of characters (excluding the null terminator). `cap` is the allocated capacity (excluding the null terminator). A null/empty string has `ptr = cast(*u8, 0)` and `len = 0`.

---

## Functions

### strlen

```cst
fn strlen(s as *u8) as i64;
```

Return the length of a null-terminated C string. Counts bytes until the first zero byte.

**Example:**

```cst
let is i64 as n = string.strlen("hello"); // 5
```

### String (constructor)

```cst
fn String(text as *u8) as String;
```

Create a `String` from a null-terminated `*u8` literal or buffer. Allocates heap memory via `galloc` and copies the content.

**Optimization:** When used in an assignment (`s = string.String("new value")`), the constructor detects the target variable via a compiler intrinsic (`getbeforevalue` / register `r10`). If the target already has a buffer with sufficient capacity, the new content is copied in-place without allocating. If the old buffer is too small, it is freed and a new one is allocated.

**Example:**

```cst
let is string.String as s = string.String("hello");
s = string.String("hi"); // reuses buffer if cap >= 2
```

### string_free

```cst
fn string_free(s as String) as void;
```

Free the heap memory owned by a `String`. Safe to call on a string with null `ptr` (no-op).

**Example:**

```cst
let is string.String as s = string.String("temp");
string.string_free(s);
```

### concat

```cst
fn concat(s1 as String, s2 as String) as String;
```

Create a new `String` containing the concatenation of `s1` and `s2`. The result is independently heap-allocated. The caller must free it with `string_free`.

**Example:**

```cst
let is string.String as full = string.concat(
    string.String("hello "),
    string.String("world")
);
io.println(full);
string.string_free(full);
```

### int_to_string

```cst
fn int_to_string(n as i64) as String;
```

Convert a signed 64-bit integer to its decimal string representation. Handles negative numbers (prepends `'-'`). Allocates via `galloc`. Caller must free.

**Example:**

```cst
let is string.String as s = string.int_to_string(42);    // "42"
let is string.String as neg = string.int_to_string(0 - 7); // "-7"
```

### int_to_string_in

```cst
fn int_to_string_in(h as *mem.Heap, n as i64) as String;
```

Same as `int_to_string`, but allocates from a specific heap `h` instead of the global heap. Useful when building strings in a scratch heap that will be released in bulk.

**Example:**

```cst
let is *mem.Heap as scratch = mem.reserve(4096);
let is string.String as s = string.int_to_string_in(scratch, 100);
// ... use s ...
mem.release(scratch); // frees everything at once
```

### find

```cst
fn find(haystack as String, needle as String) as i32;
```

Find the first occurrence of `needle` in `haystack`. Returns the byte index of the match, or `-1` if not found. Returns `0` if `needle` is empty.

**Example:**

```cst
let is i32 as idx = string.find(
    string.String("hello world"),
    string.String("world")
); // 6
```

### find_from

```cst
fn find_from(haystack as String, needle as String, start_index as i32) as i32;
```

Same as `find`, but begins searching at `start_index`. Returns `-1` if not found or if `start_index` is out of bounds.

**Example:**

```cst
let is i32 as idx = string.find_from(
    string.String("abcabc"),
    string.String("abc"),
    1
); // 3
```

### substring_in

```cst
fn substring_in(h as *mem.Heap, s as String, start as i32, end as i32) as String;
```

Create a new `String` containing the slice `s[start..end)` (start inclusive, end exclusive). Allocates from heap `h`. Indices are clamped to valid bounds (no out-of-bounds crash). Returns an empty string if the resulting length is zero.

**Example:**

```cst
let is string.String as sub = string.substring_in(
    scratch_heap,
    string.String("hello world"),
    6, 11
); // "world"
```

### eq

```cst
fn eq(a as String, b as String) as i32;
```

Compare two strings for equality. Returns `1` if they have the same length and identical bytes, `0` otherwise.

**Example:**

```cst
if (string.eq(a, string.String("yes")) == 1) {
    // ...
}
```

### starts_with

```cst
fn starts_with(s as String, prefix as String) as i32;
```

Returns `1` if `s` begins with `prefix`, `0` otherwise.

**Example:**

```cst
string.starts_with(string.String("caustic"), string.String("cau")); // 1
```

### ends_with

```cst
fn ends_with(s as String, suffix as String) as i32;
```

Returns `1` if `s` ends with `suffix`, `0` otherwise.

### contains

```cst
fn contains(s as String, needle as String) as i32;
```

Returns `1` if `needle` appears anywhere in `s`, `0` otherwise.

### char_at

```cst
fn char_at(s as String, index as i32) as u8;
```

Return the byte at `index` in the string. Returns `0` if the index is out of bounds (negative or `>= s.len`).

**Example:**

```cst
let is u8 as c = string.char_at(s, 0); // first byte
```

### parse_int

```cst
fn parse_int(s as String) as i64;
```

Parse a decimal integer from a string. Handles an optional leading `'-'` for negative numbers. Stops at the first non-digit character. Returns `0` for empty or non-numeric strings.

**Example:**

```cst
let is i64 as n = string.parse_int(string.String("-42")); // -42
let is i64 as m = string.parse_int(string.String("100abc")); // 100
```

---

## Typical Usage

```cst
use "std/string.cst" as string;
use "std/mem.cst" as mem;
use "std/io.cst" as io;

fn main() as i32 {
    mem.gheapinit(1024 * 1024);

    let is string.String as greeting = string.String("Hello, ");
    let is string.String as name = string.String("Caustic");
    let is string.String as msg = string.concat(greeting, name);

    io.println(msg); // "Hello, Caustic"

    let is i32 as idx = string.find(msg, string.String("Caustic"));
    io.print_int(cast(i64, idx)); // 7

    string.string_free(msg);
    string.string_free(greeting);
    string.string_free(name);

    return 0;
}
```

## Notes

- Strings are **not** reference-counted. The caller is responsible for freeing every `String` returned by allocating functions (`String`, `concat`, `int_to_string`, `readline`, etc.).
- The `String` constructor optimization (buffer reuse) relies on a compiler intrinsic that reads register `r10`. This only works correctly in assignment contexts.
- `substring_in` requires a specific `*Heap` to allocate from. There is no `substring` variant that uses the global heap.
- All comparison functions (`eq`, `starts_with`, `ends_with`, `contains`) are byte-wise. There is no Unicode awareness.
