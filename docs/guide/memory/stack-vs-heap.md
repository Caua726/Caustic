# Stack vs Heap

Caustic has two memory regions for program data. There is no garbage collector -- all heap memory must be freed manually.

## Stack

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

## Heap

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

## When to Use Which

| Use stack when... | Use heap when... |
|---|---|
| Size is known at compile time | Size determined at runtime |
| Data fits in a few KB | Data is large (> ~4 KB) |
| Data is local to one function | Data must outlive the function |
| Performance is critical | Lifetime flexibility is needed |

## Common Pattern: Stack Buffer, Heap Fallback

```cst
fn read_line() as *u8 {
    let is [256]u8 as tmp;
    let is i64 as n = syscall(0, 0, &tmp[0], 255);  // read into stack buf
    let is *u8 as result = mem.galloc(n + 1);        // copy to heap
    // copy tmp[0..n] into result
    return result;  // caller must gfree
}
```

## Memory Leaks

Forgetting to call `gfree` leaks memory. Use `defer` to pair allocation with cleanup:

```cst
let is *u8 as ptr = mem.galloc(1024);
defer mem.gfree(ptr);
// ptr is freed no matter which return path is taken
```
