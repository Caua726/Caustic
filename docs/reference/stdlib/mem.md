# std/mem.cst -- Memory Management

Low-level heap allocator built on top of `mmap(2)`. Provides both instance-based and global heap APIs. No libc dependency -- all memory comes directly from Linux syscalls.

## Structs

### Header

```cst
struct Header {
    size as i64;
    next as *u8;
}
```

Free-list block header. Prepended to every allocation. `size` is the usable payload size in bytes. `next` points to the next free block (cast to `*u8` because Caustic does not support forward-referencing struct pointers).

### Heap

```cst
struct Heap {
    freelist as *Header;
    start    as *u8;
    top      as *u8;
    limit    as *u8;
}
```

Heap state for a single memory region. `freelist` is the head of the free-list chain. `start` is the base of the data region (immediately after the `Heap` struct in the mmap'd page). `top` is the current bump pointer. `limit` is one past the last usable byte.

## Global State

```cst
let is *Heap as _std_heap with mut;
```

Internal global heap pointer. Initialized by `gheapinit()`. Used by `galloc()` and `gfree()`. Do not access directly -- use the `g*` functions.

---

## Functions

### reserve

```cst
fn reserve(req_size as i64) as *Heap;
```

Allocate a new heap region of `req_size` bytes via `mmap`. The actual mmap allocation is `req_size + sizeof(Heap)` because the `Heap` struct is stored at the start of the mapped region. Returns a pointer to the initialized `Heap`, or a null pointer (`cast(*Heap, 0)`) if mmap fails.

The mapped region uses `PROT_READ | PROT_WRITE` and `MAP_PRIVATE | MAP_ANONYMOUS`.

**Example:**

```cst
let is *mem.Heap as h = mem.reserve(1024 * 1024); // 1 MB heap
```

### release

```cst
fn release(h as *Heap) as void;
```

Release the entire heap region back to the OS via `munmap`. Computes the total size from `h.limit - h`. Safe to call with a null pointer (no-op).

**Example:**

```cst
mem.release(h);
```

### alloc

```cst
fn alloc(h as *Heap, req_size as i64) as *u8;
```

Allocate `req_size` bytes from heap `h`. Allocation strategy:

1. **Free-list scan**: walks the free-list looking for a block with `size >= req_size`. If found and the remainder is large enough to hold a new `Header`, the block is split. Otherwise the entire block is used.
2. **Bump allocation**: if no free block is found, advances `h.top` by `sizeof(Header) + req_size`. Returns null (`cast(*u8, 0)`) if the bump would exceed `h.limit`.

Returns a pointer to the usable payload (past the `Header`).

**Example:**

```cst
let is *u8 as buf = mem.alloc(h, 256);
```

### free

```cst
fn free(h as *Heap, ptr as *u8) as void;
```

Return a previously allocated block to the free-list of heap `h`. The block header is located at `ptr - sizeof(Header)`. The freed block is prepended to the free-list. Safe to call with a null `ptr` or null `h` (both are no-ops).

**Example:**

```cst
mem.free(h, buf);
```

### gheapinit

```cst
fn gheapinit(size as i64) as void;
```

Initialize the global heap with `size` bytes. Calls `reserve(size)` internally and stores the result in the internal `_std_heap` pointer. If the global heap is already initialized, this is a no-op. Must be called before any `galloc` or `gfree` call.

**Example:**

```cst
mem.gheapinit(4 * 1024 * 1024); // 4 MB global heap
```

### galloc

```cst
fn galloc(size as i64) as *u8;
```

Allocate `size` bytes from the global heap. If the global heap has not been initialized (via `gheapinit`), prints an error to stderr and calls `exit(1)`.

**Example:**

```cst
let is *u8 as data = mem.galloc(512);
```

### gfree

```cst
fn gfree(ptr as *u8) as void;
```

Free a pointer previously returned by `galloc`. Delegates to `free(_std_heap, ptr)`.

**Example:**

```cst
mem.gfree(data);
```

### memcpy

```cst
fn memcpy(dst as *u8, src as *u8, n as i64) as *u8;
```

Copy `n` bytes from `src` to `dst`. Byte-by-byte copy; does not handle overlapping regions. Returns `dst`.

**Example:**

```cst
mem.memcpy(dest_buf, src_buf, 128);
```

### memset

```cst
fn memset(dst as *u8, c as i32, n as i64) as *u8;
```

Fill `n` bytes of `dst` with the byte value `c` (truncated to `u8`). Returns `dst`.

**Example:**

```cst
mem.memset(buf, 0, 1024); // zero-fill
```

### memcmp

```cst
fn memcmp(mem1 as *u8, mem2 as *u8, data_size as i64) as i32;
```

Compare `data_size` bytes of `mem1` and `mem2`. Returns 0 if equal. On the first differing byte, returns `mem1[i] - mem2[i]` (positive if `mem1` is greater, negative if less).

**Example:**

```cst
let is i32 as cmp = mem.memcmp(a, b, 64);
if (cmp == 0) {
    // buffers are equal
}
```

---

## Typical Usage

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(2 * 1024 * 1024); // 2 MB

    let is *u8 as buf = mem.galloc(256);
    mem.memset(buf, 0, 256);

    // ... use buf ...

    mem.gfree(buf);
    return 0;
}
```

## Notes

- There is no coalescing of adjacent free blocks. Repeated alloc/free cycles of varying sizes can fragment the heap.
- All allocations carry a 16-byte overhead (`sizeof(Header)` = `i64` + pointer).
- The bump allocator is one-directional. Once `top` advances, that space is only reclaimable through `free`.
- `reserve` uses `mmap` with `MAP_ANONYMOUS`, so the memory is zero-initialized by the kernel on first access.
