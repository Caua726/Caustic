# Modules

## Use Statement

Import a module with `use` to access its functions, constants, and types.

### Syntax

```cst
use "path/to/file.cst" as alias;
```

- Must appear at file scope (top of file, before functions and structs).
- The path is a string literal pointing to a `.cst` source file.
- The alias becomes the namespace for all exported symbols in that module.

### Example

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

### Rules

- Each `use` statement creates one alias. You cannot import individual symbols.
- The alias name must be a valid identifier and cannot conflict with keywords (`fn`, `let`, `gen`, etc.).
- Multiple files can import the same module; it is parsed and analyzed only once (module caching).
- A file can have any number of `use` statements.

## Aliases

After importing a module with `use`, access its members through the alias using dot notation.

### Functions

```cst
use "std/mem.cst" as mem;

let is *u8 as ptr = mem.galloc(256);
mem.gfree(ptr);
```

### Constants

```cst
use "config.cst" as cfg;

let is i32 as size = cfg.BUFFER_SIZE;
```

### Types

Use the alias to reference structs defined in the module.

```cst
use "std/string.cst" as string;

let is string.String as s = string.from_cstr("hello");
let is i64 as len = string.len(&s);
```

### Method Calls

If the module defines impl methods, call them through the alias for associated functions or directly on instances for methods.

```cst
use "std/string.cst" as string;

// Associated function (no self parameter)
let is string.String as s = string.String_new();

// Regular module function
string.free(&s);
```

### Limitations

- You cannot import individual symbols; the alias is always required.
- Struct fields from another module cannot be used as field types directly in your own structs. Use `*u8` pointers and accessor functions as a workaround.

## Relative Paths

Module paths in `use` statements are resolved relative to the file that contains them.

### Resolution Rules

The string in `use "path.cst" as alias;` is treated as a filesystem path relative to the directory of the current source file.

```cst
// File: project/main.cst
use "lib/math.cst" as math;       // resolves to project/lib/math.cst
use "std/mem.cst" as mem;         // resolves to project/std/mem.cst
```

```cst
// File: project/lib/math.cst
use "../std/mem.cst" as mem;      // resolves to project/std/mem.cst
use "helpers.cst" as helpers;     // resolves to project/lib/helpers.cst
```

### Parent Directory Traversal

Use `..` to go up one or more levels.

```cst
use "../util.cst" as util;        // one level up
use "../../std/io.cst" as io;     // two levels up
```

### Typical Project Layout

```
project/
  main.cst              use "std/mem.cst" as mem;
  std/
    mem.cst
    io.cst              use "mem.cst" as mem;
    string.cst          use "mem.cst" as mem;
  lib/
    parser.cst          use "../std/io.cst" as io;
```

Files inside `std/` import siblings with just the filename. Files in other directories use `../` to reach `std/`.

### Notes

- There is no global module search path. All paths are strictly relative.
- The file extension `.cst` is always required in the path string.

## Circular Imports

Caustic uses module caching to handle cases where two modules import each other.

### How Caching Works

Each source file is parsed and semantically analyzed only once. The first time a file is imported, it is fully processed and stored in a cache. Any subsequent `use` statement referencing the same file returns the cached module immediately.

```cst
// a.cst
use "b.cst" as b;

// b.cst
use "a.cst" as a;    // uses cached version of a.cst, no re-parse
```

### Limitations

When a circular dependency exists, the second import receives the cached module in whatever state it was when caching occurred. Some symbols may not be fully resolved yet, which can lead to missing or incomplete type information.

### Best Practices

- **Avoid circular dependencies.** Structure modules in a hierarchy where lower-level modules do not import higher-level ones.
- **Extract shared code.** If two modules both need the same functionality, move it into a third module that both import.

```
// Instead of A <-> B, use:
//   common.cst        (shared definitions)
//   a.cst             use "common.cst" as common;
//   b.cst             use "common.cst" as common;
```

- **Keep modules focused.** A module that only defines data types and pure functions is easy to import without creating cycles.

## Multi-File Projects

For larger projects, compile files separately and link the resulting object files together.

### Workflow

