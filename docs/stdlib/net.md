## _module
std/net.cst — Networking

TCP/UDP socket operations over Linux syscalls.

  tcp_listen(port)    — create server socket
  tcp_connect(ip,port)— connect to server
  accept(fd)          — accept incoming connection
  send(fd, buf, len)  — send data
  recv(fd, buf, len)  — receive data
  poll(fds, n, ms)    — wait for events on multiple fds

Usage:
  use "std/net.cst" as net;
  let is i64 as server = net.tcp_listen(8080);
---
## tcp_listen
fn tcp_listen(port as i32) as i64

Create a TCP server socket, bind to port, and start listening.

Sets SO_REUSEADDR automatically. Listens on all interfaces (0.0.0.0).

Parameters:
  port - port number (1-65535)

Returns: socket fd on success, negative on error.

Example:
  let is i64 as server = net.tcp_listen(8080);
  let is i64 as client = net.accept(server);
---
## tcp_connect
fn tcp_connect(ip as *u8, port as i32) as i64

Connect to a remote TCP server.

Parameters:
  ip   - IPv4 address as null-terminated string (e.g. "127.0.0.1")
  port - port number

Returns: socket fd on success, negative on error.

Example:
  let is i64 as sock = net.tcp_connect("127.0.0.1", 8080);
---
## accept
fn accept(fd as i64) as i64

Accept an incoming connection on a listening socket.

Blocks until a client connects. Returns a new socket fd for the client.
---
## send
fn send(fd as i64, buf as *u8, len as i64) as i64

Send data over a connected socket. Returns bytes sent.
---
## recv
fn recv(fd as i64, buf as *u8, len as i64) as i64

Receive data from a connected socket.

Returns: bytes received, 0 on connection closed, negative on error.
---
## close
fn close(fd as i64) as void

Close a socket.
---
## poll
fn poll(fds as *u8, nfds as i32, timeout_ms as i32) as i32

Wait for events on multiple file descriptors.

Uses Linux poll() syscall. fds is an array of pollfd structs
(each 8 bytes: i32 fd, i16 events, i16 revents).

Parameters:
  timeout_ms - milliseconds to wait (-1 for infinite, 0 for non-blocking)

Returns: number of fds with events, 0 on timeout.
---
