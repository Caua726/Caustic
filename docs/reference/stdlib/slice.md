# std/slice.cst -- Generic Dynamic Array

A growable array type built on the global heap allocator. Requires `std/mem.cst` heap initialization.

## Struct

```cst
struct Slice gen T {
    ptr as *T;
    len as i32;
    cap as i32;
}
```

- `ptr`: Pointer to the backing array.
- `len`: Number of elements currently stored.
- `cap`: Total capacity (number of elements allocated).

## Functions

### slice_new

```cst
fn slice_new gen T (cap as i32) as Slice gen T;
```

Create a new slice with initial capacity `cap`. Allocates `cap * sizeof(T)` bytes from the global heap.

**Example:**

```cst
use "std/slice.cst" as sl;
use "std/mem.cst" as mem;

mem.gheapinit(1048576);
let is sl.Slice gen i32 as s = sl.slice_new gen i32 (16);
```

## Methods (impl Slice gen T)

### push

```cst
fn push(self as *Slice gen T, val as T) as void;
```

Append `val` to the end of the slice. If the slice is full (`len >= cap`), the backing array is reallocated with double the capacity. The old data is copied and freed.

### get

```cst
fn get(self as *Slice gen T, idx as i32) as T;
```

Return the element at index `idx`. No bounds checking is performed.

### set

```cst
fn set(self as *Slice gen T, idx as i32, val as T) as void;
```

Set the element at index `idx` to `val`. No bounds checking is performed.

### pop

```cst
fn pop(self as *Slice gen T) as T;
```

Remove and return the last element. Decrements `len`. No underflow checking is performed.

### free

```cst
fn free(self as *Slice gen T) as void;
```

Free the backing array and reset `len` and `cap` to 0. Safe to call on an already-freed slice (no-op if `ptr` is null).

## Example

```cst
use "std/slice.cst" as sl;
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(1048576);

    let is sl.Slice gen i32 as s = sl.slice_new gen i32 (4);
    s.push(10);
    s.push(20);
    s.push(30);

    let is i32 as sum = s.get(0) + s.get(1) + s.get(2);

    let is i32 as last = s.pop();  // 30

    s.free();
    return sum;  // 60
}
```

## Notes

- Requires `mem.gheapinit()` before any slice operations.
- The growth strategy doubles capacity on overflow.
- No bounds checking on `get`, `set`, or `pop`. Out-of-bounds access is undefined behavior.
- Works with any type: `Slice gen i32`, `Slice gen *u8`, `Slice gen Point`, etc.
