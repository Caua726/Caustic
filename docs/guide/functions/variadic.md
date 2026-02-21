# Variadic Functions

Variadic functions accept a variable number of arguments using `...` as the last parameter.

## Syntax

```cst
fn name(fixed_param as Type, ...) as ReturnType {
    // body
}
```

The `...` must come after all fixed parameters.

## C Interop

Variadic functions are primarily used with `extern fn` to call C library functions like `printf`:

```cst
extern fn printf(fmt as *u8, ...) as i32;

fn main() as i32 {
    printf("value: %d\n", 42);
    printf("%s has %d items\n", "list", 5);
    return 0;
}
```

This requires linking with libc (use `-lc` with the linker).

## Standard Library Usage

Caustic's `std/io.cst` provides a variadic `printf` that works without libc, using direct syscalls:

```cst
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("hello %s, you are %d years old\n", "world", 25);
    return 0;
}
```

## Calling Convention

Variadic arguments follow the System V AMD64 ABI. Integer and pointer arguments go in rdi, rsi, rdx, rcx, r8, r9 (after fixed params), then overflow to the stack. The `al` register must hold the number of SSE registers used (relevant for float args).

## Limitations

- You cannot define a variadic function with custom argument processing in pure Caustic without using inline assembly or VaList internals.
- Variadic functions are best used for `extern fn` declarations calling into C or for the standard library's built-in printf.
