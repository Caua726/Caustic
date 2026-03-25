# Function Pointers

Caustic supports both creating function pointers (`fn_ptr`) and calling through them (`call`).

## Creating Function Pointers

`fn_ptr(name)` returns the address of a function. The result carries type information (TY_FN) with parameter types and return type, enabling compile-time type checking when used with `call()`.

```cst
let is *u8 as fp = fn_ptr(function_name);
```

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
    if (a > b) { return a; }
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

## Calling Through Function Pointers

Use `call(fn_ptr_expr, args...)` to call through a function pointer:

```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn main() as i32 {
    let is *u8 as fp = fn_ptr(add);
    let is i64 as result = call(fp, 3, 4);  // returns 7
    return cast(i32, result);
}
```

### Type-Checked Calls

Since `fn_ptr()` carries the function's type signature, the compiler checks argument count and types at compile time:

```cst
fn compare(a as i64, b as i64) as i32 {
    if (a < b) { return cast(i32, 0 - 1); }
    if (a > b) { return 1; }
    return 0;
}

fn main() as i32 {
    let is *u8 as cmp = fn_ptr(compare);

    // Compiler knows cmp takes (i64, i64) and returns i32
    let is i32 as r = call(cmp, 10, 20);  // OK: types match
    return r;
}
```

### Practical Example: Custom Sorting

```cst
use "std/sort.cst" as sort;

fn reverse_cmp(a as i64, b as i64) as i32 {
    if (a > b) { return cast(i32, 0 - 1); }
    if (a < b) { return 1; }
    return 0;
}

fn main() as i32 {
    let is [5]i64 as arr;
    arr[0] = 5; arr[1] = 3; arr[2] = 1; arr[3] = 4; arr[4] = 2;

    // sort with custom comparator
    sort.sort_i64(&arr, 5, fn_ptr(reverse_cmp));
    // arr is now [5, 4, 3, 2, 1]

    return 0;
}
```

## Passing Callbacks to C (FFI)

Function pointers work with `extern fn` for C interop:

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
