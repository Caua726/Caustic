# Advanced

## Cast

Caustic requires explicit type conversions using `cast(TargetType, expression)`. There are no implicit casts between different types (except literal narrowing, e.g. an `i64` literal assigned to an `i32` variable).

### Syntax

```cst
cast(TargetType, expression)
```

### Integer Size Conversions

Widen or narrow between integer types:

```cst
let is i64 as big = 1000;
let is i32 as small = cast(i32, big);
let is i64 as back = cast(i64, small);
```

### Integer to Pointer

Convert a raw address to a typed pointer:

```cst
let is i64 as addr = 0x7fff0000;
let is *u8 as ptr = cast(*u8, addr);
```

### Pointer to Integer

Convert a pointer back to an integer for arithmetic or comparison:

```cst
let is *u8 as ptr = mem.galloc(64);
let is i64 as addr = cast(i64, ptr);
```

### Between Pointer Types

Reinterpret a pointer as a different type:

```cst
let is *u8 as raw = mem.galloc(sizeof(Point));
let is *Point as p = cast(*Point, raw);
```

### Common Pattern: Typed Heap Allocation

```cst
use "std/mem.cst" as mem;

struct Node {
    value as i64;
    next as *u8;
}

fn new_node(val as i64) as *Node {
    let is *u8 as raw = mem.galloc(sizeof(Node));
    let is *Node as n = cast(*Node, raw);
    n.value = val;
    n.next = cast(*u8, 0);
    return n;
}
```

### Rules

- `cast` does not perform any runtime checks -- it is a raw reinterpretation.
- You cannot cast between floats and integers directly. Assign through intermediate variables if needed.
- The result of `cast` cannot be used with member access directly. Split into two statements:

```cst
// WRONG: cast(*Point, raw).x
// RIGHT:
let is *Point as p = cast(*Point, raw);
p.x = 10;
```

## Sizeof

`sizeof(Type)` returns the size of a type in bytes as an `i64`.

### Syntax

```cst
sizeof(Type)
```

### Primitive Sizes

```cst
sizeof(i8)    // 1
sizeof(i16)   // 2
sizeof(i32)   // 4
sizeof(i64)   // 8
sizeof(u8)    // 1
sizeof(u16)   // 2
sizeof(u32)   // 4
sizeof(u64)   // 8
sizeof(f32)   // 4
sizeof(f64)   // 8
sizeof(bool)  // 1
sizeof(char)  // 1
sizeof(*u8)   // 8 (all pointers are 8 bytes on x86_64)
```

### Struct Sizes

Caustic structs are **packed** -- there is no alignment padding between fields.

```cst
struct Point {
    x as i32;
    y as i32;
}
// sizeof(Point) = 8 (4 + 4)

struct Mixed {
    a as i32;
    b as i64;
    c as i32;
}
// sizeof(Mixed) = 20 (4 + 8 + 4), NOT 24
```

This differs from C, where `Mixed` would be 24 bytes due to alignment padding.

### Common Use: Heap Allocation

```cst
use "std/mem.cst" as mem;

struct Buffer {
    data as *u8;
    len as i64;
    cap as i64;
}

fn new_buffer(cap as i64) as *Buffer {
    let is *u8 as raw = mem.galloc(sizeof(Buffer));
    let is *Buffer as buf = cast(*Buffer, raw);
    buf.data = mem.galloc(cap);
    buf.len = 0;
    buf.cap = cap;
    return buf;
}
```

### Array Sizing

Calculate total bytes for N elements:

```cst
let is i64 as count = 100;
let is *u8 as arr = mem.galloc(count * sizeof(i64));
```

### Gotcha: Packed Structs

Because structs are packed, mixing field sizes can cause issues with C FFI. If you need C-compatible layout, use all-`i64` fields or manually add padding fields:

```cst
// C-compatible layout for { int a; double b; }
struct CCompat {
    a as i32;
    _pad as i32;   // manual padding to match C alignment
    b as f64;
}
// sizeof(CCompat) = 16, matches C layout
```

## Inline Assembly

`asm("instructions\n")` emits raw x86_64 assembly directly into the compiler output.

### Syntax

