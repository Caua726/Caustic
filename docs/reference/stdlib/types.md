# std/types.cst -- Generic Types

Generic algebraic types for representing optional values and fallible operations. These are monomorphized at compile time -- each concrete instantiation (e.g., `Option gen i64`) generates a specialized enum type.

## Dependencies

None. This module has no imports.

---

## Types

### Option gen T

```cst
enum Option gen T {
    Some as T;
    None;
}
```

Represents an optional value. `Some` carries a value of type `T`. `None` represents the absence of a value.

### Result gen T, E

```cst
enum Result gen T, E {
    Ok as T;
    Err as E;
}
```

Represents the outcome of a fallible operation. `Ok` carries the success value of type `T`. `Err` carries the error value of type `E`.

---

## Usage

### Importing

```cst
use "std/types.cst" as types;
```

### Creating Option Values

```cst
let is types.Option gen i64 as some_val = types.Option.Some gen i64 (42);
let is types.Option gen i64 as none_val = types.Option.None gen i64;
```

### Creating Result Values

```cst
let is types.Result gen i64, i32 as ok_val = types.Result.Ok gen i64, i32 (100);
let is types.Result gen i64, i32 as err_val = types.Result.Err gen i64, i32 (cast(i32, 0 - 1));
```

### Pattern Matching with match

Use `match` to destructure and extract the inner values:

```cst
fn process_option(opt as types.Option gen i64) as i64 {
    match (opt) {
        types.Option.Some gen i64 (val) => {
            return val;
        }
        types.Option.None gen i64 => {
            return 0 - 1;
        }
    }
    return 0;
}
```

```cst
fn process_result(res as types.Result gen i64, i32) as i64 {
    match (res) {
        types.Result.Ok gen i64, i32 (val) => {
            return val;
        }
        types.Result.Err gen i64, i32 (code) => {
            io.printf("error: %d\n", cast(i64, code));
            return 0;
        }
    }
    return 0;
}
```

### Returning Option from Functions

```cst
fn find_item(items as *u8, count as i64, target as i64) as types.Option gen i64 {
    let is i64 as i with mut = 0;
    while (i < count) {
        let is *i64 as arr = cast(*i64, items);
        if (arr[i] == target) {
            return types.Option.Some gen i64 (i);
        }
        i = i + 1;
    }
    return types.Option.None gen i64;
}
```

### Returning Result from Functions

```cst
fn safe_divide(a as i64, b as i64) as types.Result gen i64, i32 {
    if (b == 0) {
        return types.Result.Err gen i64, i32 (cast(i32, 1)); // division by zero
    }
    return types.Result.Ok gen i64, i32 (a / b);
}
```

---

## Full Example

```cst
use "std/types.cst" as types;
use "std/io.cst" as io;
use "std/mem.cst" as mem;

fn divide(a as i64, b as i64) as types.Result gen i64, i32 {
    if (b == 0) {
        return types.Result.Err gen i64, i32 (cast(i32, 1));
    }
    return types.Result.Ok gen i64, i32 (a / b);
}

fn main() as i32 {
    mem.gheapinit(1024 * 1024);

    let is types.Result gen i64, i32 as res = divide(10, 3);

    match (res) {
        types.Result.Ok gen i64, i32 (val) => {
            io.printf("result: %d\n", val);
        }
        types.Result.Err gen i64, i32 (code) => {
            io.printf("error code: %d\n", cast(i64, code));
        }
    }

    return 0;
}
```

## Notes

- Generic types are monomorphized: `Option gen i64` and `Option gen i32` are completely separate types at runtime, with mangled names like `Option_i64` and `Option_i32`.
- The `gen` keyword must be followed by concrete types when constructing or matching. There is no type inference for generic parameters.
- Enum variants with payloads (`Some as T`, `Ok as T`) store the value inline in the enum. Variants without payloads (`None`) carry no data.
