# Enum Construction

## Simple Enum (No Data)

Use `EnumName.Variant` with no parentheses:

```cst
enum Color { Red; Green; Blue; }

let is Color as c = Color.Red;
```

## With Data

Pass arguments in parentheses:

```cst
enum Result {
    Ok as i32;
    Err as *u8;
}

let is Result as r1 = Result.Ok(42);
let is Result as r2 = Result.Err("something failed");
```

Multiple fields are comma-separated:

```cst
enum Msg {
    Move as i32, i32;
    Resize as i32, i32, i32, i32;
    Quit;
}

let is Msg as m = Msg.Move(10, 20);
let is Msg as r = Msg.Resize(0, 0, 800, 600);
let is Msg as q = Msg.Quit;
```

## Generic Enums

Generic enums require type parameters at both declaration and construction:

```cst
enum Option gen T {
    Some as T;
    None;
}

let is Option gen i32 as x = Option gen i32 .Some(42);
let is Option gen i32 as y = Option gen i32 .None;
```

Note the space before `.Some` -- this is required syntax when using generics.

```cst
enum Either gen L, R {
    Left as L;
    Right as R;
}

let is Either gen i32, *u8 as e = Either gen i32, *u8 .Left(10);
```

## Assigning to Existing Variables

```cst
let is Color as c = Color.Red;
c = Color.Blue;  // reassign
```

## In Function Arguments

Construct directly in a function call:

```cst
fn process(r as Result) as i32 {
    match (r) {
        Result.Ok(val) => { return val; }
        Result.Err(msg) => { return 1; }
    }
    return 0;
}

let is i32 as code = process(Result.Ok(0));
```

## Return from Functions

```cst
fn divide(a as i32, b as i32) as Result {
    if (b == 0) {
        return Result.Err("division by zero");
    }
    return Result.Ok(a / b);
}
```