```cst
asm("instruction\n");
```

The string is inserted verbatim into the generated `.s` file. Each instruction must end with `\n`.

### Basic Examples

```cst
// Single instruction
asm("nop\n");

// Move immediate to register
asm("mov rax, 1\n");

// Multiple instructions
asm("push rbx\npush rcx\npop rcx\npop rbx\n");
```

### Practical Example: CPUID

```cst
fn get_cpu_vendor() as void {
    asm("mov eax, 0\n");
    asm("cpuid\n");
    // ebx:edx:ecx now contain vendor string
}
```

### Practical Example: Read Timestamp Counter

```cst
fn rdtsc_low() as i64 {
    asm("rdtsc\n");
    // Result is in edx:eax, rax has the low 32 bits
    // The return value convention uses rax
    return 0;  // rax already holds the value from rdtsc
}
```

### Warnings

- **Register clobbering**: The compiler does not track which registers inline assembly uses. If you modify registers that the compiler expects to be preserved (rbx, r12-r15, rbp, rsp), your program will break.
- **No input/output constraints**: Unlike GCC's `asm()` with constraints, Caustic simply emits the string. You must manually coordinate with the compiler's register allocation.
- **Calling convention**: If used inside a function, be aware that the compiler may have values in any GPR or SSE register.
- **Syntax**: Uses Intel syntax (destination first), matching the rest of Caustic's assembly output.

### When to Use

- Performance-critical tight loops
- CPU feature detection (cpuid, rdtsc)
- Atomic operations not exposed by the language
- Debugging (e.g., `asm("int3\n")` for a breakpoint)

In most cases, prefer `syscall()` for OS interaction and normal Caustic code for everything else.

## Syscalls

`syscall(number, arg1, arg2, ...)` performs a direct Linux kernel system call. Caustic programs have no libc -- syscalls are the only way to interact with the OS.

### Syntax

```cst
let is i64 as result = syscall(number, arg1, arg2, ...);
```

Returns an `i64`. Negative values indicate errors (negated errno).

### Common Syscall Numbers (x86_64 Linux)

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

### Examples

**Write to stdout:**

```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

**Read from stdin:**

```cst
fn main() as i32 {
    let is [128]u8 as buf;
    let is i64 as n = syscall(0, 0, &buf, 128);
    // n = number of bytes read
    syscall(1, 1, &buf, n);
    return 0;
}
```

**Open, Read, Close a File:**

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

**Allocate Memory with mmap:**

```cst
fn alloc_page() as *u8 {
    // mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    let is i64 as ptr = syscall(9, 0, 4096, 3, 34, 0 - 1, 0);
    return cast(*u8, ptr);
}
```

### Standard Library Wrappers

For cleaner code, use `std/linux.cst` which provides named wrappers:

```cst
use "std/linux.cst" as linux;

fn main() as i32 {
    linux.write(1, "Hello\n", 6);
    linux.exit(0);
    return 0;
}
```

### Error Handling

Syscall return values of `-1` through `-4095` indicate errors. The absolute value is the errno:

```cst
let is i64 as fd = syscall(2, "/nonexistent", 0, 0);
if fd < 0 {
    // fd is -errno, e.g. -2 for ENOENT
    syscall(1, 2, "error\n", 6);
    return 1;
}
```

## Defer

`defer` schedules a function call to execute automatically before every `return` in the current scope.

### Syntax

```cst
defer function_call(args);
```

### Basic Example

```cst
use "std/mem.cst" as mem;

fn work() as i32 {
    let is *u8 as ptr = mem.galloc(1024);
    defer mem.gfree(ptr);

    // ... use ptr ...

    return 0;  // mem.gfree(ptr) runs automatically here
}
```

### Rules

1. **Only function calls**: `defer` only accepts function call expressions. You cannot defer arbitrary statements.

```cst
// VALID:
defer mem.gfree(ptr);
defer cleanup();

