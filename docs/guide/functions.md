# Functions

## Declaration

Functions in Caustic are declared with the `fn` keyword, followed by the name, parameters in parentheses, return type after `as`, and a body in braces.

### Syntax

```cst
fn name(param1 as Type1, param2 as Type2) as ReturnType {
    // body
}
```

### Void Functions

Functions that return nothing use `void` as the return type.

```cst
fn greet() as void {
    syscall(1, 1, "hello\n", 6);
}
```

### Return Type

The return type is specified after `as`. Every non-void function must end with a `return` statement.

```cst
fn add(a as i32, b as i32) as i32 {
    return a + b;
}

fn square(x as i64) as i64 {
    return x * x;
}
```

### Declaration Order

Functions must be declared before they are called, or exist in the same file scope. For cross-file usage, use modules with `use`.

```cst
fn helper() as i32 {
    return 42;
}

fn main() as i32 {
    let is i32 as val = helper();
    return val;
}
```

### No Parameters

Parentheses are still required even with no parameters.

```cst
fn get_zero() as i32 {
    return 0;
}
```

## Parameters

Function parameters are declared with `name as Type` syntax, separated by commas.

### Pass by Value

Primitive types and small structs are copied when passed to functions.

```cst
fn double(x as i32) as i32 {
    return x * 2;
}

fn main() as i32 {
    let is i32 as val = 10;
    let is i32 as result = double(val);
    // val is still 10
    return result;
}
```

### Pass by Pointer

Use pointer parameters to modify values in the caller's scope.

```cst
fn modify(p as *i32) as void {
    *p = 42;
}

fn main() as i32 {
    let is i32 as x = 0;
    modify(&x);
    // x is now 42
    return x;
}
```

### Register Allocation (System V ABI)

The first 6 integer/pointer arguments are passed in registers:

| Argument | Register |
|----------|----------|
| 1st      | rdi      |
| 2nd      | rsi      |
| 3rd      | rdx      |
| 4th      | rcx      |
| 5th      | r8       |
| 6th      | r9       |

Additional arguments are passed on the stack. Float arguments use xmm0-xmm7.

### Array Parameters

Arrays are passed by pointer to the first element.

```cst
fn sum_array(arr as *i32, len as i32) as i32 {
    let is i32 as total = 0;
    let is i32 as i = 0;
    while (i < len) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}
```

### Struct Parameters

Small structs (<=8 bytes) are passed in a register. Larger structs are passed by copying onto the stack or via a hidden pointer.

```cst
struct Point { x as i32; y as i32; }

fn distance(p as Point) as i32 {
    return p.x + p.y;
}
```

## Return Values

### Returning a Value

Use `return expr;` to return a value from a function. The expression type must match the declared return type.

```cst
fn max(a as i32, b as i32) as i32 {
    if (a > b) {
        return a;
    }
    return b;
}
```

### Void Return

Void functions use `return;` with no expression, or simply reach the end of the function body.

```cst
fn print_num(n as i32) as void {
    // ... print logic ...
    return;
}
```

### Struct Return Conventions

Return behavior depends on struct size:

**Small structs (<=8 bytes)** are returned in `rax`:

```cst
struct Pair { a as i32; b as i32; }

fn make_pair(x as i32, y as i32) as Pair {
    let is Pair as p;
    p.a = x;
    p.b = y;
    return p;
}
```

**Large structs (>8 bytes)** use SRET (struct return). The caller allocates space and passes a hidden pointer as the first argument. This is handled automatically by the compiler.

```cst
struct Big { a as i64; b as i64; c as i64; }

fn make_big() as Big {
    let is Big as b;
    b.a = 1;
    b.b = 2;
    b.c = 3;
    return b;
}
```

### Return from Main

The return value of `main` becomes the process exit code (0-255).

```cst
fn main() as i32 {
    return 0;  // exit code 0 = success
}
```

## Recursion

Functions in Caustic can call themselves directly. Each recursive call creates a new stack frame with its own local variables.

### Basic Recursion

```cst
fn factorial(n as i64) as i64 {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

fn main() as i32 {
    let is i64 as result = factorial(10);
    // result = 3628800
    return 0;
}
```

### Fibonacci

```cst
fn fib(n as i32) as i32 {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

### Stack Depth

Caustic does not perform tail-call optimization, and there is no stack overflow protection. Each call uses stack space for saved registers and local variables. Deep recursion on large inputs will crash.

For performance-critical or deep recursion, prefer an iterative approach:

```cst
fn factorial_iter(n as i64) as i64 {
    let is i64 as result = 1;
    let is i64 as i = 2;
    while (i <= n) {
        result = result * i;
        i = i + 1;
    }
    return result;
}
```

### Mutual Recursion

Two functions can call each other as long as both are declared before use (or in the same file scope).

```cst
fn is_even(n as i32) as i32;

fn is_odd(n as i32) as i32 {
    if (n == 0) { return 0; }
    return is_even(n - 1);
}

fn is_even(n as i32) as i32 {
    if (n == 0) { return 1; }
    return is_odd(n - 1);
}
```

## Variadic Functions

Variadic functions accept a variable number of arguments using `...` as the last parameter.

### Syntax

```cst
fn name(fixed_param as Type, ...) as ReturnType {
    // body
}
```

The `...` must come after all fixed parameters.

### C Interop

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

### Standard Library Usage

Caustic's `std/io.cst` provides a variadic `printf` that works without libc, using direct syscalls:

```cst
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("hello %s, you are %d years old\n", "world", 25);
    return 0;
}
```

### Calling Convention

Variadic arguments follow the System V AMD64 ABI. Integer and pointer arguments go in rdi, rsi, rdx, rcx, r8, r9 (after fixed params), then overflow to the stack. The `al` register must hold the number of SSE registers used (relevant for float args).

### Limitations

- You cannot define a variadic function with custom argument processing in pure Caustic without using inline assembly or VaList internals.
- Variadic functions are best used for `extern fn` declarations calling into C or for the standard library's built-in printf.
