# Expression Parsing

The Caustic parser uses a **precedence-climbing** algorithm for binary expressions, combined with recursive descent for unary, postfix, and primary expressions.

## Precedence Table

Binary operators are parsed according to the following precedence levels, from lowest (parsed first, binds loosest) to highest (parsed last, binds tightest):

| Precedence | Operators | Associativity | Node Kinds |
|------------|-----------|---------------|------------|
| 1 (lowest) | `\|\|` | Left-to-right | `NK_LOR` |
| 2 | `&&` | Left-to-right | `NK_LAND` |
| 3 | `\|` | Left-to-right | `NK_BOR` |
| 4 | `^` | Left-to-right | `NK_BXOR` |
| 5 | `&` | Left-to-right | `NK_BAND` |
| 6 | `==` `!=` | Left-to-right | `NK_EQ`, `NK_NEQ` |
| 7 | `<` `>` `<=` `>=` | Left-to-right | `NK_LT`, `NK_GT`, `NK_LE`, `NK_GE` |
| 8 | `<<` `>>` | Left-to-right | `NK_SHL`, `NK_SHR` |
| 9 | `+` `-` | Left-to-right | `NK_ADD`, `NK_SUB` |
| 10 (highest) | `*` `/` `%` | Left-to-right | `NK_MUL`, `NK_DIV`, `NK_MOD` |

All binary operators are left-associative. The precedence-climbing loop works as follows:

1. Parse a unary expression as the left-hand side.
2. While the current token is a binary operator with precedence >= the minimum precedence:
   a. Record the operator and its precedence.
   b. Advance past the operator token.
   c. Recursively parse the right-hand side with minimum precedence = current + 1 (for left-associativity).
   d. Create a binary node with the operator's node kind, linking `lhs` and `rhs`.
3. Return the resulting expression tree.

## Unary Prefix Operators

Before checking for binary operators, the parser handles unary prefix operators:

| Token | Meaning | Node Kind | Description |
|-------|---------|-----------|-------------|
| `!` | Logical NOT | `NK_LNOT` | Inverts boolean value |
| `~` | Bitwise NOT | `NK_BNOT` | Flips all bits |
| `-` | Negation | `NK_NEG` | Arithmetic negation |
| `*` | Dereference | `NK_DEREF` | Follows pointer to its target value |
| `&` | Address-of | `NK_ADDR` | Takes the memory address of a value |

Unary operators are right-associative and bind tighter than all binary operators. They can be chained:

```cst
**ptr       // dereference a pointer-to-pointer
!!(x > 0)  // double logical NOT
-(-x)      // double negation
&*ptr      // address of dereferenced pointer (identity on pointers)
```

The parser dispatches on the current token:
- If `TK_NOT`: parse unary, wrap in `NK_LNOT`
- If `TK_BNOT`: parse unary, wrap in `NK_BNOT`
- If `TK_MINUS`: parse unary, wrap in `NK_NEG`
- If `TK_STAR`: parse unary, wrap in `NK_DEREF`
- If `TK_BAND`: parse unary, wrap in `NK_ADDR`
- Otherwise: parse a postfix expression

## Primary Expressions

Primary expressions are the leaves and terminals of the expression tree:

### Numeric Literal
Current token is `TK_INTEGER` or `TK_FLOAT`. Creates an `NK_NUM` node with the literal value.

```cst
42
3.14
```

### Identifier
Current token is `TK_IDENT`. Creates an `NK_IDENT` node. May be followed by postfix operators (call, index, member).

```cst
x
my_variable
```

### String Literal
Current token is `TK_STRING`. Creates an `NK_STRING` node with the string data.

```cst
"hello world"
```

### Character Literal
Current token is `TK_CHAR`. Creates an `NK_NUM` node with the character's numeric value.

```cst
'A'
'\n'
```

### Boolean Literals
The identifiers `true` and `false` are recognized as boolean constants at the parser level, producing `NK_NUM` nodes with values 1 and 0 respectively.

```cst
true
false
```

### Parenthesized Expression
Current token is `TK_LPAREN`. The parser consumes `(`, recursively parses an expression, then expects `)`.

