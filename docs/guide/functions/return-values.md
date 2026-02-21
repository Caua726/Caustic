# Return Values

## Returning a Value

Use `return expr;` to return a value from a function. The expression type must match the declared return type.

```cst
fn max(a as i32, b as i32) as i32 {
    if (a > b) {
        return a;
    }
    return b;
}
```

## Void Return

Void functions use `return;` with no expression, or simply reach the end of the function body.

```cst
fn print_num(n as i32) as void {
    // ... print logic ...
    return;
}
```

## Struct Return Conventions

Return behavior depends on struct size:

**Small structs (<=8 bytes)** are returned in `rax`:

```cst
struct Pair { a as i32; b as i32; }

fn make_pair(x as i32, y as i32) as Pair {
    let is Pair as p;
    p.a = x;
    p.b = y;
    return p;
}
```

**Large structs (>8 bytes)** use SRET (struct return). The caller allocates space and passes a hidden pointer as the first argument. This is handled automatically by the compiler.

```cst
struct Big { a as i64; b as i64; c as i64; }

fn make_big() as Big {
    let is Big as b;
    b.a = 1;
    b.b = 2;
    b.c = 3;
    return b;
}
```

## Return from Main

The return value of `main` becomes the process exit code (0-255).

```cst
fn main() as i32 {
    return 0;  // exit code 0 = success
}
```
