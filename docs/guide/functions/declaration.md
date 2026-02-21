# Function Declaration

Functions in Caustic are declared with the `fn` keyword, followed by the name, parameters in parentheses, return type after `as`, and a body in braces.

## Syntax

```cst
fn name(param1 as Type1, param2 as Type2) as ReturnType {
    // body
}
```

## Void Functions

Functions that return nothing use `void` as the return type.

```cst
fn greet() as void {
    syscall(1, 1, "hello\n", 6);
}
```

## Return Type

The return type is specified after `as`. Every non-void function must end with a `return` statement.

```cst
fn add(a as i32, b as i32) as i32 {
    return a + b;
}

fn square(x as i64) as i64 {
    return x * x;
}
```

## Declaration Order

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

## No Parameters

Parentheses are still required even with no parameters.

```cst
fn get_zero() as i32 {
    return 0;
}
```
