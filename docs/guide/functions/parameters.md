# Parameters

Function parameters are declared with `name as Type` syntax, separated by commas.

## Pass by Value

Primitive types and small structs are copied when passed to functions.

```cst
fn double(x as i32) as i32 {
    return x * 2;
}

fn main() as i32 {
    let is i32 as val = 10;
    let is i32 as result = double(val);
    // val is still 10
    return result;
}
```

## Pass by Pointer

Use pointer parameters to modify values in the caller's scope.

```cst
fn modify(p as *i32) as void {
    *p = 42;
}

fn main() as i32 {
    let is i32 as x = 0;
    modify(&x);
    // x is now 42
    return x;
}
```

## Register Allocation (System V ABI)

The first 6 integer/pointer arguments are passed in registers:

| Argument | Register |
|----------|----------|
| 1st      | rdi      |
| 2nd      | rsi      |
| 3rd      | rdx      |
| 4th      | rcx      |
| 5th      | r8       |
| 6th      | r9       |

Additional arguments are passed on the stack. Float arguments use xmm0-xmm7.

## Array Parameters

Arrays are passed by pointer to the first element.

```cst
fn sum_array(arr as *i32, len as i32) as i32 {
    let is i32 as total = 0;
    let is i32 as i = 0;
    while (i < len) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}
```

## Struct Parameters

Small structs (<=8 bytes) are passed in a register. Larger structs are passed by copying onto the stack or via a hidden pointer.

```cst
struct Point { x as i32; y as i32; }

fn distance(p as Point) as i32 {
    return p.x + p.y;
}
```
