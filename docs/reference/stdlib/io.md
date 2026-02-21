# std/io.cst -- Input/Output

Buffered and unbuffered I/O primitives for reading and writing to file descriptors. Includes formatted output (`printf`), file operations, and convenience print functions. Built on raw Linux syscalls via `std/linux.cst`.

## Dependencies

```cst
use "linux.cst" as linux;
use "string.cst" as string;
use "mem.cst" as mem;
```

## Structs

### Reader

```cst
struct Reader {
    fd  as i64;
    buf as *u8;
    cap as i64;
    len as i64;
    pos as i64;
}
```

Buffered reader. `fd` is the file descriptor. `buf` is an externally-owned buffer of `cap` bytes. `len` is how many bytes were read into the buffer on the last syscall. `pos` is the current read position within the buffer.

### Writer

```cst
struct Writer {
    fd  as i64;
    buf as *u8;
    cap as i64;
    len as i64;
}
```

Buffered writer. `fd` is the file descriptor. `buf` is an externally-owned buffer of `cap` bytes. `len` is how many bytes are currently buffered.

### VaList

```cst
struct VaList {
    gp_offset         as i32;
    fp_offset          as i32;
    overflow_arg_area  as *u8;
    reg_save_area      as *u8;
}
```

System V AMD64 ABI `va_list` structure. Used internally by `printf`. Initialized via the compiler intrinsic `__builtin_va_start`.

### File

```cst
struct File {
    fd as i64;
}
```

Lightweight file handle wrapping a file descriptor.

---

## Buffered Reader Functions

### Reader (constructor)

```cst
fn Reader(fd as i64, buf as *u8, cap as i64) as Reader;
```

Create a buffered reader for file descriptor `fd`, using `buf` as the internal buffer with capacity `cap`. The caller owns the buffer memory.

**Example:**

```cst
let is [4096]u8 as rbuf;
let is io.Reader as r = io.Reader(linux.STDIN, &rbuf, 4096);
```

### read

```cst
fn read(r as *Reader) as i32;
```

Read one byte from the buffered reader. Returns the byte value (0--255) or `-1` on EOF or error. Refills the internal buffer automatically when exhausted.

**Example:**

```cst
let is i32 as b = io.read(&r);
if (b == (0 - 1)) {
    // EOF
}
```

### readline

```cst
fn readline(r as *Reader) as string.String;
```

Read a full line from the buffered reader, up to (but not including) the newline character `\n`. The returned `String` is heap-allocated via `galloc` and must be freed by the caller with `string.string_free`. Returns an empty string on EOF.

**Example:**

```cst
let is string.String as line = io.readline(&r);
io.println(line);
string.string_free(line);
```

---

## Buffered Writer Functions

### Writer (constructor)

```cst
fn Writer(fd as i64, buf as *u8, cap as i64) as Writer;
```

Create a buffered writer for file descriptor `fd`, using `buf` as the internal buffer with capacity `cap`. The caller owns the buffer memory.

**Example:**

```cst
let is [4096]u8 as wbuf;
let is io.Writer as w = io.Writer(linux.STDOUT, &wbuf, 4096);
```

### flush

```cst
fn flush(w as *Writer) as void;
```

Flush the buffered writer, writing all pending bytes to the underlying file descriptor. Must be called before the program exits to avoid losing output.

**Example:**

```cst
io.flush(&w);
```

### put

```cst
fn put(w as *Writer, b as u8) as void;
```

Write a single byte to the buffered writer. Flushes automatically if the buffer is full.

**Example:**

```cst
io.put(&w, cast(u8, 65)); // write 'A'
```

### write

```cst
fn write(w as *Writer, s as string.String) as void;
```

Write a `String` to the buffered writer. If the string is larger than the buffer capacity, it flushes first and writes directly to the fd (bypassing the buffer). Otherwise, bytes are appended to the buffer one at a time.

**Example:**

```cst
io.write(&w, string.String("hello world"));
```

---

## Unbuffered I/O Functions

### write_bytes

```cst
fn write_bytes(fd as i64, ptr as *u8, len as i64) as i64;
```

Write `len` bytes from `ptr` to file descriptor `fd`. Thin wrapper around `linux.write`. Returns the number of bytes written or a negative error code.

### write_str

```cst
fn write_str(fd as i64, s as string.String) as i64;
```

Write a `String` to file descriptor `fd`. Returns the number of bytes written or a negative error code.

### read_exact

```cst
fn read_exact(fd as i64, buf as *u8, count as i64) as i64;
```

Read exactly `count` bytes from `fd` into `buf`, retrying on partial reads. Returns the total number of bytes read. May return less than `count` on EOF or error.

**Example:**

