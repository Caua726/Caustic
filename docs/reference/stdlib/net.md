# std/net.cst -- Networking

IPv4 TCP and UDP networking primitives. Provides socket creation, address construction, connection management, data transfer (including looping send/recv helpers), UDP datagrams, socket options, and poll-based I/O readiness. Built on raw Linux syscalls via `std/linux.cst`.

## Dependencies

```cst
use "linux.cst" as linux;
use "mem.cst" as mem;
use "io.cst" as io;
use "compatcffi.cst" as cffi;
use "string.cst" as str;
```

## Import

```cst
use "std/net.cst" as net;
```

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `SOCKADDR_IN_SIZE` | `16` | Size of `sockaddr_in` structure in bytes |
| `SO_KEEPALIVE` | `9` | Socket option: enable keep-alive |
| `SO_RCVTIMEO` | `20` | Socket option: receive timeout |
| `SO_SNDTIMEO` | `21` | Socket option: send timeout |
| `IPPROTO_TCP_LVL` | `6` | Protocol level for TCP options |
| `TCP_NODELAY` | `1` | TCP option: disable Nagle's algorithm |
| `INADDR_ANY` | `0` | Bind to all interfaces |
| `INADDR_LOOPBACK` | `2130706433` | Loopback address (`127.0.0.1`) |
| `MSG_DONTWAIT` | `64` | Non-blocking send/recv flag |
| `DEFAULT_BACKLOG` | `128` | Default listen backlog |

## Structs

### Addr

```cst
struct Addr {
    data as *u8;
}
```

Wraps a pointer to a 16-byte `sockaddr_in` buffer. Constructed via `addr_init`, `addr_any`, or `addr_loopback`. The caller must provide and keep alive a `[16]u8` buffer.

### PollFd

```cst
struct PollFd {
    fd      as i32;
    events  as i16;
    revents as i16;
}
```

Matches the Linux `struct pollfd` layout (8 bytes). Used with `net_poll` to monitor multiple file descriptors for I/O readiness.

## Functions

### IP Address Utilities

| Function | Signature | Description |
|----------|-----------|-------------|
| `ip4` | `fn ip4(a as i64, b as i64, c as i64, d as i64) as i64` | Build host-order IPv4 address from 4 octets |
| `ip4_format` | `fn ip4_format(ip as i64, buf as *u8) as i64` | Format IP as `"a.b.c.d"` into buffer, returns length written |
| `ip4_parse` | `fn ip4_parse(s as *u8) as i64` | Parse `"a.b.c.d"` string to host-order i64. Returns -1 on error |

### Address Construction

| Function | Signature | Description |
|----------|-----------|-------------|
| `addr_init` | `fn addr_init(buf as *u8, ip as i64, port as i64) as Addr` | Create Addr from host-order IP and port into a `[16]u8` buffer |
| `addr_any` | `fn addr_any(buf as *u8, port as i64) as Addr` | Create Addr bound to all interfaces (`INADDR_ANY`) |
| `addr_loopback` | `fn addr_loopback(buf as *u8, port as i64) as Addr` | Create Addr for loopback (`127.0.0.1`) |
| `addr_port` | `fn addr_port(a as Addr) as i64` | Extract port (host byte order) from Addr |
| `addr_ip` | `fn addr_ip(a as Addr) as i64` | Extract IP (host byte order) from Addr |

### TCP Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `tcp_socket` | `fn tcp_socket() as i64` | Create a TCP socket. Returns fd or negative error |
| `tcp_listen` | `fn tcp_listen(a as Addr) as i64` | Bind + listen on address with `SO_REUSEADDR`. Returns server fd or negative error |
| `tcp_accept` | `fn tcp_accept(server_fd as i64, client_buf as *u8) as i64` | Accept connection. Fills `client_buf` (16 bytes) if non-null. Returns client fd |
| `tcp_connect` | `fn tcp_connect(a as Addr) as i64` | Connect to address. Returns fd or negative error |
| `send` | `fn send(fd as i64, buf as *u8, len as i64) as i64` | Send data. Returns bytes sent or negative error |
| `recv` | `fn recv(fd as i64, buf as *u8, len as i64) as i64` | Receive data. Returns bytes received, 0 for EOF, or negative error |
| `send_all` | `fn send_all(fd as i64, buf as *u8, len as i64) as i64` | Loop until all bytes sent. Returns total sent or negative on first error |
| `recv_all` | `fn recv_all(fd as i64, buf as *u8, len as i64) as i64` | Loop until exactly `len` bytes received. Returns total or negative on error |
| `send_str` | `fn send_str(fd as i64, s as *u8) as i64` | Send a null-terminated string (computes length, then calls `send_all`) |
| `send_string` | `fn send_string(fd as i64, s as str.String) as i64` | Send a Caustic `String` via `send_all` |
| `recv_string` | `fn recv_string(fd as i64, max_len as i64) as str.String` | Receive into a heap-allocated `String` (up to `max_len` bytes). Returns empty String on error |

