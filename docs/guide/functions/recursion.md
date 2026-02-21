# Recursion

Functions in Caustic can call themselves directly. Each recursive call creates a new stack frame with its own local variables.

## Basic Recursion

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

## Fibonacci

```cst
fn fib(n as i32) as i32 {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

## Stack Depth

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

## Mutual Recursion

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
