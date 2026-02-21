# Module Aliases

After importing a module with `use`, access its members through the alias using dot notation.

## Functions

```cst
use "std/mem.cst" as mem;

let is *u8 as ptr = mem.galloc(256);
mem.gfree(ptr);
```

## Constants

```cst
use "config.cst" as cfg;

let is i32 as size = cfg.BUFFER_SIZE;
```

## Types

Use the alias to reference structs defined in the module.

```cst
use "std/string.cst" as string;

let is string.String as s = string.from_cstr("hello");
let is i64 as len = string.len(&s);
```

## Method calls

If the module defines impl methods, call them through the alias for associated functions or directly on instances for methods.

```cst
use "std/string.cst" as string;

// Associated function (no self parameter)
let is string.String as s = string.String_new();

// Regular module function
string.free(&s);
```

## Limitations

- You cannot import individual symbols; the alias is always required.
- Struct fields from another module cannot be used as field types directly in your own structs. Use `*u8` pointers and accessor functions as a workaround.
