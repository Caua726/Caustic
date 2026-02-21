# Shadowing

Inner scopes can declare a variable with the same name as an outer variable. The inner variable shadows (hides) the outer one for the duration of its block.

## Basic shadowing

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

## Nested blocks

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

## Different types

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

## Loop variables

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
