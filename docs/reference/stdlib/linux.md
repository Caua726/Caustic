# std/linux.cst -- Syscall Wrappers

Thin wrappers around Linux x86_64 system calls. No libc dependency -- each function uses the `syscall` builtin to invoke the kernel directly.

## Constants

### File Descriptors

```cst
let is i64 as STDIN  with imut = 0;
let is i64 as STDOUT with imut = 1;
let is i64 as STDERR with imut = 2;
```

### open() Flags

```cst
let is i64 as O_RDONLY with imut = 0;
let is i64 as O_WRONLY with imut = 1;
let is i64 as O_RDWR   with imut = 2;
let is i64 as O_CREAT  with imut = 64;   // 0x40
let is i64 as O_TRUNC  with imut = 512;  // 0x200
let is i64 as O_APPEND with imut = 1024; // 0x400
```

Flags can be combined with bitwise OR via addition (e.g., `O_WRONLY + O_CREAT + O_TRUNC` = 577).

### lseek() Whence

```cst
let is i64 as SEEK_SET with imut = 0;
let is i64 as SEEK_CUR with imut = 1;
let is i64 as SEEK_END with imut = 2;
```

### mmap() Protection Flags

```cst
let is i64 as PROT_READ  with imut = 1;
let is i64 as PROT_WRITE with imut = 2;
let is i64 as PROT_EXEC  with imut = 4;
```

### mmap() Mapping Flags

```cst
let is i64 as MAP_SHARED    with imut = 1;
let is i64 as MAP_PRIVATE   with imut = 2;
let is i64 as MAP_ANONYMOUS with imut = 32;
```

---

## Functions

### read

```cst
fn read(fd as i64, buf as *u8, count as i64) as i64;
```

Read up to `count` bytes from file descriptor `fd` into `buf`. Syscall number: 0. Returns the number of bytes read, 0 on EOF, or a negative error code.

**Example:**

```cst
let is [256]u8 as buf;
let is i64 as n = linux.read(linux.STDIN, &buf, 256);
```

### write

```cst
fn write(fd as i64, buf as *u8, count as i64) as i64;
```

Write `count` bytes from `buf` to file descriptor `fd`. Syscall number: 1. Returns the number of bytes written or a negative error code.

**Example:**

```cst
linux.write(linux.STDOUT, "hello\n", 6);
```

### open

```cst
fn open(path as *u8, flags as i64, mode as i64) as i64;
```

Open a file at `path` with the given `flags` and `mode`. Syscall number: 2. Returns a file descriptor (>= 0) on success or a negative error code.

The `mode` parameter specifies file permissions when creating a file (only relevant with `O_CREAT`). Common values: `420` = `0644`, `448` = `0700`.

**Example:**

```cst
let is i64 as fd = linux.open("data.txt", linux.O_RDONLY, 0);
if (fd < 0) {
    // error
}
```

### close

```cst
fn close(fd as i64) as i64;
```

Close file descriptor `fd`. Syscall number: 3. Returns 0 on success or a negative error code.

### lseek

```cst
fn lseek(fd as i64, offset as i64, whence as i64) as i64;
```

Reposition the file offset of `fd`. Syscall number: 8. Returns the new offset in bytes from the beginning of the file, or a negative error code.

Common pattern to get file size:

```cst
let is i64 as size = linux.lseek(fd, 0, linux.SEEK_END);
linux.lseek(fd, 0, linux.SEEK_SET); // rewind
```

### mmap

```cst
fn mmap(addr as *u8, len as i64, prot as i64, flags as i64, fd as i64, off as i64) as *u8;
```

Map memory pages. Syscall number: 9. Returns a pointer to the mapped region, or `MAP_FAILED` (a large negative value cast to pointer) on error.

For anonymous mappings (heap allocation), pass `fd = -1` and `off = 0`:

```cst
let is i64 as minus_one = 0 - 1;
let is *u8 as page = linux.mmap(
    cast(*u8, 0),          // addr: kernel chooses
    4096,                   // len: one page
    linux.PROT_READ + linux.PROT_WRITE, // prot
    linux.MAP_PRIVATE + linux.MAP_ANONYMOUS, // flags
    minus_one,              // fd: not backed by file
    0                       // offset
);
```

### munmap

```cst
fn munmap(addr as *u8, len as i64) as i64;
```

Unmap memory pages previously mapped with `mmap`. Syscall number: 11. Returns 0 on success or a negative error code.

### brk

```cst
fn brk(addr as *u8) as *u8;
```

Set the program break (end of the data segment). Syscall number: 12. Returns the new program break. Passing `cast(*u8, 0)` returns the current break without changing it.

### exit

```cst
fn exit(code as i32) as void;
```

Terminate the process with exit status `code`. Syscall number: 60. Does not return.

**Example:**

```cst
linux.exit(cast(i32, 0)); // success
linux.exit(cast(i32, 1)); // failure
```

---

## Syscall Numbers Reference

| Number | Name | Signature |
|--------|------|-----------|
| 0 | read | `(fd, buf, count)` |
| 1 | write | `(fd, buf, count)` |
| 2 | open | `(path, flags, mode)` |
| 3 | close | `(fd)` |
| 8 | lseek | `(fd, offset, whence)` |
| 9 | mmap | `(addr, len, prot, flags, fd, off)` |
| 11 | munmap | `(addr, len)` |
| 12 | brk | `(addr)` |
| 60 | exit | `(code)` |

## Notes

- All functions use the Caustic `syscall` builtin, which maps directly to the `syscall` x86_64 instruction. Arguments follow the System V ABI register order: `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`.
- Error codes are returned as negative values (e.g., `-2` = `ENOENT`, `-13` = `EACCES`).
- There is no `errno` -- check the return value directly.
- Caustic has no negative integer literals. Use `0 - N` to express negative values (e.g., `0 - 1` for `-1`).
