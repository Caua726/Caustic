## STDIN
let is i64 as STDIN with imut = 0

Standard input file descriptor.

Use with io.read(), io.Reader(), or linux.read() to read user input.

Example:
  let is [256]u8 as buf;
  let is i64 as n = linux.read(linux.STDIN, &buf, 256);
---
## STDOUT
let is i64 as STDOUT with imut = 1

Standard output file descriptor.

Use with io.write_str(), io.write_int(), or linux.write() to print output.

Example:
  io.write_str(linux.STDOUT, "hello world\n");
  linux.write(linux.STDOUT, "raw\n", 4);
---
## STDERR
let is i64 as STDERR with imut = 2

Standard error file descriptor.

Use for error messages and diagnostics. Not buffered.

Example:
  io.write_str(linux.STDERR, "error: something went wrong\n");
---
## O_RDONLY
let is i64 as O_RDONLY with imut = 0

Open file for reading only. Used with linux.open().

Example:
  let is i64 as fd = linux.open("file.txt", linux.O_RDONLY, 0);
---
## O_WRONLY
let is i64 as O_WRONLY with imut = 1

Open file for writing only. Used with linux.open().
Combine with O_CREAT and O_TRUNC to create/overwrite a file.

Example:
  let is i64 as fd = linux.open("out.txt", linux.O_WRONLY + linux.O_CREAT + linux.O_TRUNC, 438);
---
## O_RDWR
let is i64 as O_RDWR with imut = 2

Open file for both reading and writing.
---
## O_CREAT
let is i64 as O_CREAT with imut = 64

Create the file if it does not exist. Requires a mode argument (e.g. 438 = 0666).
Combine with O_WRONLY or O_RDWR.
---
## O_TRUNC
let is i64 as O_TRUNC with imut = 512

Truncate (empty) the file if it already exists. Combine with O_WRONLY.
---
## O_APPEND
let is i64 as O_APPEND with imut = 1024

All writes go to the end of the file. Useful for log files.
---
## SEEK_SET
let is i64 as SEEK_SET with imut = 0

Seek from the beginning of the file. Used with linux.lseek().
---
## SEEK_CUR
let is i64 as SEEK_CUR with imut = 1

Seek from the current file position.
---
## SEEK_END
let is i64 as SEEK_END with imut = 2

Seek from the end of the file. Use linux.lseek(fd, 0, SEEK_END) to get file size.
---
## read
fn read(fd as i64, buf as *u8, count as i64) as i64

Read up to 'count' bytes from file descriptor into buffer.

Parameters:
  fd    - open file descriptor
  buf   - destination buffer (must be large enough)
  count - maximum bytes to read

Returns: number of bytes actually read, 0 on EOF, negative on error.

Example:
  let is [1024]u8 as buf;
  let is i64 as n = linux.read(fd, &buf, 1024);
---
## write
fn write(fd as i64, buf as *u8, count as i64) as i64

Write 'count' bytes from buffer to file descriptor.

Parameters:
  fd    - open file descriptor (STDOUT, STDERR, or opened file)
  buf   - source buffer
  count - number of bytes to write

Returns: number of bytes written, negative on error.

Example:
  linux.write(linux.STDOUT, "hello\n", 6);
---
## open
fn open(path as *u8, flags as i64, mode as i64) as i64

Open or create a file.

Parameters:
  path  - null-terminated file path
  flags - O_RDONLY, O_WRONLY, O_RDWR, combined with O_CREAT, O_TRUNC, O_APPEND
  mode  - file permissions (438 = 0o666, 493 = 0o755). Only used with O_CREAT.

Returns: file descriptor (>= 0) on success, negative on error.

Example:
  let is i64 as fd = linux.open("data.txt", linux.O_RDONLY, 0);
  if (fd < 0) { io.write_str(linux.STDERR, "open failed\n"); }
---
## close
fn close(fd as i64) as i64

Close a file descriptor. Always close files when done.

Returns: 0 on success.

Example:
  linux.close(fd);
---
## lseek
fn lseek(fd as i64, offset as i64, whence as i64) as i64

Reposition the file read/write offset.

Parameters:
  fd     - open file descriptor
  offset - byte offset
  whence - SEEK_SET (from start), SEEK_CUR (from current), SEEK_END (from end)

Returns: new absolute offset, negative on error.

Example:
  let is i64 as size = linux.lseek(fd, 0, linux.SEEK_END);  // get file size
  linux.lseek(fd, 0, linux.SEEK_SET);                        // rewind to start
---
## mmap
fn mmap(addr as *u8, length as i64, prot as i64, flags as i64, fd as i64, offset as i64) as *u8

Map a file or anonymous memory into the process address space.

