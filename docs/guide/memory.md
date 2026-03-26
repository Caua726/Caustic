# Memory

## Pointers

Caustic uses typed pointers for all indirect memory access. There is no garbage collector and no runtime safety checks.

### Syntax

`*T` declares a pointer to type `T`.

```cst
let is *i32 as p;
let is *Point as sp;
let is **u8 as pp;       // pointer to pointer
```

### Generic Byte Pointer

`*u8` is the generic byte pointer, equivalent to `void*` in C. The heap allocator returns `*u8`, which you cast to the desired type.

```cst
let is *u8 as raw = mem.galloc(64);
let is *i32 as nums = cast(*i32, raw);
```

### Null Pointer

There is no `null` literal. Use `cast(*T, 0)` to create a null pointer.

```cst
let is *u8 as ptr = cast(*u8, 0);
```

Check for null by casting back to integer:

```cst
if (cast(i64, ptr) == 0) {
    // ptr is null
}
```

### Pointer Size

All pointers are 8 bytes (64-bit addresses) regardless of the pointed-to type.

### Passing Pointers to Functions

```cst
fn fill(buf as *u8, len as i64) as void {
    // write to buf[0..len]
}

let is [32]u8 as data;
fill(&data[0], 32);
```

## Address-Of

The `&` operator returns a pointer to a variable, array element, or struct field.

### Local Variables

```cst
let is i32 as x = 42;
let is *i32 as p = &x;
// p now points to x on the stack
```

The result type is `*T` where `T` is the type of the operand.

### Array Elements

```cst
let is [10]i32 as arr;
let is *i32 as first = &arr[0];
let is *i32 as fifth = &arr[4];
```

This is how you pass arrays to functions -- take the address of element 0:

```cst
fn sum(data as *i32, len as i64) as i32 { ... }

let is [5]i32 as nums;
sum(&nums[0], 5);
```

### Struct Fields

```cst
struct Point { x as i32; y as i32; }

let is Point as p;
let is *i32 as px = &p.x;
let is *i32 as py = &p.y;
```

### Restrictions

- Cannot take the address of global variables (they use RIP-relative addressing internally).
- Cannot take the address of a literal or expression result -- only named storage locations.

## Dereferencing

The `*` operator reads or writes through a pointer.

### Reading

```cst
let is i32 as x = 10;
let is *i32 as p = &x;
let is i32 as val = *p;   // val == 10
```

### Writing

```cst
let is i32 as x = 10;
let is *i32 as p = &x;
*p = 20;                   // x is now 20
```

### Struct Pointer Access

Caustic automatically dereferences struct pointers when accessing fields. There is no `->` operator.

```cst
struct Point { x as i32; y as i32; }

let is Point as pt;
pt.x = 5;

let is *Point as p = &pt;
p.x = 10;    // automatic dereference, same as (*p).x
let is i32 as val = p.y;  // reads through pointer
```

### Chained Access

```cst
struct Node { value as i32; next as *u8; }

let is *Node as n = cast(*Node, mem.galloc(sizeof(Node)));
n.value = 42;
n.next = cast(*u8, 0);

// follow the chain
let is *Node as next = cast(*Node, n.next);
```

### Works With Any Pointer Type

```cst
let is *u8 as bytes = mem.galloc(8);
*bytes = 0;

let is *i64 as nums = cast(*i64, bytes);
*nums = 123456;
```

## Pointer Arithmetic

Caustic does not have built-in pointer arithmetic operators (`ptr + n`). Instead, cast pointers to integers, compute the offset in bytes, and cast back.

### Basic Pattern

```cst
// advance ptr by N bytes
let is *u8 as next = cast(*u8, cast(i64, ptr) + N);
```

### Typed Element Access

For arrays of a known type, multiply the index by the element size:

```cst
// access element at index i in an i32 array
let is *i32 as base = cast(*i32, mem.galloc(40));  // 10 i32s
let is i64 as i = 3;
let is *i32 as elem = cast(*i32, cast(i64, base) + i * 4);
*elem = 99;
```

### Struct Array Traversal