### UDP Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `udp_socket` | `fn udp_socket() as i64` | Create a UDP socket. Returns fd or negative error |
| `udp_bind` | `fn udp_bind(a as Addr) as i64` | Bind UDP socket to address. Returns fd or negative error |
| `udp_send` | `fn udp_send(fd as i64, buf as *u8, len as i64, a as Addr) as i64` | Send datagram to address. Returns bytes sent or negative error |
| `udp_recv` | `fn udp_recv(fd as i64, buf as *u8, len as i64, sender_buf as *u8) as i64` | Receive datagram. Fills `sender_buf` (16 bytes) if non-null. Returns bytes received |
| `udp_send_str` | `fn udp_send_str(fd as i64, s as *u8, a as Addr) as i64` | Send null-terminated string as UDP datagram |

### Socket Options

| Function | Signature | Description |
|----------|-----------|-------------|
| `set_reuseaddr` | `fn set_reuseaddr(fd as i64) as i64` | Enable `SO_REUSEADDR` on socket |
| `set_reuseport` | `fn set_reuseport(fd as i64) as i64` | Enable `SO_REUSEPORT` on socket |
| `set_nodelay` | `fn set_nodelay(fd as i64) as i64` | Enable `TCP_NODELAY` (disable Nagle's algorithm) |
| `set_keepalive` | `fn set_keepalive(fd as i64) as i64` | Enable `SO_KEEPALIVE` on socket |
| `set_recv_timeout` | `fn set_recv_timeout(fd as i64, secs as i64) as i64` | Set receive timeout in seconds |
| `set_send_timeout` | `fn set_send_timeout(fd as i64, secs as i64) as i64` | Set send timeout in seconds |
| `set_nonblock` | `fn set_nonblock(fd as i64) as i64` | Set socket to non-blocking mode via `fcntl` |
| `close` | `fn close(fd as i64) as void` | Close a socket file descriptor |

### Poll

| Function | Signature | Description |
|----------|-----------|-------------|
| `pollfd_init` | `fn pollfd_init(pfd as *PollFd, fd as i64, events as i64) as void` | Initialize a `PollFd` with fd and event mask |
| `pollfd_ready` | `fn pollfd_ready(pfd as *PollFd, event as i64) as i64` | Check if event occurred in `revents`. Returns 1 if set, 0 otherwise |
| `net_poll` | `fn net_poll(fds as *PollFd, count as i64, timeout_ms as i64) as i64` | Poll array of `PollFd`. timeout_ms=-1 infinite, 0 non-blocking. Returns ready count |
| `wait_readable` | `fn wait_readable(fd as i64, timeout_ms as i64) as i64` | Wait for single fd to be readable. Returns 1 ready, 0 timeout, -1 error |
| `wait_writable` | `fn wait_writable(fd as i64, timeout_ms as i64) as i64` | Wait for single fd to be writable. Returns 1 ready, 0 timeout, -1 error |

## Example

A minimal TCP echo server that accepts one connection and echoes back what it receives:

```cst
use "std/net.cst" as net;
use "std/io.cst" as io;

fn main() as i32 {
    // Bind to 0.0.0.0:8080
    let is [16]u8 as buf;
    let is net.Addr as addr = net.addr_any(&buf, 8080);

    let is i64 as server = net.tcp_listen(addr);
    if (server < 0) {
        io.puts("listen failed\n");
        return 1;
    }

    io.puts("listening on :8080\n");

    // Accept one client
    let is [16]u8 as client_addr;
    let is i64 as client = net.tcp_accept(server, &client_addr);
    if (client < 0) {
        io.puts("accept failed\n");
        net.close(server);
        return 1;
    }

    // Echo loop
    let is [1024]u8 as data;
    let is i64 as n with mut = net.recv(client, &data, 1024);
    while (n > 0) {
        net.send_all(client, &data, n);
        n = net.recv(client, &data, 1024);
    }

    net.close(client);
    net.close(server);
    return 0;
}
```