// INVALID:
// defer syscall(3, fd);    -- syscall is not a regular function
// defer x = 0;             -- not a function call
```

2. **LIFO order**: Multiple defers execute in reverse order (last in, first out).

```cst
fn example() as i32 {
    defer print_a();   // runs third
    defer print_b();   // runs second
    defer print_c();   // runs first
    return 0;
}
```

3. **Return value evaluated first**: The return expression is computed before defers run.

```cst
fn compute() as i64 {
    let is i64 as result = 42;
    defer modify_something();
    return result;  // returns 42, then modify_something() runs
}
```

4. **Block scoping**: Defers inside `if`, `else`, `while`, `for`, or `match` blocks are scoped to that block.

```cst
fn example() as i32 {
    if true {
        let is *u8 as p = mem.galloc(64);
        defer mem.gfree(p);  // only runs when returning from inside this if block
    }
    return 0;
}
```

### Workaround for syscall

Since `defer syscall(...)` is not supported, wrap the syscall in a helper function:

```cst
fn close_fd(fd as i64) as void {
    syscall(3, fd);
}

fn read_file() as i32 {
    let is i64 as fd = syscall(2, "/tmp/data", 0, 0);
    defer close_fd(fd);
    // ... read from fd ...
    return 0;
}
```

## Defer Patterns

Common patterns for using `defer` to manage resources in Caustic.

### Allocate / Free

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

### Open / Close

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

### Multiple Resources (LIFO)

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

### Constructor Pattern

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

### Cleanup Guards in Loops

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

## Function Pointers and Indirect Calls

Caustic supports both creating function pointers (`fn_ptr`) and calling through them (`call`).

### Creating Function Pointers

`fn_ptr(name)` returns the address of a function. The result carries type information (TY_FN) with parameter types and return type, enabling compile-time type checking when used with `call()`.

```cst
let is *u8 as fp = fn_ptr(function_name);
```

### Local Functions

```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(add);
    return 0;
}
```

### Module Functions

```cst
use "utils.cst" as utils;

fn main() as i32 {
    let is *u8 as fp = fn_ptr(utils.process);
    return 0;
}
```

### Generic Instantiations

Specify the type parameter to get the monomorphized version:

```cst
fn max gen T (a as T, b as T) as T {
    if (a > b) { return a; }
    return b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(max gen i32);
    // fp points to max_i32
    return 0;
}
```

### Impl Methods

Use the mangled name (TypeName_method):

```cst
struct Point { x as i32; y as i32; }

impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(Point_sum);
    return 0;
}
```

### Calling Through Function Pointers

Use `call(fn_ptr_expr, args...)` to call through a function pointer:

```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(add);
    let is i64 as result = call(fp, 3, 4);  // returns 7
    return cast(i32, result);
}
```

### Type-Checked Calls

Since `fn_ptr()` carries the function's type signature, the compiler checks argument count and types at compile time:

```cst
fn compare(a as i64, b as i64) as i32 {
    if (a < b) { return cast(i32, 0 - 1); }
    if (a > b) { return 1; }
    return 0;
}

fn main() as i32 {
    let is *u8 as cmp = fn_ptr(compare);

    // Compiler knows cmp takes (i64, i64) and returns i32
    let is i32 as r = call(cmp, 10, 20);  // OK: types match
    return r;
}
```

### Practical Example: Custom Sorting

```cst
use "std/sort.cst" as sort;

fn reverse_cmp(a as i64, b as i64) as i32 {
    if (a > b) { return cast(i32, 0 - 1); }
    if (a < b) { return 1; }
    return 0;
}

fn main() as i32 {
    let is [5]i64 as arr;
    arr[0] = 5; arr[1] = 3; arr[2] = 1; arr[3] = 4; arr[4] = 2;

    // sort with custom comparator
    sort.sort_i64(&arr, 5, fn_ptr(reverse_cmp));
    // arr is now [5, 4, 3, 2, 1]

    return 0;
}
```

### Passing Callbacks to C (FFI)

Function pointers work with `extern fn` for C interop:

```cst
extern fn qsort(base as *u8, nmemb as i64, size as i64, compar as *u8) as void;

fn compare_ints(a as *i32, b as *i32) as i32 {
    return *a - *b;
}

