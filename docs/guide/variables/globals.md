# Global Variables

Global variables are declared at file scope (outside any function) and require `with mut` or `with imut`.

## Mutable globals

```cst
let is i32 as count with mut = 0;
let is *u8 as heap_base with mut = cast(*u8, 0);
```

Mutable globals live in the `.data` section. They are zero-initialized by default if the initializer is `0`.

## Immutable globals

```cst
let is i32 as MAX_TOKENS with imut = 8192;
let is i64 as PAGE_SIZE with imut = 4096;
```

Immutable globals are compile-time constants. The compiler inlines the value at every use site.

## Initializer restrictions

Global initializers must be simple constant expressions. The following are not allowed:

- Function calls: `let is i32 as x with mut = compute();` -- **error**
- Variable references: `let is i32 as y with mut = x + 1;` -- **error**
- Negative literals via subtraction: `let is i32 as neg with imut = 0 - 1;` -- **error**

Use positive sentinel values instead of negative constants.

## Accessing globals

Functions in the same file access globals directly by name:

```cst
let is i32 as total with mut = 0;

fn add(n as i32) as void {
    total = total + n;
}
```

## Cross-module globals

Globals from imported modules are accessed via the module alias:

```cst
use "config.cst" as cfg;

fn init() as void {
    let is i32 as limit = cfg.MAX_SIZE;
}
```

## Counter pattern

A common pattern for tracking state across function calls:

```cst
let is i32 as alloc_count with mut = 0;

fn allocate(size as i64) as *u8 {
    alloc_count = alloc_count + 1;
    return mem.galloc(size);
}

fn get_alloc_count() as i32 {
    return alloc_count;
}
```
