# Variables

## Declaration

Variables in Caustic are declared with the `let` keyword, followed by `is TYPE as name`.

### Syntax

```cst
let is TYPE as name = value;
let is TYPE as name;            // uninitialized
```

The `is` keyword is required. The type always comes before `as name`.

### Examples

#### Integer types

```cst
let is i32 as x = 10;
let is i64 as big = 100000;
let is u8 as byte = 255;
```

#### Pointers

```cst
let is *u8 as ptr = cast(*u8, 0);
let is *i32 as data;
```

#### Booleans

```cst
let is bool as flag = true;
let is bool as done = false;
```

#### Arrays

```cst
let is [64]u8 as buffer;
let is [10]i32 as nums;
```

#### Structs

```cst
let is Point as p;
let is Vec3 as origin;
```

### Multiple declarations

Each variable requires its own `let` statement. There is no comma-separated multi-declaration syntax.

```cst
let is i32 as x = 0;
let is i32 as y = 0;
let is i32 as z = 0;
```

### Uninitialized variables

Omitting `= value` leaves the variable uninitialized. The contents are whatever happened to be on the stack at that location.

```cst
let is i64 as result;       // undefined contents until assigned
result = compute();
```

## Mutability

Caustic distinguishes mutability for global variables. Local variables are always mutable.

### Local variables

Local variables can be reassigned freely. There is no `const` or `imut` qualifier for locals.

```cst
fn example() as void {
    let is i32 as x = 10;
    x = 20;                    // allowed
    x = x + 1;                // allowed
}
```

### Mutable globals (`with mut`)

Mutable globals are stored in the `.data` section and can be modified at runtime.

```cst
let is i32 as counter with mut = 0;
let is bool as initialized with mut = false;

fn increment() as void {
    counter = counter + 1;
}
```

### Immutable globals (`with imut`)

Immutable globals are compile-time constants. They are inlined at every use site and cannot be reassigned.

```cst
let is i32 as MAX_SIZE with imut = 1024;
let is i32 as BUFFER_LEN with imut = 4096;
let is u8 as NEWLINE with imut = 10;
```

Attempting to assign to an immutable global is a compile error.

### Summary

| Kind | Syntax | Reassignable | Storage |
|------|--------|-------------|---------|
| Local variable | `let is T as x = v;` | Yes | Stack |
| Mutable global | `let is T as x with mut = v;` | Yes | `.data` section |
| Immutable global | `let is T as x with imut = v;` | No | Inlined constant |

## Globals

Global variables are declared at file scope (outside any function) and require `with mut` or `with imut`.

### Mutable globals

```cst
let is i32 as count with mut = 0;
let is *u8 as heap_base with mut = cast(*u8, 0);
```

Mutable globals live in the `.data` section. They are zero-initialized by default if the initializer is `0`.

### Immutable globals

```cst
let is i32 as MAX_TOKENS with imut = 8192;
let is i64 as PAGE_SIZE with imut = 4096;
```

Immutable globals are compile-time constants. The compiler inlines the value at every use site.

### Initializer restrictions

Global initializers must be simple constant expressions. The following are not allowed:

- Function calls: `let is i32 as x with mut = compute();` -- **error**
- Variable references: `let is i32 as y with mut = x + 1;` -- **error**
- Negative literals via subtraction: `let is i32 as neg with imut = 0 - 1;` -- **error**

Use positive sentinel values instead of negative constants.

### Accessing globals

Functions in the same file access globals directly by name:

```cst
let is i32 as total with mut = 0;

fn add(n as i32) as void {
    total = total + n;
}
```

### Cross-module globals

Globals from imported modules are accessed via the module alias:

```cst
use "config.cst" as cfg;

fn init() as void {
    let is i32 as limit = cfg.MAX_SIZE;
}
```

### Counter pattern

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

## Shadowing

Inner scopes can declare a variable with the same name as an outer variable. The inner variable shadows (hides) the outer one for the duration of its block.

### Basic shadowing

```cst
fn example() as i32 {
    let is i32 as x = 10;

    if true {
        let is i32 as x = 20;     // shadows outer x
        // x is 20 here
    }

    // x is 10 again
    return x;                      // returns 10
}
```

### Nested blocks

Each block creates a new scope. Shadowing can be nested multiple levels deep.

```cst
fn nested() as i32 {
    let is i32 as val = 1;

    if true {
        let is i32 as val = 2;

        if true {
            let is i32 as val = 3;
            // val is 3
        }

        // val is 2
    }

    // val is 1
    return val;
}
```

### Different types

A shadowing variable does not need to share the type of the outer variable.

```cst
fn convert() as void {
    let is i32 as result = 42;
    // result is i32 here

    if true {
        let is *u8 as result = cast(*u8, 0);
        // result is *u8 here
    }

    // result is i32 again
}
```

### Loop variables

Loop counters in `for` or `while` blocks follow the same scoping rules. A variable declared inside a loop body is created fresh each iteration and does not persist outside the loop.

```cst
fn sum_to(n as i32) as i32 {
    let is i32 as total = 0;
    let is i32 as i = 0;
    while i < n {
        let is i32 as val = i * 2;    // scoped to loop body
        total = total + val;
        i = i + 1;
    }
    // val is not accessible here
    return total;
}
```

## Initialization

Caustic has different initialization rules for local variables, globals, arrays, and structs.

### Local variables

Locals can be declared with or without an initializer.

```cst
let is i32 as x = 42;         // initialized to 42
let is i32 as y;               // uninitialized
```

An uninitialized local contains whatever value was previously on the stack at that memory location. Always assign before reading to avoid undefined behavior.

```cst
let is i64 as result;
result = do_work();            // assign before use
syscall(60, result, 0, 0);
```

### Global variables

Globals always require an initializer value.

```cst
let is i32 as count with mut = 0;         // mutable, starts at 0
let is i32 as LIMIT with imut = 256;       // immutable constant
```

Omitting the initializer on a global is a compile error:

```cst
let is i32 as bad with mut;               // error: missing initializer
```

### Arrays

Arrays are declared with `[N]T` syntax. They are stack-allocated and uninitialized.

```cst
let is [256]u8 as buffer;                 // 256 bytes, uninitialized
let is [16]i64 as table;                  // 16 i64 values, uninitialized
```

Elements are accessed by index and must be assigned individually:

```cst
let is [4]i32 as arr;
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
arr[3] = 40;
```

### Structs

Struct variables are allocated on the stack with all fields uninitialized.

```cst
struct Point {
    x as i32;
    y as i32;
}

fn make_origin() as Point {
    let is Point as p;
    p.x = 0;
    p.y = 0;
    return p;
}
```

If the struct type has an associated constructor function (via `impl`), use it to ensure all fields are properly set:

```cst
impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }
}

fn example() as void {
    let is Point as p = Point.new(10, 20);
}
```

### Pointer initialization

Pointers should be explicitly initialized. A null pointer can be created with `cast`:

```cst
let is *u8 as ptr = cast(*u8, 0);         // null pointer
let is *i32 as data = mem.galloc(64);      // heap-allocated
```