fn main() as i32 {
    let is [5]i32 as arr;
    arr[0] = 5; arr[1] = 3; arr[2] = 1; arr[3] = 4; arr[4] = 2;
    qsort(cast(*u8, &arr), 5, 4, fn_ptr(compare_ints));
    return 0;
}
```

## FFI

Caustic can call C library functions using `extern fn` declarations and dynamic linking.

### Declaring External Functions

```cst
extern fn printf(fmt as *u8) as i32;
extern fn malloc(size as i64) as *u8;
extern fn free(ptr as *u8) as void;
extern fn strlen(s as *u8) as i64;
extern fn puts(s as *u8) as i32;
```

Declare the function signature with `extern fn`. No body is provided -- the symbol is resolved at link time.

### Building with libc

```bash
# Compile
./caustic program.cst

# Assemble
./caustic-as program.cst.s

# Link with libc (dynamic linking)
./caustic-ld program.cst.s.o -lc -o program
```

The `-lc` flag tells the linker to link against `libc.so.6` dynamically. The resulting binary uses the system's dynamic linker (`/lib64/ld-linux-x86-64.so.2`).

### Complete Example: Hello with printf

```cst
extern fn printf(fmt as *u8) as i32;

fn main() as i32 {
    printf("Hello from Caustic!\n");
    return 0;
}
```

```bash
./caustic hello.cst && ./caustic-as hello.cst.s && ./caustic-ld hello.cst.s.o -lc -o hello
./hello
```

### Using malloc/free

```cst
extern fn malloc(size as i64) as *u8;
extern fn free(ptr as *u8) as void;
extern fn memcpy(dst as *u8, src as *u8, n as i64) as *u8;

fn main() as i32 {
    let is *u8 as buf = malloc(256);
    memcpy(buf, "Hello\n", 6);
    syscall(1, 1, buf, 6);
    free(buf);
    return 0;
}
```

### Multi-Object Linking with FFI

Compile library and main separately, then link together with libc:

```cst
// lib.cst
extern fn printf(fmt as *u8) as i32;

fn greet(name as *u8) as void {
    printf("Hello, ");
    printf(name);
    printf("!\n");
}
```

```cst
// main.cst
extern fn greet(name as *u8) as void;

fn main() as i32 {
    greet("World");
    return 0;
}
```

```bash
./caustic -c lib.cst && ./caustic main.cst
./caustic-as lib.cst.s && ./caustic-as main.cst.s
./caustic-ld lib.cst.s.o main.cst.s.o -lc -o program
```

### String Compatibility

Caustic string literals are null-terminated, so they are directly compatible with C functions that expect `const char*`. No conversion is needed.

### Gotcha: stdio Buffering

When using libc's `printf` or `puts`, output is buffered. The buffer is flushed when:
- A newline is written (for stdout, if connected to a terminal)
- `fflush` is called
- The program calls libc's `exit()` (NOT `syscall(60, ...)`)

If your output is missing, make sure your program returns from `main` normally (which calls libc's exit) rather than using a raw exit syscall.

## FFI Struct Passing

When passing structs to or from C functions, Caustic follows the System V AMD64 ABI classification rules.

### Classification Rules

**Small structs (1-8 bytes): Register**

Passed in a single GPR (INTEGER class) or XMM register (SSE class):

```cst
struct Small {
    a as i32;
    b as i32;
}
// 8 bytes -> passed in one register (e.g., rdi)

extern fn use_small(s as Small) as void;

fn main() as i32 {
    let is Small as s;
    s.a = 10;
    s.b = 20;
    use_small(s);
    return 0;
}
```

**Medium structs (9-16 bytes): Two Registers**

Split into two 8-byte halves, each independently classified:

```cst
struct Medium {
    x as i64;
    y as i64;
}
// 16 bytes -> passed in two registers (rdi, rsi)
```

**Large structs (>16 bytes): Memory**

Caller allocates a stack buffer, copies the struct, and passes a pointer:

```cst
struct Large {
    a as i64;
    b as i64;
    c as i64;
}
// 24 bytes -> passed via hidden pointer
```

### Return Values

- 1-8 bytes: returned in `rax` (INTEGER) or `xmm0` (SSE)
- 9-16 bytes: returned in `rax` + `rdx`
- Greater than 16 bytes: caller passes a hidden pointer (SRET), callee writes the result there

### Packed Struct Gotcha

Caustic structs are **packed** (no alignment padding). C structs have alignment padding. This mismatch causes problems:

```cst
// Caustic: 12 bytes (packed)
struct CausticMixed {
    a as i32;   // offset 0, 4 bytes
    b as f64;   // offset 4, 8 bytes
}

