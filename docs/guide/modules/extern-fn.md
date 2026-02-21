# Extern Fn

Declare a function that is defined elsewhere -- in another Caustic object file or in a shared library.

## Syntax

```cst
extern fn name(param as Type, ...) as ReturnType;
```

No body is provided. The linker resolves the symbol at link time.

## Cross-object references

When splitting a project across multiple files compiled with `-c`, use `extern fn` to reference functions from other translation units.

```cst
// math.cst — compiled with ./caustic -c math.cst
fn square(x as i64) as i64 {
    return x * x;
}
```

```cst
// main.cst
extern fn square(x as i64) as i64;

fn main() as i32 {
    let is i64 as result = square(7);
    // result == 49
    return cast(i32, result);
}
```

```sh
./caustic -c math.cst && ./caustic main.cst
./caustic-as math.cst.s && ./caustic-as main.cst.s
./caustic-ld math.cst.s.o main.cst.s.o -o program
```

## C FFI with dynamic linking

Use `extern fn` with the `-lc` linker flag to call C library functions.

```cst
extern fn puts(s as *u8) as i32;
extern fn exit(code as i32) as void;

fn main() as i32 {
    puts("hello from caustic");
    exit(0);
    return 0;
}
```

```sh
./caustic main.cst
./caustic-as main.cst.s
./caustic-ld main.cst.s.o -lc -o program
./program
```

## Rules

- The signature must exactly match the definition (parameter types, return type, number of arguments).
- Follows the System V AMD64 ABI: integer/pointer args in rdi, rsi, rdx, rcx, r8, r9; floats in xmm0-xmm7.
- `extern fn` does not support variadic arguments in Caustic's type system, but the call will work at the ABI level if you pass the correct number of arguments.
- You cannot use `extern fn` with `syscall` -- use the `syscall()` builtin directly instead.
