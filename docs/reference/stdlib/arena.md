# std/arena.cst -- Arena Allocator

Bump allocator with O(1) allocation and no individual free. Memory is obtained via `mmap` and released all at once with `destroy()` or logically reclaimed with `reset()`. All allocations are 8-byte aligned.

> **Note:** `std/arena.cst` and `std/mem/arena.cst` are identical in functionality (they differ only in the import path for `linux.cst`). Use whichever matches your import style.

## Dependencies

```cst
use "linux.cst" as linux;
```

## Import

```cst
use "std/arena.cst" as arena;
```

## Structs

### Arena

```cst
struct Arena {
    base  as *u8;
    top   as *u8;
    limit as *u8;
    size  as i64;
}
```

`base` is the start of the mmap'd region. `top` is the current bump pointer (next allocation starts here). `limit` is one past the end of the region. `size` is the total capacity in bytes.

## Functions

### Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `create` | `fn create(size as i64) as Arena` | Create a new arena with `size` bytes via `mmap`. Returns an Arena with null `base` on failure |
| `destroy` | `fn destroy(a as *Arena) as void` | Release all memory via `munmap`. Zeroes out the Arena fields |
| `reset` | `fn reset(a as *Arena) as void` | Reset the bump pointer to `base`. All previous allocations become invalid |

### Allocation

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc` | `fn alloc(a as *Arena, n as i64) as *u8` | Allocate `n` bytes (8-byte aligned). Returns null if arena is full |
| `alloc_zero` | `fn alloc_zero(a as *Arena, n as i64) as *u8` | Allocate `n` bytes and zero them. Returns null if arena is full |
| `alloc_str` | `fn alloc_str(a as *Arena, s as *u8) as *u8` | Copy a null-terminated string into the arena (including the null terminator). Returns null if arena is full |

### Save / Restore

| Function | Signature | Description |
|----------|-----------|-------------|
| `save` | `fn save(a as *Arena) as *u8` | Save the current bump pointer position |
| `restore` | `fn restore(a as *Arena, pos as *u8) as void` | Restore the bump pointer to a previously saved position (partial reset). No-op if `pos` is outside the arena bounds |

### Introspection

| Function | Signature | Description |
|----------|-----------|-------------|
| `used` | `fn used(a as *Arena) as i64` | Bytes currently allocated |
| `remaining` | `fn remaining(a as *Arena) as i64` | Bytes still available |
| `capacity` | `fn capacity(a as *Arena) as i64` | Total arena size in bytes |

## Example

```cst
use "std/arena.cst" as arena;
use "std/io.cst" as io;

fn main() as i32 {
    let is arena.Arena as a = arena.create(4096);

    // Allocate some data
    let is *u8 as buf = arena.alloc(&a, 256);
    let is *u8 as name = arena.alloc_str(&a, "caustic");

    io.printf("used: %d / %d\n", arena.used(&a), arena.capacity(&a));

    // Save/restore for temporary allocations
    let is *u8 as mark = arena.save(&a);
    let is *u8 as tmp = arena.alloc(&a, 1024);
    // ... use tmp ...
    arena.restore(&a, mark);  // reclaim the 1024 bytes

    io.printf("after restore: %d used\n", arena.used(&a));

    // Free everything
    arena.destroy(&a);
    return 0;
}
```
