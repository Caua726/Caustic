# Syscalls

`syscall(number, arg1, arg2, ...)` performs a direct Linux kernel system call. Caustic programs have no libc -- syscalls are the only way to interact with the OS.

## Syntax

```cst
let is i64 as result = syscall(number, arg1, arg2, ...);
```

Returns an `i64`. Negative values indicate errors (negated errno).

## Common Syscall Numbers (x86_64 Linux)

| Number | Name    | Arguments                          |
|--------|---------|------------------------------------|
| 0      | read    | fd, buf, count                     |
| 1      | write   | fd, buf, count                     |
| 2      | open    | path, flags, mode                  |
| 3      | close   | fd                                 |
| 8      | lseek   | fd, offset, whence                 |
| 9      | mmap    | addr, len, prot, flags, fd, offset |
| 10     | mprotect| addr, len, prot                    |
| 11     | munmap  | addr, len                          |
| 12     | brk     | addr                               |
| 60     | exit    | code                               |

## Examples

### Write to stdout

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

### Read from stdin

```cst
fn main() as i32 {
    let is [128]u8 as buf;
    let is i64 as n = syscall(0, 0, &buf, 128);
    // n = number of bytes read
    syscall(1, 1, &buf, n);
    return 0;
}
```

### Open, Read, Close a File

```cst
fn main() as i32 {
    let is i64 as fd = syscall(2, "/etc/hostname", 0, 0);
    if fd < 0 {
        syscall(1, 2, "open failed\n", 12);
        return 1;
    }
    let is [256]u8 as buf;
    let is i64 as n = syscall(0, fd, &buf, 256);
    syscall(1, 1, &buf, n);
    syscall(3, fd);
    return 0;
}
```

### Allocate Memory with mmap

```cst
fn alloc_page() as *u8 {
    // mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    let is i64 as ptr = syscall(9, 0, 4096, 3, 34, 0 - 1, 0);
    return cast(*u8, ptr);
}
```

## Standard Library Wrappers

For cleaner code, use `std/linux.cst` which provides named wrappers:

```cst
use "std/linux.cst" as linux;

fn main() as i32 {
    linux.write(1, "Hello\n", 6);
    linux.exit(0);
    return 0;
}
```

## Error Handling

Syscall return values of `-1` through `-4095` indicate errors. The absolute value is the errno:

```cst
let is i64 as fd = syscall(2, "/nonexistent", 0, 0);
if fd < 0 {
    // fd is -errno, e.g. -2 for ENOENT
    syscall(1, 2, "error\n", 6);
    return 1;
}
```
