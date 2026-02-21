# FFI (Foreign Function Interface)

Caustic can call C library functions using `extern fn` declarations and dynamic linking.

## Declaring External Functions

```cst
extern fn printf(fmt as *u8) as i32;
extern fn malloc(size as i64) as *u8;
extern fn free(ptr as *u8) as void;
extern fn strlen(s as *u8) as i64;
extern fn puts(s as *u8) as i32;
```

Declare the function signature with `extern fn`. No body is provided -- the symbol is resolved at link time.

## Building with libc

```bash
# Compile
./caustic program.cst

# Assemble
./caustic-as program.cst.s

# Link with libc (dynamic linking)
./caustic-ld program.cst.s.o -lc -o program
```

The `-lc` flag tells the linker to link against `libc.so.6` dynamically. The resulting binary uses the system's dynamic linker (`/lib64/ld-linux-x86-64.so.2`).

## Complete Example: Hello with printf

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

## Using malloc/free

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

## Multi-Object Linking with FFI

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

## String Compatibility

Caustic string literals are null-terminated, so they are directly compatible with C functions that expect `const char*`. No conversion is needed.

## Gotcha: stdio Buffering

When using libc's `printf` or `puts`, output is buffered. The buffer is flushed when:
- A newline is written (for stdout, if connected to a terminal)
- `fflush` is called
- The program calls libc's `exit()` (NOT `syscall(60, ...)`)

If your output is missing, make sure your program returns from `main` normally (which calls libc's exit) rather than using a raw exit syscall.