```cst
let is [128]u8 as buf;
let is i64 as n = io.read_exact(fd, &buf, 128);
```

---

## Formatted Output

### printf

```cst
fn printf(fmt as *u8, ...) as void;
```

Formatted output to stdout. Variadic function using the System V AMD64 `va_list` ABI. Writes directly to `STDOUT` (unbuffered).

**Format specifiers:**

| Specifier | Type | Description |
|-----------|------|-------------|
| `%d` | `i64` | Signed decimal integer |
| `%s` | `*u8` | Null-terminated C string |
| `%S` | `*string.String` | Pointer to a Caustic `String` struct |
| `%x` | `i64` | Hexadecimal (16-digit, `0x` prefixed) |
| `%c` | `i64` (truncated to `u8`) | Single character |
| `%%` | -- | Literal `%` character |

**Example:**

```cst
io.printf("count = %d, name = %s\n", 42, "caustic");
io.printf("hex: %x, char: %c\n", 255, 65);
```

**Note:** `%S` expects a pointer to a `String`, not a `String` value. Pass `&s` where `s` is a `string.String`.

### print_hex

```cst
fn print_hex(n as i64) as void;
```

Print an `i64` value in hexadecimal with `0x` prefix to stdout. Always prints 16 hex digits (zero-padded). Heap-allocates a temporary buffer.

### print_bool

```cst
fn print_bool(b as i32) as void;
```

Print `"true"` or `"false"` to stdout based on whether `b` is nonzero.

### print_ptr

```cst
fn print_ptr(ptr as *u8) as void;
```

Print a pointer value in hexadecimal to stdout. Equivalent to `print_hex(cast(i64, ptr))`.

### print

```cst
fn print(s as string.String) as void;
```

Print a `String` to stdout. No-op if `s.len` is 0.

### println

```cst
fn println(s as string.String) as void;
```

Print a `String` to stdout followed by a newline.

### print_int

```cst
fn print_int(n as i64) as void;
```

Print an `i64` as a decimal string to stdout. Converts via `string.int_to_string`, prints, then frees the temporary string.

---

## Legacy / Unbuffered Line Reading

### read_line

```cst
fn read_line() as string.String;
```

Read a line from stdin (unbuffered, one byte at a time via syscall). Returns a heap-allocated `String` without the trailing newline. Caller must free with `string.string_free`. Prefer `readline` with a buffered `Reader` for performance.

---

## File Operations

### read_file

```cst
fn read_file(path as string.String) as string.String;
```

Read an entire file into a heap-allocated `String`. Uses `lseek` to determine file size, then reads in one syscall. Returns an empty string if the file cannot be opened. Caller must free the result with `string.string_free`.

**Example:**

```cst
let is string.String as content = io.read_file(string.String("data.txt"));
```

### write_file

```cst
fn write_file(path as string.String, content as string.String) as i64;
```

Write `content` to a file at `path`, creating or truncating it. Opens with flags `O_WRONLY | O_CREAT | O_TRUNC` and mode `0644`. Returns the number of bytes written, or a negative error code.

### append_file

```cst
fn append_file(path as string.String, content as string.String) as i64;
```

Append `content` to a file at `path`, creating it if necessary. Opens with flags `O_WRONLY | O_CREAT | O_APPEND` and mode `0644`. Returns the number of bytes written, or a negative error code.

### file_exists

```cst
fn file_exists(path as string.String) as i32;
```

Check if a file exists by attempting to open it read-only. Returns `1` if the file can be opened, `0` otherwise.

### file_size

```cst
fn file_size(path as *u8) as i64;
```

Return the size of a file in bytes, or `-1` if the file cannot be opened. Takes a null-terminated `*u8` path (not a `String`).

### open_file

```cst
fn open_file(path as string.String) as File;
```

Open a file for reading. Returns a `File` struct. The `fd` field will be negative if the open failed.

### close_file

```cst
fn close_file(file as File) as void;
```

Close a file handle.

**Example:**

```cst
let is io.File as f = io.open_file(string.String("input.txt"));
if (f.fd >= 0) {
    // read from f.fd ...
    io.close_file(f);
}
```

---

## Typical Usage

```cst
use "std/io.cst" as io;
use "std/string.cst" as string;
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(2 * 1024 * 1024);

    // Buffered reading from stdin
    let is [4096]u8 as rbuf;
    let is io.Reader as r = io.Reader(0, &rbuf, 4096);

    let is string.String as line = io.readline(&r);
    io.printf("you said: %s\n", line.ptr);
    string.string_free(line);

    // File I/O
    let is string.String as path = string.String("output.txt");
    io.write_file(path, string.String("hello from caustic\n"));

    return 0;
}
```
