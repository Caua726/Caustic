# std/mem.cst -- Memory Management

Central memory management module. Acts as a hub that re-exports five specialized submodule allocators and provides backward-compatible global API wrappers.

```cst
use "std/mem.cst" as mem;
```

## Architecture

`mem.cst` imports five submodules and delegates all work to them:

```
mem.cst
 ├── mem/freelist.cst   (general-purpose free-list)
 ├── mem/bins.cst       (fixed-size slab allocator)
 ├── mem/arena.cst      (bump allocator)
 ├── mem/pool.cst       (fixed-size slot pool)
 └── mem/lifo.cst       (stack allocator)
```

The top-level `galloc`/`gfree` functions delegate to `freelist` for backward compatibility. New code can import submodules directly for better performance characteristics.

## Submodule Overview

| Submodule | Import | Alloc | Free | Use Case |
|-----------|--------|-------|------|----------|
| freelist | `mem.freelist` | O(n) | O(n) | General purpose, address-ordered coalescing, 16-byte header |
| bins | `mem.bins` | O(1) | O(1) | Fixed-size objects, slab-based, bitmap double-free detection, 8-byte header |
| arena | `mem.arena` | O(1) | bulk | Bump allocator, no individual free |
| pool | `mem.pool` | O(1) | O(1) | Fixed-size slots, optional debug mode with double-free detection |
| lifo | `mem.lifo` | O(1) | O(1) LIFO | Stack-like, 8-byte header, free must be in reverse order |

## Global API (mem.cst)

These functions delegate to `freelist` and exist for backward compatibility.

### gheapinit

```cst
fn gheapinit(size as i64) as void;
```

Initialize the global heap with `size` bytes via `freelist.reserve()`. No-op if already initialized. Must be called before `galloc`/`gfree`.

### gheapreset

```cst
fn gheapreset(size as i64) as void;
```

Release the current global heap and create a fresh one of `size` bytes.

### galloc

```cst
fn galloc(size as i64) as *u8;
```

Allocate `size` bytes from the global heap. Delegates to `freelist.alloc`.

### gfree

```cst
fn gfree(ptr as *u8) as void;
```

Free a pointer returned by `galloc`. Delegates to `freelist.free`.

### memcpy

```cst
fn memcpy(dst as *u8, src as *u8, n as i64) as *u8;
```

Copy `n` bytes from `src` to `dst`. Byte-by-byte, does not handle overlapping regions. Returns `dst`.

### memset

```cst
fn memset(dst as *u8, c as i32, n as i64) as *u8;
```

Fill `n` bytes of `dst` with byte value `c`. Returns `dst`.

### memcmp

```cst
fn memcmp(mem1 as *u8, mem2 as *u8, data_size as i64) as i32;
```

Compare `data_size` bytes. Returns 0 if equal, positive/negative on first differing byte.

### Type Aliases

```cst
type Heap = freelist.Heap;
type Header = freelist.Header;
```

For compatibility. Code using `mem.Heap` continues to work.

### Legacy Instance API

`reserve`, `release`, `alloc`, `free` are thin wrappers around the freelist equivalents. New code should use `mem.freelist` directly.

---

## Submodule: freelist

General-purpose allocator with address-ordered free-list and coalescing.

```cst
use "std/mem.cst" as mem;
// Access via mem.freelist.*
```

**Structs**: `Header` (size, next -- 16 bytes), `Heap` (freelist, start, top, limit)

**Key functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `reserve` | `(req_size as i64) as *Heap` | Create a new heap region via mmap |
| `release` | `(h as *Heap) as void` | Release entire heap via munmap |
| `alloc` | `(h as *Heap, req_size as i64) as *u8` | Allocate: free-list scan, then bump. 8-byte aligned |
| `free` | `(h as *Heap, ptr as *u8) as void` | Free with address-ordered insertion + coalescing |
| `memcpy` | `(dst as *u8, src as *u8, n as i64) as *u8` | Byte-by-byte copy |
| `memset` | `(dst as *u8, c as i32, n as i64) as *u8` | Fill memory |
| `memcmp` | `(mem1 as *u8, mem2 as *u8, data_size as i64) as i32` | Compare memory |

---

## Submodule: bins

O(1) slab allocator with 16 default size classes (8 to 4096 bytes), lookup table for bin selection, and bitmap-based double-free detection.

```cst
use "std/mem.cst" as mem;
// Access via mem.bins.*
```

**Structs**: `Bins` (top-level state), `Bin` (per-size-class), `Slab` (per-page region)

**Key functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `bins_new` | `(page_size as i64) as Bins` | Create with 16 default size classes |
| `bins_custom` | `(page_size as i64, sizes as *i64, count as i64) as Bins` | Create with custom size classes |
| `bins_alloc` | `(b as *Bins, size as i64) as *u8` | O(1) allocate from appropriate bin |
| `bins_free` | `(b as *Bins, ptr as *u8) as void` | O(1) free with double-free detection |
| `bins_destroy` | `(b as *Bins) as void` | Release all slabs and metadata |
| `bins_which` | `(b as *Bins, size as i64) as i64` | Which size class serves this size |
| `bins_overhead` | `(b as *Bins, size as i64) as i64` | Overhead for a given allocation |
| `bins_used` | `(b as *Bins) as i64` | Total bytes in use |
| `bins_total` | `(b as *Bins) as i64` | Total bytes mapped from kernel |
| `bins_waste` | `(b as *Bins) as i64` | Waste percentage |
| `bins_stats` | `(b as *Bins) as void` | Print per-bin usage to stdout |

