# Generic Enums

Generic enums work like generic structs: each unique type argument combination generates a concrete enum with mangled name.

## Declaration

```cst
enum Option gen T {
    Some as T;
    None;
}

enum Result gen T, E {
    Ok as T;
    Err as E;
}
```

## Creating Values

```cst
let is Option gen i32 as some_val = Option gen i32 .Some(42);
let is Option gen i32 as none_val = Option gen i32 .None();

let is Result gen i32, i32 as ok = Result gen i32, i32 .Ok(100);
let is Result gen i32, i32 as err = Result gen i32, i32 .Err(1);
```

## Matching

```cst
match Option gen i32 (some_val) {
    case Some(val) {
        // val is i32
        syscall(60, val);
    }
    case None {
        syscall(60, 1);
    }
}
```

## Standard Library Types

`std/types.cst` provides `Option` and `Result`:

```cst
use "std/types.cst" as types;

fn find(arr as *i32, len as i32, target as i32) as Option gen i32 {
    let is i32 as i = 0;
    while i < len {
        if arr[i] == target {
            return Option gen i32 .Some(i);
        }
        i = i + 1;
    }
    return Option gen i32 .None();
}
```

## Name Mangling

| Usage | Generated Name |
|-------|---------------|
| `Option gen i32` | `Option_i32` |
| `Result gen i32, i64` | `Result_i32_i64` |
| `Option gen *u8` | `Option_pu8` (pointer prefix) |
