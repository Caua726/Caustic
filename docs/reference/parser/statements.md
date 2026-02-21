# Statement Parsing

Statements are the building blocks of function bodies and control flow blocks. The parser dispatches on the current token kind to determine which statement form to parse.

## Statement Dispatch

When parsing a statement, the parser checks the current token:

| Token | Statement Type | Reference |
|-------|---------------|-----------|
| `TK_RETURN` | Return statement | [Return](#return-statement) |
| `TK_LET` | Local variable declaration | [Let](#let-statement) |
| `TK_IF` | If / else-if / else | [If](#if-statement) |
| `TK_WHILE` | While loop | [While](#while-loop) |
| `TK_FOR` | For loop | [For](#for-loop) |
| `TK_DO` | Do-while loop | [Do-While](#do-while-loop) |
| `TK_MATCH` | Match expression | [Match](#match-statement) |
| `TK_DEFER` | Deferred call | [Defer](#defer-statement) |
| `TK_LBRACE` | Block | [Block](#block) |
| `TK_BREAK` | Loop break | [Break](#break-and-continue) |
| `TK_CONTINUE` | Loop continue | [Break](#break-and-continue) |
| (other) | Expression statement | [Expression Statement](#expression-statement) |

## Expression Statement

Any expression followed by a semicolon is a valid statement. The parser wraps the expression in an `NK_EXPR_STMT` node.

```cst
foo(42);
x + 1;      // valid but the result is discarded
syscall(60, 0);
```

If the expression is followed by `TK_ASSIGN` or a compound assignment token, the parser instead produces an `NK_ASSIGN` node (see [Expression Parsing](expressions.md#assignment)).

```cst
x = 10;
arr[i] += 1;
```

All expression statements and assignments must end with `TK_SEMICOLON`.

## Return Statement

Syntax: `return expr;` or `return;`

```cst
return 0;
return x + y;
return;          // void return
```

Parsing:
1. Consume `TK_RETURN`.
2. If the next token is `TK_SEMICOLON`, this is a void return: create `NK_RETURN` with null `lhs`.
3. Otherwise, parse an expression as the return value, set it as `lhs`.
4. Consume `TK_SEMICOLON`.

The `NK_RETURN` node's `lhs` field holds the return value expression, or null for void returns.

When deferred calls are active, the code generator inserts deferred call execution between evaluating the return value and the actual return instruction.

## Let Statement

Syntax: `let is Type as name = expr;` or `let is Type as name;`

```cst
let is i64 as x = 10;
let is *u8 as ptr;
let is [64]u8 as buffer;
let is Point as p = Point.new(1, 2);
```

Parsing:
1. Consume `TK_LET`.
2. Consume `TK_IS`.
3. Parse the type (see [Type Parsing](type-parsing.md)).
4. Consume `TK_AS`.
5. Consume `TK_IDENT` as the variable name.
6. If the next token is `TK_ASSIGN`:
   a. Consume `TK_ASSIGN`.
   b. Parse the initializer expression, set as `let_init`.
7. Consume `TK_SEMICOLON`.

Creates an `NK_LET` node with:
- `name`: variable name
- `let_type`: the declared type
- `let_init`: initializer expression (or null if uninitialized)

Uninitialized variables are zero-initialized on the stack.

## If Statement

Syntax: `if (cond) { body }`, optionally followed by `else if` or `else` chains.

```cst
if (x > 0) {
    return 1;
}

if (x > 0) {
    return 1;
} else {
    return 0;
}

if (x > 10) {
    return 2;
} else if (x > 5) {
    return 1;
} else {
    return 0;
}
```

Parsing:
1. Consume `TK_IF`.
2. Consume `TK_LPAREN`.
3. Parse the condition expression, set as `cond`.
4. Consume `TK_RPAREN`.
5. Parse the then-body (a block `{ ... }`), set as `then_body`.
6. If the next token is `TK_ELSE`:
   a. Consume `TK_ELSE`.
   b. If the next token is `TK_IF`, recursively parse another if-statement and set it as `else_body` (creating an else-if chain).
   c. Otherwise, parse a block as `else_body`.

The `NK_IF` node has:
- `cond`: condition expression
- `then_body`: body executed when condition is true
- `else_body`: body for else/else-if (or null)

Else-if chains are represented as nested `NK_IF` nodes in the `else_body` field.

## While Loop

Syntax: `while (cond) { body }`

```cst
while (i < 10) {
    i += 1;
}
```

Parsing:
1. Consume `TK_WHILE`.
2. Consume `TK_LPAREN`.
3. Parse the condition expression, set as `cond`.
4. Consume `TK_RPAREN`.
5. Parse the body block.

Creates `NK_WHILE` with `cond` and `body`.

## For Loop

Syntax: `for (init; cond; update) { body }`

```cst
for (let is i64 as i = 0; i < 10; i += 1) {
    process(i);
}
```

Parsing:
1. Consume `TK_FOR`.
2. Consume `TK_LPAREN`.
3. Parse the initializer statement (typically a `let`), set as `for_init`.
4. Parse the condition expression, set as `cond`.
5. Consume `TK_SEMICOLON`.
6. Parse the update expression (typically an assignment), set as `for_update`.
7. Consume `TK_RPAREN`.
8. Parse the body block.

Creates `NK_FOR` with `for_init`, `cond`, `for_update`, and `body`.

Note: The initializer's semicolon is consumed as part of the let statement parsing. The condition's semicolon is consumed explicitly between condition and update.

## Do-While Loop

Syntax: `do { body } while (cond);`

```cst
do {
    n = read_byte();
} while (n != 0);
```

Parsing:
1. Consume `TK_DO`.
2. Parse the body block.
3. Consume `TK_WHILE`.
4. Consume `TK_LPAREN`.
5. Parse the condition expression.
6. Consume `TK_RPAREN`.
7. Consume `TK_SEMICOLON`.

Creates `NK_DO_WHILE` with `body` and `cond`.

## Match Statement

Syntax: `match (expr) { case value: { body } ... }`

```cst
match (code) {
    case 0: {
        return "zero";
    }
    case 1: {
        return "one";
    }
}
```

Parsing:
1. Consume `TK_MATCH`.
2. Consume `TK_LPAREN`.
3. Parse the match target expression, set as `cond`.
4. Consume `TK_RPAREN`.
5. Consume `TK_LBRACE`.
6. Parse case arms in a loop:
   a. Consume `TK_CASE`.
   b. Parse the case value expression.
   c. Consume `TK_COLON`.
   d. Parse the case body block.
   e. Link case nodes together.
7. Consume `TK_RBRACE`.

Creates `NK_MATCH` with `cond` and `cases` (linked list of case arm nodes).

Each case arm has a value expression and a body block. Cases are compared for equality with the match target. There is no fallthrough between cases.

## Defer Statement

Syntax: `defer fncall;`

```cst
defer mem.gfree(ptr);
defer close(fd);
```

Parsing:
1. Consume `TK_DEFER`.
2. Parse an expression (must be a function call, enforced semantically).
3. Consume `TK_SEMICOLON`.

Creates `NK_DEFER` with `lhs` set to the function call node.

Rules:
- Only function calls are valid after `defer`. Using a non-call expression is a semantic error.
- `defer syscall(...)` is not supported. Wrap the syscall in a helper function.
- Multiple defers execute in LIFO order (last defer registered executes first).
- The return value of a function is evaluated before deferred calls run.
- Defers inside control flow blocks (if, while, for, match) are scoped to that block.

## Break and Continue

Syntax: `break;` and `continue;`

```cst
while (true) {
    if (done) {
        break;
    }
    if (skip) {
        continue;
    }
    process();
}
```

Parsing:
1. Consume `TK_BREAK` or `TK_CONTINUE`.
2. Consume `TK_SEMICOLON`.

Creates `NK_BREAK` or `NK_CONTINUE` node with no value fields. These nodes must appear inside a loop body; usage outside a loop is a semantic error.

## Block

Syntax: `{ statements... }`

```cst
{
    let is i32 as temp = compute();
    use_value(temp);
}
```

Parsing:
1. Consume `TK_LBRACE`.
2. Parse statements in a loop until `TK_RBRACE`.
3. Consume `TK_RBRACE`.

Creates `NK_BLOCK` with `body` as a linked list of the contained statements. Blocks introduce a new scope for variable declarations.

## Inline Assembly Statement

While `asm(...)` is technically an expression, it is most commonly used as a statement:

```cst
asm("mov rax, 60\n");
asm("xor rdi, rdi\n");
asm("syscall\n");
```

When used as a statement, it is wrapped in an `NK_EXPR_STMT` node containing the `NK_ASM` expression.
