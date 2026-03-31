## _module
std/io.cst — Input/Output

Buffered and unbuffered I/O for files and stdio.

Key functions:
  write_str(fd, s)     — write string to fd
  write_int(fd, n)     — write integer as text
  write_bytes(fd, b,n) — write raw bytes
  printf(fmt, ...)     — formatted output
  read_file(path)      — read entire file into memory
  file_exists(path)    — check if file exists

Buffered I/O:
  Reader(fd, buf, cap) — buffered byte reader
  Writer(fd, buf, cap) — buffered writer

Usage:
  use "std/io.cst" as io;
  io.write_str(linux.STDOUT, "hello\n");
---
## Reader
struct Reader { fd as i64; buf as *u8; cap as i64; len as i64; pos as i64; }

Buffered reader for efficient byte-at-a-time input.

Wraps a file descriptor with an internal buffer. Reads large chunks
from the OS and serves bytes one at a time from the buffer.

Create with io.Reader(fd, buf, cap) where buf is your buffer.

Example:
  let is [4096]u8 as buf;
  let is io.Reader as r = io.Reader(linux.STDIN, &buf, 4096);
  let is i32 as byte = io.read(&r);  // read one byte
---
## Reader
fn Reader(fd as i64, buf as *u8, cap as i64) as Reader

Create a new buffered Reader.

Parameters:
  fd  - file descriptor to read from (linux.STDIN, or an opened file)
  buf - pointer to a buffer (e.g. [4096]u8 on the stack)
  cap - buffer capacity in bytes

Returns: initialized Reader struct.

Example:
  let is [8192]u8 as buf;
  let is io.Reader as r = io.Reader(linux.STDIN, &buf, 8192);
---
## read
fn read(r as *Reader) as i32

Read a single byte from a buffered Reader.

Returns: byte value (0-255) on success, -1 on EOF or error.

Automatically refills the internal buffer when exhausted.

Example:
  let is i32 as ch = io.read(&r);
  if (ch == (0 - 1)) { /* EOF */ }
---
## readline
fn readline(r as *Reader) as string.String

Read a full line from a buffered Reader (up to newline or EOF).

Returns: a string.String with the line content (without trailing newline).
Returns empty string on EOF.

The returned string is heap-allocated. Free with io._free() if needed.

Example:
  let is string.String as line = io.readline(&r);
  io.write_str(linux.STDOUT, line.ptr);
---
## read_until
fn read_until(r as *Reader, delim as u8) as string.String

Read bytes until the delimiter character is found (or EOF).

Parameters:
  r     - pointer to Reader
  delim - delimiter byte to stop at (not included in result)

Returns: a string.String with the content before the delimiter.
---
## Writer
struct Writer { fd as i64; buf as *u8; cap as i64; len as i64; }

Buffered writer for efficient output.

Accumulates bytes in an internal buffer and flushes when full
or when io.flush() is called. Avoids many small syscalls.

Example:
  let is [4096]u8 as buf;
  let is io.Writer as w = io.Writer(linux.STDOUT, &buf, 4096);
  io.wwrite(&w, "hello ", 6);
  io.wwrite(&w, "world\n", 6);
  io.flush(&w);  // actually writes to stdout
---
## Writer
fn Writer(fd as i64, buf as *u8, cap as i64) as Writer

Create a new buffered Writer.

Parameters:
  fd  - file descriptor to write to
  buf - pointer to a buffer
  cap - buffer capacity

Returns: initialized Writer struct.
---
## wwrite
fn wwrite(w as *Writer, data as *u8, len as i64) as void

Write bytes to a buffered Writer. Flushes automatically when buffer is full.

Parameters:
  w    - pointer to Writer
  data - bytes to write
  len  - number of bytes
---
## wwrite_str
fn wwrite_str(w as *Writer, s as *u8) as void

Write a null-terminated string to a buffered Writer.
---
## wwrite_int
fn wwrite_int(w as *Writer, n as i64) as void

Write an integer as decimal text to a buffered Writer.
---
## flush
fn flush(w as *Writer) as void

Flush all buffered data to the underlying file descriptor.
Call this before closing the fd or when you need output to appear immediately.
---
## write_str
fn write_str(fd as i64, s as *u8) as void

Write a null-terminated string directly to a file descriptor (unbuffered).

Parameters:
  fd - file descriptor (linux.STDOUT, linux.STDERR, etc.)
  s  - null-terminated string

Example:
  io.write_str(linux.STDOUT, "hello world\n");
---
## write_bytes
fn write_bytes(fd as i64, buf as *u8, n as i64) as void

Write exactly n bytes to a file descriptor (unbuffered).

Parameters:
  fd  - file descriptor
  buf - source buffer
  n   - number of bytes to write

Example:
  io.write_bytes(linux.STDOUT, name_ptr, cast(i64, name_len));
---
## write_int
fn write_int(fd as i64, n as i64) as void

Write an integer as decimal text to a file descriptor (unbuffered).

Handles negative numbers (prints '-' prefix).

Example:
  io.write_int(linux.STDOUT, 42);       // prints "42"
  io.write_int(linux.STDOUT, 0 - 7);    // prints "-7"
---
## write_float
fn write_float(fd as i64, f as f64) as void

Write a floating-point number as decimal text to a file descriptor.
Uses 6 decimal places by default.

Example:
  io.write_float(linux.STDOUT, 3.14159);  // prints "3.141590"
---
## write_hex
fn write_hex(fd as i64, n as i64) as void

Write an integer as hexadecimal (lowercase, with 0x prefix).

Example:
  io.write_hex(linux.STDOUT, 255);  // prints "0xff"
---
## read_file
fn read_file(path as *u8, out_size as *i64) as *u8

Read an entire file into a heap-allocated buffer.

Parameters:
  path     - null-terminated file path
  out_size - pointer to i64, receives file size

Returns: pointer to buffer (null-terminated), or null on error.
Caller should free the returned buffer when done.

Example:
  let is i64 as size with mut = 0;
  let is *u8 as data = io.read_file("config.txt", &size);
  if (cast(i64, data) != 0) {
      io.write_bytes(linux.STDOUT, data, size);
  }
---
## file_exists
fn file_exists(path as *u8) as i32

Check if a file exists.

Returns: 1 if file exists, 0 if not.

Example:
  if (io.file_exists("data.txt") == 1) { ... }
---
## printf
fn printf(fmt as *u8, ...) as void

Formatted output to stdout. Supports %s (string), %d (integer), %x (hex).

This is a variadic function. Format specifiers:
  %s - null-terminated string (*u8)
  %d - signed integer (i64)
  %x - hexadecimal integer (i64)
  %% - literal percent

Example:
  io.printf("name=%s age=%d\n", name, age);
---
