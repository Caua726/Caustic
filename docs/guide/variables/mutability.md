# Mutability

Caustic distinguishes mutability for global variables. Local variables are always mutable.

## Local variables

Local variables can be reassigned freely. There is no `const` or `imut` qualifier for locals.

```cst
fn example() as void {
    let is i32 as x = 10;
    x = 20;                    // allowed
    x = x + 1;                // allowed
}
```

## Mutable globals (`with mut`)

Mutable globals are stored in the `.data` section and can be modified at runtime.

```cst
let is i32 as counter with mut = 0;
let is bool as initialized with mut = false;

fn increment() as void {
    counter = counter + 1;
}
```

## Immutable globals (`with imut`)

Immutable globals are compile-time constants. They are inlined at every use site and cannot be reassigned.

```cst
let is i32 as MAX_SIZE with imut = 1024;
let is i32 as BUFFER_LEN with imut = 4096;
let is u8 as NEWLINE with imut = 10;
```

Attempting to assign to an immutable global is a compile error.

## Summary

| Kind | Syntax | Reassignable | Storage |
|------|--------|-------------|---------|
| Local variable | `let is T as x = v;` | Yes | Stack |
| Mutable global | `let is T as x with mut = v;` | Yes | `.data` section |
| Immutable global | `let is T as x with imut = v;` | No | Inlined constant |