**1. Compile each file**

Use `-c` for library files (no `main` function required). The main file is compiled without `-c`.

```sh
./caustic -c lib.cst        # produces lib.cst.s
./caustic main.cst           # produces main.cst.s (requires main)
```

**2. Assemble each file**

```sh
./caustic-as lib.cst.s       # produces lib.cst.s.o
./caustic-as main.cst.s      # produces main.cst.s.o
```

**3. Link together**

```sh
./caustic-ld lib.cst.s.o main.cst.s.o -o program
./program
```

### The `-c` Flag

- Skips the requirement for a `main` function.
- Marks all functions as reachable (nothing is dead-code eliminated).
- All functions are emitted with `.globl` visibility so other object files can reference them.

### Cross-Object References

Use `extern fn` to declare functions defined in another object file.

```cst
// lib.cst -- compiled with -c
fn helper(x as i32) as i32 {
    return x * 2;
}
```

```cst
// main.cst
extern fn helper(x as i32) as i32;

fn main() as i32 {
    return helper(21);
}
```

### Complete Example

```sh
# Write the files
# lib.cst:  fn add(a as i64, b as i64) as i64 { return a + b; }
# main.cst: extern fn add(a as i64, b as i64) as i64;
#           fn main() as i32 { return cast(i32, add(30, 12)); }

# Build
./caustic -c lib.cst && ./caustic main.cst
./caustic-as lib.cst.s && ./caustic-as main.cst.s
./caustic-ld lib.cst.s.o main.cst.s.o -o program
./program; echo "Exit: $?"
# Exit: 42
```

### When to Use Multi-File vs `use`

| Approach | When to use |
|----------|-------------|
| `use "file.cst" as alias;` | Single compilation unit. The imported file is compiled together with the main file. |
| `-c` + `extern fn` + linking | Separate compilation units. Each file is compiled independently. Useful for large projects or mixing with other languages. |

## Extern Functions

Declare a function that is defined elsewhere -- in another Caustic object file or in a shared library.

### Syntax

```cst
extern fn name(param as Type, ...) as ReturnType;
```

No body is provided. The linker resolves the symbol at link time.

### Cross-Object References

When splitting a project across multiple files compiled with `-c`, use `extern fn` to reference functions from other translation units.

```cst
// math.cst -- compiled with ./caustic -c math.cst
fn square(x as i64) as i64 {
    return x * x;
}
```

```cst
// main.cst
extern fn square(x as i64) as i64;

fn main() as i32 {
    let is i64 as result = square(7);
    // result == 49
    return cast(i32, result);
}
```

```sh
./caustic -c math.cst && ./caustic main.cst
./caustic-as math.cst.s && ./caustic-as main.cst.s
./caustic-ld math.cst.s.o main.cst.s.o -o program
```

### C FFI with Dynamic Linking

Use `extern fn` with the `-lc` linker flag to call C library functions.

```cst
extern fn puts(s as *u8) as i32;
extern fn exit(code as i32) as void;

fn main() as i32 {
    puts("hello from caustic");
    exit(0);
    return 0;
}
```

```sh
./caustic main.cst
./caustic-as main.cst.s
./caustic-ld main.cst.s.o -lc -o program
./program
```

### Rules

- The signature must exactly match the definition (parameter types, return type, number of arguments).
- Follows the System V AMD64 ABI: integer/pointer args in rdi, rsi, rdx, rcx, r8, r9; floats in xmm0-xmm7.
- `extern fn` does not support variadic arguments in Caustic's type system, but the call will work at the ABI level if you pass the correct number of arguments.
- You cannot use `extern fn` with `syscall` -- use the `syscall()` builtin directly instead.

## Selective Imports (only)

```cst
use "std/mem.cst" only freelist, bins as mem;
```

The `only` keyword filters which names are imported from a module.

## Submodule Access

```cst
use "std/mem.cst" as mem;
mem.bins.bins_new(4096);       // access submodule function
mem.arena.create(65536);       // another submodule
mem.freelist.alloc(heap, 32);  // explicit submodule
```

Modules that import other modules expose them as submodules accessible via dot notation.
