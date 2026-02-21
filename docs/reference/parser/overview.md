# Parser Overview

The Caustic parser is the second phase of the compiler pipeline. It consumes the flat token list produced by the lexer and builds an abstract syntax tree (AST) represented as a linked list of `Node` structures.

## Input and Output

- **Input**: A `TokenList` (array of `Token` structs) from the lexer.
- **Output**: A linked list of `Node` structs representing the program's AST. The head of this list contains all top-level declarations.

## Source Files

The parser implementation is spread across several files in `src/parser/`:

| File | Purpose |
|------|---------|
| `top.cst` | Top-level parsing: functions, structs, enums, globals, imports, impls |
| `stmt.cst` | Statement parsing: if, while, for, return, let, defer, match, blocks |
| `expr.cst` | Expression parsing: precedence climbing, unary, postfix, primary |
| `type.cst` | Type parsing: primitives, pointers, arrays, generics, module-qualified |
| `ast.cst` | Node and Type struct definitions, node kind constants, constructors |
| `dump.cst` | AST debug printer (used by `-debugparser` flag) |
| `util.cst` | Token stream utilities: peek, advance, expect, error reporting |

## Parsing Strategy

The parser uses **recursive descent** with **precedence climbing** for expressions:

- **Top-level loop**: Repeatedly parses top-level declarations (`fn`, `struct`, `enum`, `let`, `use`, `impl`, `extern fn`) until `TK_EOF`.
- **Statements**: Parsed by dispatching on the current token kind (e.g., `TK_IF` triggers if-statement parsing, `TK_LET` triggers variable declaration parsing).
- **Expressions**: Use a precedence-climbing algorithm to correctly handle operator precedence and associativity without deep recursion.

## AST Structure

### Node Struct

The `Node` struct is a large, flat structure with approximately 40 fields that cover all possible node types. Rather than using separate structs per node kind (or a union), a single struct type is reused for all AST nodes. Unused fields are left at their zero-initialized default.

Key fields present on all nodes:

| Field | Type | Description |
|-------|------|-------------|
| `kind` | `i32` | Node kind (NK_NUM=0 through NK_FN_PTR=49) |
| `ty` | `*Type` | Type assigned during semantic analysis |
| `next` | `*Node` | Next sibling in a linked list |
| `line` | `i32` | Source line number for error reporting |

Additional fields are used depending on the node kind. For example:

- **NK_FN**: `name`, `params`, `body`, `ret_type`, `generic_params`, `is_extern`
- **NK_LET**: `name`, `let_type`, `let_init`, `is_global`, `mutability`
- **NK_IF**: `cond`, `then_body`, `else_body`
- **NK_FNCALL**: `name`, `args`, `module_name`
- **NK_ADD, NK_SUB, etc.**: `lhs`, `rhs` (binary operation children)

### Linked List Organization

AST nodes are connected via `next` pointers to form linked lists:

- A function body is a linked list of statement nodes.
- A struct's fields are a linked list of field nodes.
- Function arguments in a call are a linked list.
- Top-level declarations form the root linked list.

The parser maintains `ast_head` and `ast_tail` pointers to build the top-level list incrementally.

## Type Struct

The `Type` struct represents resolved and unresolved types in the AST:

| Field | Type | Description |
|-------|------|-------------|
| `kind` | `i32` | Type kind (TY_VOID=0 through TY_ENUM=17) |
| `base` | `*Type` | Base type for pointers and arrays |
| `array_len` | `i32` | Length for array types |
| `name` | `*u8` | Type name for named/struct/enum types |
| `size` | `i32` | Size in bytes (filled during semantic analysis) |
| `fields` | `*Node` | Field list for struct types |
| `generic_params` | ... | Generic parameter information |
| `generic_args` | ... | Concrete type arguments for generic instantiations |

See [Type Parsing](type-parsing.md) for the full type kind table.

## Top-Level Declarations

The parser recognizes these top-level constructs:

| Token | Declaration | Description |
|-------|-------------|-------------|
| `TK_FN` | Function | `fn name(params) as RetType { body }` |
| `TK_STRUCT` | Struct | `struct Name { fields }` |
| `TK_ENUM` | Enum | `enum Name { variants }` |
| `TK_LET` | Global variable | `let is Type as name with mut\|imut = value;` |
| `TK_USE` | Import | `use "path" as alias;` |
| `TK_IMPL` | Implementation | `impl TypeName { methods }` |
| `TK_EXTERN` | External function | `extern fn name(params) as RetType;` |

## Impl Block Desugaring

The parser desugars `impl` blocks at parse time. Methods inside an `impl` block are converted to top-level functions with mangled names:

```cst
impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}
```

Becomes the top-level function:

```cst
fn Point_sum(self as *Point) as i32 {
    return self.x + self.y;
}
```

This desugaring means the semantic analysis and later phases do not need special handling for methods versus functions.

## Error Handling

Parser errors are reported with the token's line and column information. Common error patterns:

- **Unexpected token**: When the current token does not match any expected production.
- **Missing semicolon**: Expected `TK_SEMICOLON` after a statement.
- **Missing closing delimiter**: Unmatched `(`, `{`, or `[`.
- **Invalid declaration**: Token sequence that does not form a valid top-level declaration.

The parser generally halts on the first error rather than attempting error recovery.

## Debug Mode

The `-debugparser` flag runs the lexer and parser phases, then prints the AST using the dump module:

```bash
./caustic -debugparser source.cst
```

This outputs a textual representation of every node in the AST, showing kinds, names, types, and tree structure.