**Default size classes:** 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 2048, 4096

---

## Submodule: arena

Bump allocator. O(1) allocation, no individual free. Reset or destroy to reclaim all memory at once.

```cst
use "std/mem.cst" as mem;
// Access via mem.arena.*
```

**Struct**: `Arena` (base, top, limit, size)

**Key functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `create` | `(size as i64) as Arena` | Create arena with given capacity |
| `destroy` | `(a as *Arena) as void` | Munmap all memory |
| `reset` | `(a as *Arena) as void` | Reset bump pointer, invalidates all allocations |
| `alloc` | `(a as *Arena, n as i64) as *u8` | Bump-allocate n bytes (8-byte aligned) |
| `alloc_zero` | `(a as *Arena, n as i64) as *u8` | Allocate and zero-fill |
| `alloc_str` | `(a as *Arena, s as *u8) as *u8` | Copy null-terminated string into arena |
| `save` | `(a as *Arena) as *u8` | Save current position |
| `restore` | `(a as *Arena, pos as *u8) as void` | Restore to saved position (partial reset) |
| `used` | `(a as *Arena) as i64` | Bytes currently allocated |
| `remaining` | `(a as *Arena) as i64` | Bytes available |
| `capacity` | `(a as *Arena) as i64` | Total capacity |

---

## Submodule: pool

Fixed-size slot allocator. O(1) alloc/free. Optional debug mode enables bitmap-based double-free detection.

```cst
use "std/mem.cst" as mem;
// Access via mem.pool.*
```

**Struct**: `Pool` (region, bitmap, freelist, slot_size, total_slots, used_slots, debug)

**Key functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `create` | `(slot_size as i64, count as i64, debug as i64) as Pool` | Create pool. debug=0 fast, debug=1 with double-free detection |
| `alloc` | `(p as *Pool) as *u8` | Pop a slot from freelist |
| `free` | `(p as *Pool, ptr as *u8) as void` | Push slot back. Debug mode detects double-free |
| `destroy` | `(p as *Pool) as void` | Munmap region and bitmap |
| `reset` | `(p as *Pool) as void` | Rebuild freelist, mark all slots free |
| `used` | `(p as *Pool) as i64` | Slots in use |
| `available` | `(p as *Pool) as i64` | Slots available |
| `capacity` | `(p as *Pool) as i64` | Total slots |
| `slot_size` | `(p as *Pool) as i64` | Slot size in bytes |

---

## Submodule: lifo

Stack allocator. O(1) alloc, O(1) free in LIFO order only. Each allocation has an 8-byte header storing the block size.

```cst
use "std/mem.cst" as mem;
// Access via mem.lifo.*
```

**Struct**: `Lifo` (base, top, limit, size)

**Key functions:**

| Function | Signature | Description |
|----------|-----------|-------------|
| `create` | `(size as i64) as Lifo` | Create LIFO allocator with given capacity |
| `destroy` | `(s as *Lifo) as void` | Munmap all memory |
| `reset` | `(s as *Lifo) as void` | Free everything |
| `alloc` | `(s as *Lifo, n as i64) as *u8` | Bump-allocate with 8-byte size header |
| `free` | `(s as *Lifo, ptr as *u8) as void` | Free last allocation only (validates LIFO order) |
| `save` | `(s as *Lifo) as *u8` | Save current position |
| `restore` | `(s as *Lifo, pos as *u8) as void` | Restore to saved position (bulk free) |
| `used` | `(s as *Lifo) as i64` | Bytes currently allocated |
| `remaining` | `(s as *Lifo) as i64` | Bytes available |
| `capacity` | `(s as *Lifo) as i64` | Total capacity |

---

## Typical Usage

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    // Global freelist (backward compatible)
    mem.gheapinit(4 * 1024 * 1024);
    let is *u8 as buf = mem.galloc(256);
    mem.gfree(buf);

    // Arena for batch work
    let is mem.arena.Arena as a = mem.arena.create(1024 * 1024);
    let is *u8 as tmp = mem.arena.alloc(&a, 512);
    mem.arena.reset(&a);
    mem.arena.destroy(&a);

    // Bins for many small objects
    let is mem.bins.Bins as b = mem.bins.bins_new(65536);
    let is *u8 as obj = mem.bins.bins_alloc(&b, 32);
    mem.bins.bins_free(&b, obj);
    mem.bins.bins_destroy(&b);

    // Pool for fixed-size slots
    let is mem.pool.Pool as p = mem.pool.create(64, 1024, 1);
    let is *u8 as slot = mem.pool.alloc(&p);
    mem.pool.free(&p, slot);
    mem.pool.destroy(&p);

    // LIFO for stack-like patterns
    let is mem.lifo.Lifo as s = mem.lifo.create(65536);
    let is *u8 as a1 = mem.lifo.alloc(&s, 128);
    let is *u8 as a2 = mem.lifo.alloc(&s, 64);
    mem.lifo.free(&s, a2);
    mem.lifo.free(&s, a1);
    mem.lifo.destroy(&s);

    return 0;
}
```

## Notes

- `std/arena.cst` is a standalone copy of `std/mem/arena.cst` for projects that only need an arena without pulling in the full mem module.
- All allocators use `mmap`/`munmap` directly -- no libc dependency.
- All allocators align to 8 bytes minimum.
- The global API (`galloc`/`gfree`) will remain stable. Direct submodule usage is recommended for new code where performance characteristics matter.
