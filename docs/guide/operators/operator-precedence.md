# Operator Precedence

Operators are listed from highest precedence (evaluated first) to lowest. Operators on the same row have equal precedence and are evaluated left to right.

| Precedence | Operators                        | Description              |
|------------|----------------------------------|--------------------------|
| 1 (highest)| `()`                            | Grouping                 |
| 2          | `[]` `.`                         | Indexing, member access   |
| 3          | `!` `~` `-` (unary) `*` `&`     | Unary NOT, complement, negate, deref, address-of |
| 4          | `*` `/` `%`                      | Multiplication, division, modulo |
| 5          | `+` `-`                          | Addition, subtraction    |
| 6          | `<<` `>>`                        | Bit shifts               |
| 7          | `<` `<=` `>` `>=`               | Relational comparison    |
| 8          | `==` `!=`                        | Equality comparison      |
| 9          | `&`                              | Bitwise AND              |
| 10         | `^`                              | Bitwise XOR              |
| 11         | `\|`                             | Bitwise OR               |
| 12         | `&&`                             | Logical AND              |
| 13         | `\|\|`                           | Logical OR               |
| 14 (lowest)| `=` `+=` `-=` `*=` `/=` `%=` `<<=` `>>=` `&=` `\|=` `^=` | Assignment |

## Examples

```cst
// Multiplication before addition
let is i32 as x = 2 + 3 * 4;     // 14, not 20

// Comparison before logical AND
if (a > 0 && b < 10) { }          // parsed as (a > 0) && (b < 10)

// Bitwise AND before bitwise OR
let is i32 as r = a & 0xF | b;    // parsed as (a & 0xF) | b

// Shift before comparison
if (1 << n > 100) { }             // parsed as (1 << n) > 100
```

## Using Parentheses

When in doubt, use parentheses to make intent explicit. This is especially important with bitwise operators, where precedence is often surprising:

```cst
// Without parens: & binds tighter than ==
if (flags & MASK == 0) { }        // parsed as flags & (MASK == 0) -- probably wrong!

// With parens: correct intent
if ((flags & MASK) == 0) { }      // check if masked bits are zero
```

## Associativity

All binary operators are left-associative:

```cst
let is i32 as x = a - b - c;     // parsed as (a - b) - c
```

Assignment operators are right-associative:

```cst
x = y = 0;                        // parsed as x = (y = 0)
```
