# Defer

`defer` schedules a function call to execute automatically before every `return` in the current scope.

## Syntax

```cst
defer function_call(args);
```

## Basic Example

```cst
use "std/mem.cst" as mem;

fn work() as i32 {
    let is *u8 as ptr = mem.galloc(1024);
    defer mem.gfree(ptr);

    // ... use ptr ...

    return 0;  // mem.gfree(ptr) runs automatically here
}
```

## Rules

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

## Workaround for syscall

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
