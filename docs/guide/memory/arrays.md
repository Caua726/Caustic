# Arrays

Caustic supports fixed-size arrays allocated on the stack.

## Declaration

```cst
let is [N]T as name;
```

`N` must be a compile-time integer constant. `T` can be any type.

```cst
let is [64]u8 as buffer;
let is [10]i32 as numbers;
let is [4]f64 as coords;
```

## Indexing

Zero-based, using `arr[i]`:

```cst
let is [5]i32 as a;
a[0] = 10;
a[1] = 20;
let is i32 as sum = a[0] + a[1];
```

There is **no bounds checking**. Out-of-bounds access silently reads or writes wrong memory.

## Array Size

`sizeof([N]T)` equals `N * sizeof(T)`.

```cst
// [64]u8 = 64 bytes
// [10]i32 = 40 bytes
// [4]i64 = 32 bytes
```

## Passing Arrays to Functions

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

## Character Buffers

Byte arrays are commonly used as string buffers:

```cst
let is [256]u8 as buf;
buf[0] = 72;   // 'H'
buf[1] = 105;  // 'i'
buf[2] = 10;   // newline
syscall(1, 1, &buf[0], 3);  // write to stdout
```

## Limitations

- Size must be a compile-time constant (no `let is [n]T`).
- No array literals or initializer syntax.
- Stack-allocated only for locals. Use heap for dynamic sizes.
