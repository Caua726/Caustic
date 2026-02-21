# Compound Assignment

Compound assignment operators modify a variable in place, combining an operation with assignment.

## Arithmetic Compound

| Operator | Equivalent       | Example     |
|----------|-----------------|-------------|
| `+=`     | `x = x + y`     | `x += 1;`  |
| `-=`     | `x = x - y`     | `x -= 5;`  |
| `*=`     | `x = x * y`     | `x *= 2;`  |
| `/=`     | `x = x / y`     | `x /= 3;`  |
| `%=`     | `x = x % y`     | `x %= 10;` |

```cst
fn main() as i32 {
    let is i32 as count = 0;
    count += 1;   // count is now 1
    count += 10;  // count is now 11
    count *= 2;   // count is now 22
    count -= 2;   // count is now 20
    return count;
}
```

## Shift Compound

| Operator | Equivalent       | Example      |
|----------|-----------------|--------------|
| `<<=`    | `x = x << y`    | `x <<= 4;`  |
| `>>=`    | `x = x >> y`    | `x >>= 8;`  |

```cst
let is i32 as flags = 1;
flags <<= 3;  // flags is now 8
```

## Bitwise Compound

| Operator | Equivalent       | Example         |
|----------|-----------------|-----------------|
| `&=`     | `x = x & y`     | `x &= 0xFF;`   |
| `\|=`    | `x = x \| y`    | `x \|= mask;`  |
| `^=`     | `x = x ^ y`     | `x ^= bits;`   |

```cst
let is i32 as val = 0xFF00;
val &= 0x00FF;  // val is now 0
val |= 0x0042;  // val is now 0x42
```

## Loop Counter Pattern

Compound assignment is commonly used for loop counters:

```cst
let is i32 as i = 0;
while (i < 100) {
    // work
    i += 1;
}
```
