# Control Flow

## If/Else

Conditional branching. The condition must be in parentheses and braces are required around the body.

### Basic If

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
}
```

### If/Else

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
} else {
    syscall(1, 1, "non-positive\n", 13);
}
```

### Else If Chains

```cst
if (x > 0) {
    syscall(1, 1, "positive\n", 9);
} else if (x == 0) {
    syscall(1, 1, "zero\n", 5);
} else {
    syscall(1, 1, "negative\n", 9);
}
```

### Condition Semantics

The condition evaluates to an integer. Zero is false, any non-zero value is true.

```cst
let is i64 as flags = 3;
if (flags) {
    // executes because flags != 0
}
```

### Nesting

If/else blocks can be nested arbitrarily.

```cst
if (x > 0) {
    if (x < 100) {
        syscall(1, 1, "small positive\n", 15);
    } else {
        syscall(1, 1, "large positive\n", 15);
    }
}
```

### Rules

- Parentheses around the condition are required.
- Braces around the body are required (no single-statement shorthand).
- `else if` is not a special keyword -- it is an `else` followed by another `if`.

## While

Repeats a block while the condition is true (non-zero).

### Syntax

```cst
while (condition) {
    // body
}
```

### Counting Loop

```cst
let is i32 as i = 0;
while (i < 10) {
    // do work
    i = i + 1;
}
```

### Infinite Loop

```cst
while (1) {
    // runs forever until break or return
}
```

### Using Break and Continue

```cst
let is i32 as i = 0;
while (i < 100) {
    i = i + 1;
    if (i == 50) {
        break;
    }
    if (i == 25) {
        continue;
    }
    // skipped when i == 25, stops when i == 50
}
```

### Rules

- Parentheses around the condition are required.
- Braces around the body are required.
- The condition is checked before each iteration. If false on the first check, the body never executes.

## For

A loop with init, condition, and step expressions.

### Syntax

```cst
for (init; condition; step) {
    // body
}
```

- **init** -- executed once before the loop starts.
- **condition** -- checked before each iteration. Loop exits when false (zero).
- **step** -- executed after each iteration body.

### Basic Example

```cst
for (let is i32 as i = 0; i < 10; i += 1) {
    // runs 10 times with i = 0..9
}
```

### Iterating Over an Array

```cst
let is [5]i32 as arr;
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
arr[3] = 40;
arr[4] = 50;

let is i64 as sum = 0;
for (let is i32 as i = 0; i < 5; i += 1) {
    sum = sum + arr[i];
}
```

### Nested Loops

```cst
for (let is i32 as i = 0; i < 3; i += 1) {
    for (let is i32 as j = 0; j < 3; j += 1) {
        // 9 iterations total
    }
}
```

### Rules

- Parentheses around the three clauses are required.
- Braces around the body are required.
- The init clause can declare a variable with `let`.
- `break` and `continue` work inside for loops.

## Do-While

Executes the body at least once, then repeats while the condition is true.

### Syntax

```cst
do {
    // body
} while (condition);
```

### Example

```cst
let is i32 as x = 0;
do {
    x = x + 1;
} while (x < 10);
// x is now 10
```

### Guaranteed First Execution

Unlike `while`, the body always runs at least once, even if the condition is initially false.

```cst
let is i32 as x = 100;
do {
    // this runs once even though x >= 10
    x = x + 1;
} while (x < 10);
// x is now 101
```

### Rules

- Braces around the body are required.
- Parentheses around the condition are required.
- The trailing semicolon after `while (condition)` is required.
- `break` and `continue` work inside do-while loops.

## Break and Continue

Control statements for exiting or skipping loop iterations.

### Break

`break` exits the innermost enclosing loop immediately.

```cst
let is i32 as i = 0;
while (1) {
    if (i == 5) {
        break;
    }
    i = i + 1;
}
// i is now 5
```

### Continue

`continue` skips the rest of the current iteration and jumps to the next one (condition check for while/do-while, step then condition for for-loops).

```cst
let is i64 as sum = 0;
for (let is i32 as i = 0; i < 10; i += 1) {
    if (i == 3) {
        continue;
    }
    sum = sum + i;
}
// sum is 0+1+2+4+5+6+7+8+9 = 42
```

### Works in All Loop Types

Both `break` and `continue` work in `while`, `for`, and `do-while` loops.

```cst
do {
    if (should_stop()) {
        break;
    }
    if (should_skip()) {
        continue;
    }
    // work
} while (1);
```

### Nested Loops

`break` and `continue` only affect the innermost loop. There are no labeled breaks.

```cst
for (let is i32 as i = 0; i < 10; i += 1) {
    for (let is i32 as j = 0; j < 10; j += 1) {
        if (j == 5) {
            break;  // only exits the inner loop
        }
    }
    // continues with next i
}
```

## Match

Pattern matching on enum values.

### Syntax

```cst
match EnumType (value) {
    case Variant1 { ... }
    case Variant2 { ... }
    else { ... }
}
```

The enum type name comes after `match`, and the value to match is in parentheses.

### Simple Enum Matching

```cst
enum Color { Red; Green; Blue; }

fn describe(c as Color) as i32 {
    match Color (c) {
        case Red {
            syscall(1, 1, "red\n", 4);
        }
        case Green {
            syscall(1, 1, "green\n", 6);
        }
        case Blue {
            syscall(1, 1, "blue\n", 5);
        }
    }
    return 0;
}
```

### Else Branch

The `else` branch handles any variant not explicitly listed.

```cst
match Color (c) {
    case Red {
        return 1;
    }
    else {
        return 0;
    }
}
```

### Matching Enums with Data

Enum variants can carry payloads. Use destructuring to extract the data.

```cst
enum Shape {
    Circle as i32;
    Rect as i32, i32;
    None;
}

let is Shape as s = Shape.Circle(5);
match Shape (s) {
    case Circle(r) {
        // r is bound to the payload value
        return r;
    }
    case Rect(w, h) {
        return w + h;
    }
    else {
        return 0;
    }
}
```

### Rules

- The enum type name is required after `match`.
- Braces are required around each case body.
- Case variant names are written without the enum prefix (e.g. `case Red`, not `case Color.Red`).
- The `else` branch is optional but recommended to handle unexpected variants.

## Destructuring

Extract data from enum variant payloads inside `match` expressions.

### Syntax

```cst
match EnumType (value) {
    case VariantName(var1, var2) {
        // var1 and var2 are bound to the variant's payload fields
    }
}
```

Variables in parentheses after the variant name are bound to the payload values in order.

### Example

```cst
enum Shape {
    Circle as i32;
    Rect as i32, i32;
    None;
}

fn area(s as Shape) as i32 {
    match Shape (s) {
        case Circle(r) {
            return r * r * 3;
        }
        case Rect(w, h) {
            return w * h;
        }
        else {
            return 0;
        }
    }
    return 0;
}

fn main() as i32 {
    let is Shape as s = Shape.Rect(10, 20);
    return area(s);  // returns 200
}
```

### Different Payload Sizes

Each variant can have a different number of fields. Variants without data need no parentheses.

```cst
enum Result {
    Ok as i64;
    Pair as i64, i64;
    Err;
}

match Result (val) {
    case Ok(n) {
        // single field
        return n;
    }
    case Pair(a, b) {
        // two fields
        return a + b;
    }
    case Err {
        // no payload, no parentheses
        return 0;
    }
}
```

### Rules

- The number of variables must match the number of fields in the variant.
- Destructured variables are scoped to the case block -- they do not exist outside it.
- Variants without data are matched without parentheses.
- Variable names are arbitrary and independent of the field type names in the enum definition.