Parameters:
  addr   - hint address (cast(*u8, 0) to let kernel choose)
  length - size in bytes
  prot   - PROT_READ, PROT_WRITE, PROT_EXEC (combine with +)
  flags  - MAP_PRIVATE, MAP_SHARED, MAP_ANONYMOUS
  fd     - file descriptor (-1 for anonymous)
  offset - offset in file

Returns: pointer to mapped region, or MAP_FAILED on error.

Example:
  // Allocate 4096 bytes of anonymous memory
  let is *u8 as p = linux.mmap(cast(*u8, 0), 4096, 3, 34, cast(i64, 0 - 1), 0);
  // 3 = PROT_READ + PROT_WRITE, 34 = MAP_PRIVATE + MAP_ANONYMOUS
---
## munmap
fn munmap(addr as *u8, length as i64) as i64

Unmap a previously mapped memory region.

Example:
  linux.munmap(ptr, 4096);
---
## exit
fn exit(code as i32) as void

Terminate the process immediately with the given exit code.
Does NOT run deferred calls. Prefer returning from main().

Example:
  linux.exit(1);  // exit with error
---
## fork
fn fork() as i64

Create a child process (copy of the current process).

Returns: child PID in parent, 0 in child, negative on error.

Example:
  let is i64 as pid = linux.fork();
  if (pid == 0) {
      // child process
  } else {
      // parent process, pid = child's PID
  }
---
## execve
fn execve(path as *u8, argv as *u8, envp as *u8) as i64

Replace the current process with a new program.

Parameters:
  path - path to executable
  argv - pointer to null-terminated array of argument pointers
  envp - pointer to null-terminated array of environment pointers

Returns: only on error (negative). On success, never returns.
---
## wait4
fn wait4(pid as i64, status as *u8, options as i64, rusage as *u8) as i64

Wait for a child process to change state.

Parameters:
  pid     - child PID to wait for (-1 for any child)
  status  - pointer to i32 to receive exit status
  options - 0 for blocking wait
  rusage  - resource usage (cast(*u8, 0) to ignore)

Returns: PID of the child that changed state.
---
## pipe
fn pipe(pipefd as *i32) as i64

Create a unidirectional pipe.

Parameters:
  pipefd - pointer to [2]i32 array. pipefd[0] = read end, pipefd[1] = write end.

Returns: 0 on success.

Example:
  let is [2]i32 as fds;
  linux.pipe(&fds);
  // fds[0] = read end, fds[1] = write end
---
## dup2
fn dup2(oldfd as i64, newfd as i64) as i64

Duplicate a file descriptor onto another.
Commonly used to redirect stdin/stdout/stderr.

Example:
  linux.dup2(pipe_write, linux.STDOUT);  // redirect stdout to pipe
---
## stat
fn stat(path as *u8, statbuf as *u8) as i64

Get file information (size, permissions, timestamps).
The statbuf is a 144-byte buffer matching struct stat.

Returns: 0 on success, negative on error.
---
## mkdir
fn mkdir(path as *u8, mode as i64) as i64

Create a directory.

Parameters:
  path - directory path
  mode - permissions (493 = 0o755)
---
## unlink
fn unlink(path as *u8) as i64

Delete a file.
---
## rename
fn rename(oldpath as *u8, newpath as *u8) as i64

Rename or move a file.
---
## getcwd
fn getcwd(buf as *u8, size as i64) as i64

Get the current working directory.

Example:
  let is [4096]u8 as cwd;
  linux.getcwd(&cwd, 4096);
---
## clock_gettime
fn clock_gettime(clk_id as i64, tp as *u8) as i64

Get current time. tp points to [2]i64: [seconds, nanoseconds].

Parameters:
  clk_id - CLOCK_REALTIME (0) for wall time, CLOCK_MONOTONIC (1) for elapsed time

Example:
  let is [2]i64 as ts;
  linux.clock_gettime(1, cast(*u8, &ts));  // monotonic
  // ts[0] = seconds, ts[1] = nanoseconds
---
## nanosleep
fn nanosleep(req as *u8, rem as *u8) as i64

Sleep for a specified duration. req points to [2]i64: [seconds, nanoseconds].

Example:
  let is [2]i64 as ts;
  ts[0] = 0; ts[1] = 500000000;  // 500ms
  linux.nanosleep(cast(*u8, &ts), cast(*u8, 0));
---
## socket
fn socket(domain as i64, stype as i64, protocol as i64) as i64

Create a network socket.

Parameters:
  domain   - AF_INET (2) for IPv4
  stype    - SOCK_STREAM (1) for TCP, SOCK_DGRAM (2) for UDP
  protocol - 0 for default

Returns: socket file descriptor.
---
## getrandom
fn getrandom(buf as *u8, count as i64, flags as i64) as i64

Fill buffer with cryptographically secure random bytes.

Example:
  let is [32]u8 as entropy;
  linux.getrandom(&entropy, 32, 0);
---