// C equivalent: 16 bytes (padded)
// struct CMixed {
//     int a;      // offset 0, 4 bytes
//     // 4 bytes padding
//     double b;   // offset 8, 8 bytes
// };
```

The Caustic struct is 12 bytes. The C struct is 16 bytes. Passing a Caustic `CausticMixed` to a C function expecting `CMixed` will read incorrect field values.

### Workaround: Match C Layout Manually

Add padding fields to match C alignment:

```cst
struct CCompatMixed {
    a as i32;
    _pad as i32;    // 4 bytes padding to match C alignment
    b as f64;
}
// Now 16 bytes, matches C layout
```

### Safe Patterns for C Interop

Use all-`i64` structs to avoid alignment issues:

```cst
struct SafePoint {
    x as i64;
    y as i64;
}
// 16 bytes, same layout in Caustic and C (assuming C uses long/int64_t)
```

Or use pointer-based passing for complex structs:

```cst
extern fn process_data(ptr as *u8, len as i64) as i32;
```

### Classification Edge Case

If a field crosses the 8-byte boundary in a packed struct, the entire struct is classified as MEMORY (passed via pointer), regardless of total size. This is the correct System V ABI behavior for unaligned fields.

## C Interop Types

The `std/compatcffi.cst` module provides helpers for building C-compatible struct layouts with proper alignment padding.

### Import

```cst
use "std/compatcffi.cst" as cffi;
```

### Building C-Compatible Struct Layouts

Caustic structs are packed (no padding), but C structs have alignment padding. `CStruct` lets you describe a C struct layout and access fields correctly.

**Define a Layout:**

```cst
// Equivalent to C: struct { int a; double b; }
let is cffi.CStruct as cs = cffi.new_struct();
cffi.add_i32(&cs);   // field 0: int (4 bytes + 4 padding)
cffi.add_f64(&cs);   // field 1: double (8 bytes)
cffi.finish(&cs);
// Total size: 16 bytes (matching C layout)
```

**Allocate and Access Fields:**

```cst
use "std/mem.cst" as mem;

let is *u8 as buf = mem.galloc(cffi.struct_size(&cs));

// Set field 0 (i32) to 42
cffi.set_i32(buf, &cs, 0, 42);

// Set field 1 (f64) to 3.14
cffi.set_f64(buf, &cs, 1, 3.14);

// Read back
let is i32 as a = cffi.get_i32(buf, &cs, 0);
let is f64 as b = cffi.get_f64(buf, &cs, 1);
```

### C Union Layouts

```cst
let is cffi.CUnion as cu = cffi.new_union();
cffi.union_add_i64(&cu);
cffi.union_add_f64(&cu);
cffi.union_finish(&cu);
// Size = max(sizeof(i64), sizeof(f64)) = 8
```

### C String Helpers

```cst
// Convert Caustic string to C string (null-terminated, allocated)
let is *u8 as cstr = cffi.to_cstr("hello");

// Convert C string back (reads until null byte)
let is *u8 as s = cffi.from_cstr(cstr);
```

### Typed Pointer Arithmetic

```cst
// Advance pointer by index * element_size
let is *u8 as base = mem.galloc(40);
let is *u8 as third = cffi.ptr_add(base, 2, sizeof(i64));
// third points to base + 16
```

### Complete Example: Calling C with a Struct

```cst
use "std/compatcffi.cst" as cffi;
use "std/mem.cst" as mem;

extern fn process_point(p as *u8) as i32;