```cst
(a + b) * c
```

## Special Expression Forms

These are parsed as primary expressions when their keyword token is encountered:

### Cast Expression
Syntax: `cast(Type, expr)`

```cst
cast(*u8, address)
cast(i32, long_value)
```

Parses as:
1. Consume `TK_CAST`, `TK_LPAREN`
2. Parse the target type
3. Consume `TK_COMMA`
4. Parse the expression to cast
5. Consume `TK_RPAREN`
6. Create `NK_CAST` node with `cast_type` and `lhs`

### Sizeof Expression
Syntax: `sizeof(Type)`

```cst
sizeof(i64)
sizeof(Point)
sizeof(*u8)
```

Parses as:
1. Consume `TK_SIZEOF`, `TK_LPAREN`
2. Parse the type
3. Consume `TK_RPAREN`
4. Create `NK_SIZEOF` node with `sizeof_type`

### Syscall Expression
Syntax: `syscall(number, arg1, arg2, ...)`

```cst
syscall(1, 1, msg, len)    // write(stdout, msg, len)
syscall(60, code)           // exit(code)
```

Parses as:
1. Consume `TK_SYSCALL`, `TK_LPAREN`
2. Parse a comma-separated list of argument expressions
3. Consume `TK_RPAREN`
4. Create `NK_SYSCALL` node with `args` list

Up to 6 arguments are supported (matching the Linux syscall convention for x86_64).

### Inline Assembly
Syntax: `asm("assembly string")`

```cst
asm("mov rax, 1\n")
asm("xor rdi, rdi\n")
```

Parses as:
1. Consume `TK_ASM`, `TK_LPAREN`
2. Expect `TK_STRING`, record the assembly text
3. Consume `TK_RPAREN`
4. Create `NK_ASM` node with `str_val`

### Function Pointer
Syntax: `fn_ptr(name)`, `fn_ptr(mod.name)`, `fn_ptr(name gen Type)`, `fn_ptr(Type_method)`

```cst
let is *u8 as cb = fn_ptr(my_func);
let is *u8 as cb = fn_ptr(io.print);
let is *u8 as cb = fn_ptr(max gen i32);
```

Parses as:
1. Consume `TK_FN_PTR`, `TK_LPAREN`
2. Parse the function reference (possibly module-qualified or with generic arguments)
3. Consume `TK_RPAREN`
4. Create `NK_FN_PTR` node

## Postfix Operators

After parsing a primary expression, the parser checks for postfix operators in a loop:

### Function Call
If followed by `TK_LPAREN`, parse argument list:

```cst
foo(1, 2, 3)
mod.func(x)
name gen i32 (args)
```

Creates `NK_FNCALL` with `name`, `args`, and optionally `module_name` and `generic_args`.

### Array/Pointer Indexing
If followed by `TK_LBRACKET`, parse index expression:

```cst
arr[0]
ptr[i + 1]
```

Creates `NK_INDEX` with `lhs` (base) and `rhs` (index).

### Member Access
If followed by `TK_DOT`, parse member name:

```cst
point.x
obj.field
```

Creates `NK_MEMBER` with `lhs` (struct expression) and `member_name`.

If the member is followed by `TK_LPAREN`, this becomes a method call. The parser transforms `obj.method(args)` into a function call `TypeName_method(&obj, args)` during semantic analysis.

### Chaining

Postfix operators can be chained arbitrarily:

```cst
arr[i].field
get_list().items[0]
matrix[row][col]
obj.get_ptr().field
```

The parser loops on postfix operators, wrapping each result as the `lhs` of the next postfix operation.

## Assignment

Assignment is not handled in the expression precedence table. Instead, after parsing an expression, the statement-level parser checks for `TK_ASSIGN` or compound assignment tokens (`TK_PLUS_EQ`, etc.):

```cst
x = 10;
arr[i] = value;
ptr.field = 42;
x += 1;
```

For compound assignment `x += y`, the parser desugars this into `x = x + y` by constructing the appropriate binary node as the right-hand side of an `NK_ASSIGN` node.
