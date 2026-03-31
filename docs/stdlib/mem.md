## _module
mem — Manual Memory Allocation

Caustic has no garbage collector. You manage memory yourself.
mem gives you 5 different allocators — pick the right one for your use case:

  mem.bins     — Use this by default. Fast O(1) slab allocator for mixed sizes.
  mem.arena    — Fastest. Allocates linearly, frees everything at once. Great for
                 temporary work like parsing or building a response.
  mem.pool     — Fixed-size slots. Use when you need many objects of the same size
                 (linked list nodes, tree nodes, etc.)
  mem.freelist — Traditional allocator. Handles any size, coalesces adjacent frees.
                 Slower but most flexible.
  mem.lifo     — Stack allocator. Must free in reverse order. Extremely fast.

Also provides low-level primitives via mem.core:
  memcpy(dst, src, n)  — copy bytes (regions must not overlap)
  memset(dst, c, n)    — fill memory with a byte value
  memcmp(a, b, n)      — compare memory (0 if equal)
  memzero(dst, n)      — zero out memory

How to use:
  use "std/mem.cst" as mem;
  // or selectively:
  use "std/mem.cst" only bins, core as mem;

  // Allocate with bins (most common):
  let is mem.Bins as heap = mem.bins.bins_new(131072);
  let is *u8 as ptr = mem.bins.bins_alloc(&heap, 64);
  mem.bins.bins_free(&heap, ptr);

  // Allocate with arena (fastest for batch work):
  let is mem.Arena as a = mem.arena.create(65536);
  let is *u8 as p = mem.arena.alloc(&a, 128);
  mem.arena.destroy(&a);  // frees everything at once
---
## memcpy
fn memcpy(dst as *u8, src as *u8, n as i64) as *u8

Copy n bytes from src to dst. Regions must not overlap.

This is the fundamental memory copy operation. Equivalent to C's memcpy.

Parameters:
  dst - destination buffer
  src - source buffer
  n   - number of bytes to copy

Returns: dst pointer.

Example:
  mem.memcpy(dest, source, 64);
---
## memset
fn memset(dst as *u8, c as i32, n as i64) as *u8

Fill n bytes of dst with the byte value c.

Example:
  mem.memset(buf, 0, 4096);  // zero out buffer
---
## memcmp
fn memcmp(a as *u8, b as *u8, n as i64) as i32

Compare n bytes of memory.

Returns: 0 if equal, negative if a < b, positive if a > b.

Example:
  if (mem.memcmp(buf1, buf2, 16) == 0) { /* equal */ }
---
## memzero
fn memzero(dst as *u8, n as i64) as void

Zero out n bytes of memory. Shorthand for memset(dst, 0, n).
---
## bins
Submodule: mem/bins.cst — O(1) slab-based allocator

The fastest general-purpose allocator. Uses size classes (bins) for
O(1) allocation and deallocation. Best for mixed-size allocations.

Features:
  - O(1) alloc and free
  - Bitmap-based double-free detection
  - Size classes: 8, 16, 32, 64, 128, 256, 512, 1024, ... up to 65536 bytes

Usage:
  let is mem.Bins as b = mem.bins.bins_new(131072);  // 128KB initial heap
  let is *u8 as p = mem.bins.bins_alloc(&b, 64);     // allocate 64 bytes
  mem.bins.bins_free(&b, p);                          // free
  mem.bins.bins_destroy(&b);                          // release all memory
---
## core
Submodule: mem/core.cst — Low-level memory operations

Provides the fundamental memory primitives:
  memcpy(dst, src, n)  — copy bytes
  memset(dst, c, n)    — fill bytes
  memcmp(a, b, n)      — compare bytes
  memzero(dst, n)      — zero bytes

These are used by all other allocators and most stdlib modules.
---
## arena
Submodule: mem/arena.cst — O(1) bump allocator

The fastest allocator for temporary/batch allocations.
All memory freed at once with arena_destroy(). Individual frees are no-ops.

Perfect for: parsing, compilation passes, request handling.

Usage:
  let is mem.Arena as a = mem.arena.create(65536);  // 64KB arena
  let is *u8 as p = mem.arena.alloc(&a, 128);       // bump pointer
  mem.arena.destroy(&a);                             // free everything at once
---
## pool
Submodule: mem/pool.cst — O(1) fixed-size pool allocator

Allocates fixed-size slots from a pre-allocated block.
Best when you need many objects of the same size (linked list nodes, etc.)

Usage:
  let is mem.Pool as p = mem.pool.create(sizeof(Node), 1024, 0);
  let is *Node as n = cast(*Node, mem.pool.alloc(&p));
  mem.pool.pfree(&p, cast(*u8, n));
---
## freelist
Submodule: mem/freelist.cst — General-purpose allocator

Traditional free-list allocator with address-ordered coalescing.
O(n) alloc/free but handles any size. Best for long-lived allocations.

Usage:
  let is mem.Freelist as fl = mem.freelist.new(65536);
  let is *u8 as p = mem.freelist.alloc(&fl, 256);
  mem.freelist.free(&fl, p, 256);  // must pass original size
---
## lifo
Submodule: mem/lifo.cst — Stack-like allocator

LIFO (last-in, first-out) allocator. Frees must happen in reverse
allocation order. O(1) for both alloc and free.

Best for: nested/scoped allocations where you always free newest first.
---