fn main() as i32 {
    // Build layout for C struct { int x; int y; double z; }
    let is cffi.CStruct as cs = cffi.new_struct();
    cffi.add_i32(&cs);
    cffi.add_i32(&cs);
    cffi.add_f64(&cs);
    cffi.finish(&cs);

    let is *u8 as buf = mem.galloc(cffi.struct_size(&cs));
    defer mem.gfree(buf);

    cffi.set_i32(buf, &cs, 0, 10);
    cffi.set_i32(buf, &cs, 1, 20);
    cffi.set_f64(buf, &cs, 2, 3.14);

    let is i32 as result = process_point(buf);
    return result;
}
```

## String Escapes

Caustic supports standard escape sequences in both string literals and char literals.

### Supported Escapes

| Escape | Value | Description       |
|--------|-------|-------------------|
| `\n`   | 10    | Newline           |
| `\t`   | 9     | Horizontal tab    |
| `\\`   | 92    | Backslash         |
| `\0`   | 0     | Null byte         |
| `\"`   | 34    | Double quote      |

### In String Literals

```cst
// Newline
syscall(1, 1, "Hello\n", 6);

// Tab-separated columns
syscall(1, 1, "Name\tAge\tCity\n", 14);

// Embedded quotes
syscall(1, 1, "She said \"hello\"\n", 17);

// Null byte in the middle
let is [8]u8 as buf;
// String literals are already null-terminated
```

### In Char Literals

```cst
let is char as newline = '\n';    // 10
let is char as tab = '\t';        // 9
let is char as backslash = '\\';  // 92
let is char as null = '\0';       // 0
```

### Multi-line Output

Build multi-line strings with `\n`:

```cst
fn print_help() as void {
    syscall(1, 1, "Usage: program [options]\n\n", 25);
    syscall(1, 1, "Options:\n", 9);
    syscall(1, 1, "  -h\tShow help\n", 16);
    syscall(1, 1, "  -v\tShow version\n", 19);
}
```

### Null-Terminated Strings

All Caustic string literals are automatically null-terminated, which makes them compatible with C functions:

```cst
extern fn puts(s as *u8) as i32;

fn main() as i32 {
    puts("Already null-terminated");
    return 0;
}
```

### Gotcha: Counting Bytes

When passing strings to `syscall(1, ...)` (write), the length parameter counts the bytes in the string, where each escape sequence counts as **one byte**:

```cst
// "Hi\n" = 'H' + 'i' + '\n' = 3 bytes
syscall(1, 1, "Hi\n", 3);
```

## Char Literals

Char literals use single quotes and represent a single byte value.

### Syntax

```cst
let is char as c = 'A';
```

### Type

Char literals have type `char`, which is 1 byte. No cast is needed when assigning to a `char` variable.

### ASCII Values

```cst
let is char as letter = 'A';   // 65
let is char as digit = '0';    // 48
let is char as space = ' ';    // 32
let is char as bang = '!';     // 33
```

### Escape Sequences

Standard escape sequences work inside char literals:

```cst
let is char as newline = '\n';     // 10
let is char as tab = '\t';        // 9
let is char as backslash = '\\';  // 92
let is char as null = '\0';       // 0
```

### Comparisons

Compare characters directly:

```cst
fn is_digit(c as char) as bool {
    return c >= '0' && c <= '9';
}

fn is_alpha(c as char) as bool {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

fn is_whitespace(c as char) as bool {
    return c == ' ' || c == '\t' || c == '\n';
}
```

### Arithmetic

Character arithmetic works for tasks like case conversion or digit parsing:

```cst
fn to_lower(c as char) as char {
    if c >= 'A' && c <= 'Z' {
        return cast(char, cast(i32, c) + 32);
    }
    return c;
}

fn digit_value(c as char) as i32 {
    return cast(i32, c) - cast(i32, '0');
}
```

### In Arrays

```cst
fn main() as i32 {
    let is [5]char as word;
    word[0] = 'H';
    word[1] = 'e';
    word[2] = 'l';
    word[3] = 'l';
    word[4] = 'o';
    syscall(1, 1, &word, 5);
    return 0;
}
```

### Char vs u8

Both `char` and `u8` are 1 byte. Use `char` when working with text and `u8` when working with raw bytes. They may need a cast to convert between them depending on context.

## Block Comments

```cst
/* This is a block comment */
/* Block comments can be /* nested */ */
```
