# Initialization

Caustic has different initialization rules for local variables, globals, arrays, and structs.

## Local variables

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

## Global variables

Globals always require an initializer value.

```cst
let is i32 as count with mut = 0;         // mutable, starts at 0
let is i32 as LIMIT with imut = 256;       // immutable constant
```

Omitting the initializer on a global is a compile error:

```cst
let is i32 as bad with mut;               // error: missing initializer
```

## Arrays

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

## Structs

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

## Pointer initialization

Pointers should be explicitly initialized. A null pointer can be created with `cast`:

```cst
let is *u8 as ptr = cast(*u8, 0);         // null pointer
let is *i32 as data = mem.galloc(64);      // heap-allocated
```
