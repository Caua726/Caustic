# Defer Patterns

Common patterns for using `defer` to manage resources in Caustic.

## Allocate / Free

The most common pattern -- allocate memory, immediately defer the free:

```cst
use "std/mem.cst" as mem;

fn process_data() as i32 {
    let is *u8 as buf = mem.galloc(4096);
    defer mem.gfree(buf);

    // Work with buf safely
    // No matter which return path is taken, buf is freed
    if some_error() {
        return 1;  // gfree(buf) runs here
    }
    return 0;      // gfree(buf) runs here too
}
```

## Open / Close

Wrap `close` in a function since `defer syscall(...)` is not supported:

```cst
fn close_fd(fd as i64) as void {
    syscall(3, fd);
}

fn read_config() as i32 {
    let is i64 as fd = syscall(2, "/etc/config", 0, 0);
    if fd < 0 { return 1; }
    defer close_fd(fd);

    let is [512]u8 as buf;
    let is i64 as n = syscall(0, fd, &buf, 512);
    // fd is closed automatically on return
    return 0;
}
```

## Multiple Resources (LIFO)

When acquiring multiple resources, defers execute in reverse order:

```cst
fn copy_file(src as *u8, dst as *u8) as i32 {
    let is i64 as in_fd = syscall(2, src, 0, 0);
    if in_fd < 0 { return 1; }
    defer close_fd(in_fd);

    let is i64 as out_fd = syscall(2, dst, 65, 420);
    if out_fd < 0 { return 1; }   // in_fd closed here
    defer close_fd(out_fd);

    let is *u8 as buf = mem.galloc(4096);
    defer mem.gfree(buf);

    // On return: gfree(buf), close(out_fd), close(in_fd) -- LIFO order
    return 0;
}
```

## Constructor Pattern

Allocate, defer cleanup, do initialization, return on success:

```cst
use "std/mem.cst" as mem;

struct Connection {
    fd as i64;
    buf as *u8;
    buf_len as i64;
}

fn connect(host as *u8) as *Connection {
    let is *u8 as raw = mem.galloc(sizeof(Connection));
    let is *Connection as conn = cast(*Connection, raw);

    conn.buf = mem.galloc(1024);
    conn.buf_len = 1024;
    conn.fd = open_socket(host);

    return conn;
}

fn disconnect(conn as *Connection) as void {
    close_fd(conn.fd);
    mem.gfree(conn.buf);
    mem.gfree(cast(*u8, conn));
}

fn do_request(host as *u8) as i32 {
    let is *Connection as conn = connect(host);
    defer disconnect(conn);

    // Use conn...
    return 0;
}
```

## Cleanup Guards in Loops

Defers inside loops are scoped to that iteration's block:

```cst
fn process_items(count as i64) as i32 {
    let is i64 as i = 0;
    while i < count {
        let is *u8 as item = mem.galloc(256);
        defer mem.gfree(item);

        // item is freed at the end of each iteration
        // when the block's implicit return point is reached

        i = i + 1;
    }
    return 0;
}
```
