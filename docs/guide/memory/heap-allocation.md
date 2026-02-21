# Heap Allocation

Caustic provides a free-list heap allocator in `std/mem.cst`, backed by the `mmap` syscall. No libc required.

## Setup

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

## Allocate and Free

```cst
let is *u8 as buf = mem.galloc(256);
// use buf...
mem.gfree(buf);
```

- `galloc(size)` returns `*u8`. Returns null (`cast(i64, ptr) == 0`) if out of memory.
- `gfree(ptr)` returns the block to the free list for reuse.

## Typed Allocation

Cast the result to the desired pointer type:

```cst
struct Node { value as i64; next as *u8; }

let is *Node as n = cast(*Node, mem.galloc(sizeof(Node)));
n.value = 42;
n.next = cast(*u8, 0);
// ...
mem.gfree(cast(*u8, n));
```

## Auto-Free With Defer

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

## Per-Heap API

For isolated heaps (e.g., per-subsystem memory):

```cst
let is *u8 as heap = mem.reserve(65536);     // reserve 64 KB
let is *u8 as a = mem.alloc(heap, 128);      // allocate from this heap
let is *u8 as b = mem.alloc(heap, 256);
mem.free(heap, a);                           // free to this heap's list
mem.release(heap);                           // release entire heap (munmap)
```

## Null Check

Always check allocations that might fail:

```cst
let is *u8 as ptr = mem.galloc(size);
if (cast(i64, ptr) == 0) {
    // allocation failed
    syscall(60, 1);  // exit
}
```

## Sizing Guidelines

- Small programs: 64 KB -- 256 KB
- Compiler-sized programs: 1 MB -- 8 MB
- `gheapinit` size is the total available heap; the allocator cannot grow beyond it.
