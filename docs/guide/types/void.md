# Void

The `void` type represents the absence of a value. It has a size of 0 bytes.

## Function Return Type

`void` is used when a function does not return a value.

```cst
fn print_number(n as i64) as void {
    // write n to stdout
    // no return value
}
```

A `void` function can use `return;` with no expression, or simply fall through to the end of the body.

```cst
fn greet() as void {
    syscall(1, 1, "hello\n", 6);
    return;    // optional, implicit at end of function
}
```

## Restrictions

You cannot declare a variable of type `void`.

```cst
// ERROR: cannot have a void variable
// let is void as nothing;
```

You cannot use the result of a void function in an expression.

```cst
fn do_work() as void { }

// ERROR: void has no value
// let is i64 as x = do_work();
```

## Void Pointers

While `void` itself has no size, `*u8` is used as the generic pointer type in Caustic (similar to `void*` in C).

```cst
let is *u8 as generic_ptr = mem.galloc(64);
let is *i32 as typed_ptr = cast(*i32, generic_ptr);
```