```cst
struct Item { id as i32; val as i64; }

let is *u8 as buf = mem.galloc(cast(i64, sizeof(Item)) * 10);
let is i64 as i = 0;
while (i < 10) {
    let is *Item as it = cast(*Item, cast(i64, buf) + i * cast(i64, sizeof(Item)));
    it.id = cast(i32, i);
    it.val = i * 100;
    i = i + 1;
}
```

### Byte-Level Access

```cst
fn read_byte(base as *u8, offset as i64) as u8 {
    let is *u8 as p = cast(*u8, cast(i64, base) + offset);
    return *p;
}
```

### Important Notes

- All offsets are in **bytes**, not elements.
- Caustic structs are **packed** (no padding). A `{i32, i64}` struct is 12 bytes, not 16.
- Use `sizeof(T)` to get the correct element size.

## Arrays

Caustic supports fixed-size arrays allocated on the stack.

### Declaration

```cst
let is [N]T as name;
```

`N` must be a compile-time integer constant. `T` can be any type.

```cst
let is [64]u8 as buffer;
let is [10]i32 as numbers;
let is [4]f64 as coords;
```

### Indexing

Zero-based, using `arr[i]`:

```cst
let is [5]i32 as a;
a[0] = 10;
a[1] = 20;
let is i32 as sum = a[0] + a[1];
```

There is **no bounds checking**. Out-of-bounds access silently reads or writes wrong memory.

### Array Size

`sizeof([N]T)` equals `N * sizeof(T)`.

```cst
// [64]u8 = 64 bytes
// [10]i32 = 40 bytes
// [4]i64 = 32 bytes
```

### Passing Arrays to Functions

Pass a pointer to the first element and the length:

```cst
fn print_nums(data as *i32, len as i64) as void {
    let is i64 as i = 0;
    while (i < len) {
        let is *i32 as elem = cast(*i32, cast(i64, data) + i * 4);
        // use *elem
        i = i + 1;
    }
}

let is [5]i32 as nums;
nums[0] = 1; nums[1] = 2; nums[2] = 3;
print_nums(&nums[0], 3);
```

### Character Buffers

Byte arrays are commonly used as string buffers:

```cst
let is [256]u8 as buf;
buf[0] = 72;   // 'H'
buf[1] = 105;  // 'i'
buf[2] = 10;   // newline
syscall(1, 1, &buf[0], 3);  // write to stdout
```

### Limitations

- Size must be a compile-time constant (no `let is [n]T`).
- No array literals or initializer syntax.
- Stack-allocated only for locals. Use heap for dynamic sizes.

## Array of Structs

Arrays can hold struct values, stored contiguously in memory.

### Stack-Allocated

```cst
struct Point { x as i32; y as i32; }

let is [10]Point as points;
points[0].x = 1;
points[0].y = 2;
points[3].x = 10;
```

Elements are packed end-to-end. For `Point` (two i32 fields = 8 bytes), `points[3]` starts at byte offset `3 * 8 = 24`.

### Heap-Allocated

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

### Packed Layout

Caustic structs have **no padding**. Fields are packed sequentially.

```cst
struct Mixed { a as i32; b as i64; }
// sizeof(Mixed) = 12 (not 16)
// Mixed.a at offset 0, Mixed.b at offset 4
```

This differs from C, where the same struct would be 16 bytes due to alignment. Keep this in mind when computing element addresses manually.

### Accessing Nested Fields

```cst
struct Rect { origin as Point; size as Point; }

let is [5]Rect as rects;
rects[0].origin.x = 0;
rects[0].origin.y = 0;
rects[0].size.x = 100;
rects[0].size.y = 50;
```

## Heap Allocation

Caustic provides a free-list heap allocator in `std/mem.cst`, backed by the `mmap` syscall. No libc required.

### Setup

