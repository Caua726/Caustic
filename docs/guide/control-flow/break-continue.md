# Break and Continue

Control statements for exiting or skipping loop iterations.

## Break

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

## Continue

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

## Works in All Loop Types

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

## Nested Loops

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
