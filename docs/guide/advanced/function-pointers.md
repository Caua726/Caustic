# Function Pointers

`fn_ptr(name)` returns the address of a function as a `*u8` pointer.

## Syntax

```cst
let is *u8 as fp = fn_ptr(function_name);
```

## Supported Targets

### Local Functions

```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(add);
    return 0;
}
```

### Module Functions

```cst
use "utils.cst" as utils;

fn main() as i32 {
    let is *u8 as fp = fn_ptr(utils.process);
    return 0;
}
```

### Generic Instantiations

Specify the type parameter to get the monomorphized version:

```cst
fn max gen T (a as T, b as T) as T {
    if a > b { return a; }
    return b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(max gen i32);
    // fp points to max_i32
    return 0;
}
```

### Impl Methods

Use the mangled name (TypeName_method):

```cst
struct Point { x as i32; y as i32; }

impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(Point_sum);
    return 0;
}
```

## Use Case: Passing Callbacks to C

The primary use case is passing Caustic functions as callbacks to C libraries via FFI:

```cst
extern fn qsort(base as *u8, nmemb as i64, size as i64, compar as *u8) as void;

fn compare_ints(a as *i32, b as *i32) as i32 {
    return *a - *b;
}

fn main() as i32 {
    let is [5]i32 as arr;
    arr[0] = 5; arr[1] = 3; arr[2] = 1; arr[3] = 4; arr[4] = 2;
    qsort(cast(*u8, &arr), 5, 4, fn_ptr(compare_ints));
    return 0;
}
```

## Limitation

Caustic has **no syntax to call through function pointers**. You can obtain the address of a function but cannot invoke it via the pointer. Function pointers are useful for:

- Passing callbacks to C functions
- Storing in data structures for later use by external code
- Comparing function identity

If you need dynamic dispatch within Caustic, use `if`/`match` on an enum tag and call functions directly.