Import the module and initialize the global heap before any allocation:

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(1048576);  // 1 MB heap
    // ... allocations here ...
    return 0;
}
```

`gheapinit` calls `mmap` to reserve the requested bytes. Call it once, early in `main`.

### Allocate and Free

```cst
let is *u8 as buf = mem.galloc(256);
// use buf...
mem.gfree(buf);
```

- `galloc(size)` returns `*u8`. Returns null (`cast(i64, ptr) == 0`) if out of memory.
- `gfree(ptr)` returns the block to the free list for reuse.

### Typed Allocation

Cast the result to the desired pointer type:

```cst
struct Node { value as i64; next as *u8; }

let is *Node as n = cast(*Node, mem.galloc(sizeof(Node)));
n.value = 42;
n.next = cast(*u8, 0);
// ...
mem.gfree(cast(*u8, n));
```

### Auto-Free With Defer

Use `defer` to ensure memory is freed before the function returns:

```cst
fn process() as i32 {
    let is *u8 as data = mem.galloc(4096);
    defer mem.gfree(data);

    // ... work with data ...
    if (error) {
        return 1;   // gfree(data) runs here
    }
    return 0;       // gfree(data) runs here too
}
```

### Per-Heap API

For isolated heaps (e.g., per-subsystem memory):

```cst
let is *u8 as heap = mem.reserve(65536);     // reserve 64 KB
let is *u8 as a = mem.alloc(heap, 128);      // allocate from this heap
let is *u8 as b = mem.alloc(heap, 256);
mem.free(heap, a);                           // free to this heap's list
mem.release(heap);                           // release entire heap (munmap)
```

### Null Check

Always check allocations that might fail:

```cst
let is *u8 as ptr = mem.galloc(size);
if (cast(i64, ptr) == 0) {
    // allocation failed
    syscall(60, 1);  // exit
}
```

### Sizing Guidelines

- Small programs: 64 KB -- 256 KB
- Compiler-sized programs: 1 MB -- 8 MB
- `gheapinit` size is the total available heap; the allocator cannot grow beyond it.

## Stack vs Heap

Caustic has two memory regions for program data. There is no garbage collector -- all heap memory must be freed manually.

### Stack

Local variables, arrays, and structs declared inside functions live on the stack.

```cst
fn example() as void {
    let is i32 as x = 10;        // stack
    let is [64]u8 as buf;        // stack (64 bytes)
    let is Point as p;           // stack
}
// x, buf, p are gone after return
```

**Properties:**
- Automatic: allocated on function entry, freed on return.
- Fast: just a pointer bump (rsp adjustment).
- Limited: default Linux stack is ~8 MB. Deep recursion or huge arrays can overflow.
- Scoped: data is invalid after the function returns.

### Heap

Heap memory is explicitly allocated and freed via `std/mem.cst`.

```cst
use "std/mem.cst" as mem;

fn create_buffer(size as i64) as *u8 {
    let is *u8 as buf = mem.galloc(size);
    return buf;   // valid after return
}

fn main() as i32 {
    mem.gheapinit(1048576);
    let is *u8 as data = create_buffer(4096);
    // use data...
    mem.gfree(data);
    return 0;
}
```

**Properties:**
- Manual: you allocate and free.
- Persistent: survives function returns, available until freed.
- Larger: limited only by `gheapinit` size and system memory.
- Slower: free-list search on each allocation.

### When to Use Which

| Use stack when... | Use heap when... |
|---|---|
| Size is known at compile time | Size determined at runtime |
| Data fits in a few KB | Data is large (> ~4 KB) |
| Data is local to one function | Data must outlive the function |
| Performance is critical | Lifetime flexibility is needed |

### Common Pattern: Stack Buffer, Heap Fallback

```cst
fn read_line() as *u8 {
    let is [256]u8 as tmp;
    let is i64 as n = syscall(0, 0, &tmp[0], 255);  // read into stack buf
    let is *u8 as result = mem.galloc(n + 1);        // copy to heap
    // copy tmp[0..n] into result
    return result;  // caller must gfree
}
```

### Memory Leaks

Forgetting to call `gfree` leaks memory. Use `defer` to pair allocation with cleanup:

```cst
let is *u8 as ptr = mem.galloc(1024);
defer mem.gfree(ptr);
// ptr is freed no matter which return path is taken
```
