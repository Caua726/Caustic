# Use Statement

Import a module with `use` to access its functions, constants, and types.

## Syntax

```cst
use "path/to/file.cst" as alias;
```

- Must appear at file scope (top of file, before functions and structs).
- The path is a string literal pointing to a `.cst` source file.
- The alias becomes the namespace for all exported symbols in that module.

## Example

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;

fn main() as i32 {
    let is *u8 as buf = mem.galloc(1024);
    io.printf("allocated %d bytes\n", 1024);
    mem.gfree(buf);
    return 0;
}
```

## Rules

- Each `use` statement creates one alias. You cannot import individual symbols.
- The alias name must be a valid identifier and cannot conflict with keywords (`fn`, `let`, `gen`, etc.).
- Multiple files can import the same module; it is parsed and analyzed only once (module caching).
- A file can have any number of `use` statements.
