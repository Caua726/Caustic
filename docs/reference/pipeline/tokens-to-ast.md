# Tokens to AST

The parser is the second phase of the Caustic compiler. It consumes a `TokenList` produced by the lexer and builds an abstract syntax tree (AST) represented as a linked list of `Node` structs.

## Files

| File | Purpose |
|------|---------|
| `src/parser/ast.cst` | `Node` and `Type` struct definitions, node kind constants, type kind constants, singleton types, constructors |
| `src/parser/top.cst` | Top-level parsing: functions, structs, enums, imports, impl blocks, extern declarations, globals |
| `src/parser/stmt.cst` | Statement parsing: let, return, if/else, while, for, do/while, match, break, continue, defer, asm, expression statements |
| `src/parser/expr.cst` | Expression parsing with precedence climbing: binary ops, unary ops, calls, member access, indexing, cast, sizeof, syscall, fn_ptr, literals |
| `src/parser/type.cst` | Type annotation parsing: primitives, pointers, arrays, struct/enum references, generic arguments |
| `src/parser/dump.cst` | Debug printer for AST visualization |
| `src/parser/util.cst` | Token consumption, peek, expect, error helpers |

## Input and Output

**Input**: `TokenList` (flat array of `Token`), plus the source buffer and filename for error reporting.

**Output**: `*Node` -- a pointer to the first node in a linked list of top-level declarations. Each node may have child nodes forming a tree (via fields like `.lhs`, `.rhs`, `.body`, `.stmts`, `.args`, `.params`, etc.).

```cst
let is *ast.Node as root = top.parse_file(&tokens, src, file_size, filename);
```

## Parsing Strategy

The parser uses **recursive descent** for declarations and statements, and **precedence climbing** (Pratt-style) for binary expressions. There is no separate grammar specification; the grammar is encoded directly in the parsing functions.

The parser maintains a current token index into the `TokenList` and advances it with helper functions (`next()`, `expect()`, `peek()`).

## Node Structure

Every AST node is an instance of a single `Node` struct. Rather than using tagged unions or separate structs per node kind, Caustic uses one large struct with fields for every possible node type. Unused fields are zero-initialized.

```cst
struct Node {
    kind as i32;            // NK_* constant
    next as *Node;          // next sibling (linked list)
    ty as *Type;            // type annotation (set by semantic phase)
    tok_idx as i32;         // token index for error reporting
    // binary / unary
    lhs as *Node;           // left-hand side
    rhs as *Node;           // right-hand side
    val as i64;             // integer literal value
    fval as f64;            // float literal value
    // expression / statement
    expr as *Node;          // expression (return, expr_stmt, defer)
    init_expr as *Node;     // initializer (let declarations)
    // name (zero-copy from source)
    name_ptr as *u8;        // name pointer into source
    name_len as i32;        // name length
    member_ptr as *u8;      // member name (for member access)
    member_len as i32;      // member name length
    return_type as *Type;   // function return type
    flags as i32;           // variable flags (VF_MUT, VF_IMUT)
    // function
    body as *Node;          // function body block
    params as *Node;        // parameter list (linked via .next)
    is_variadic as i32;     // variadic function flag
    is_extern as i32;       // extern declaration flag
    impl_ptr as *u8;        // impl type name
    impl_len as i32;
    // call
    args as *Node;          // argument list (linked via .next)
    // generics
    generic_params as *u8;  // packed array of {*u8, i32} (12 bytes each)
    generic_param_count as i32;
    generic_args as *u8;    // packed array of *Type (8 bytes each)
    generic_arg_count as i32;
    // block
    stmts as *Node;         // statement list (linked via .next)
    // control flow
    cond as *Node;          // condition expression
    then_b as *Node;        // then branch
    else_b as *Node;        // else branch
    for_init as *Node;      // for loop initializer
    for_step as *Node;      // for loop step
    // match
    match_type as *Type;    // match expression type
    cases as *Node;         // case list (linked via .next)
    // enum literal
    discriminant as i32;    // enum variant index
    payload_args as *Node;  // enum payload arguments
    // semantic phase (filled later)
    var_offset as i32;      // stack offset for variables
    stack_size as i32;      // total stack frame size (functions)
    is_global as i32;       // global variable flag
    asm_name_ptr as *u8;    // assembly-level name
    asm_name_len as i32;
}
```

Nodes are allocated on the heap via `new_node(kind)`, which zeroes all fields and sets the `.kind`.

## Node Kinds

There are 50 node kinds (NK_NUM through NK_FN_PTR):

### Literals and Names

| Kind | Value | Description |
|------|-------|-------------|
| `NK_NUM` | 0 | Integer or float literal. `.val` for integers, `.fval` for floats. |
| `NK_IDENT` | 1 | Identifier reference. `.name_ptr`/`.name_len` point into source. |
| `NK_STRING` | 2 | String literal. `.name_ptr`/`.name_len` span the string content. |

### Binary Operators

| Kind | Value | Operation |
|------|-------|-----------|
| `NK_ADD` | 3 | `lhs + rhs` |
| `NK_SUB` | 4 | `lhs - rhs` |
| `NK_MUL` | 5 | `lhs * rhs` |
| `NK_DIV` | 6 | `lhs / rhs` |
| `NK_MOD` | 7 | `lhs % rhs` |
| `NK_EQ` | 8 | `lhs == rhs` |
| `NK_NE` | 9 | `lhs != rhs` |
| `NK_LT` | 10 | `lhs < rhs` |
| `NK_LE` | 11 | `lhs <= rhs` |
| `NK_GT` | 12 | `lhs > rhs` |
| `NK_GE` | 13 | `lhs >= rhs` |
| `NK_ASSIGN` | 14 | `lhs = rhs` (assignment) |
| `NK_LAND` | 30 | `lhs && rhs` (logical and) |
| `NK_LOR` | 31 | `lhs \|\| rhs` (logical or) |
| `NK_SHL` | 34 | `lhs << rhs` |
| `NK_SHR` | 35 | `lhs >> rhs` |
| `NK_BAND` | 36 | `lhs & rhs` |
| `NK_BOR` | 37 | `lhs \| rhs` |
| `NK_BXOR` | 42 | `lhs ^ rhs` |

### Unary Operators

| Kind | Value | Operation |
|------|-------|-----------|
| `NK_ADDR` | 25 | `&expr` (address-of) |
| `NK_DEREF` | 26 | `*expr` (dereference) |
| `NK_BNOT` | 43 | `~expr` (bitwise not) |
| `NK_LNOT` | 44 | `!expr` (logical not) |
| `NK_NEG` | 45 | `-expr` (negation) |

### Statements

| Kind | Value | Description |
|------|-------|-------------|
| `NK_EXPR_STMT` | 15 | Expression statement. `.expr` holds the expression. |
| `NK_RETURN` | 16 | Return statement. `.expr` holds the return value (or null for void). |
| `NK_LET` | 20 | Variable declaration. `.name_ptr`, `.ty`, `.init_expr`, `.flags`. |
| `NK_IF` | 21 | If statement. `.cond`, `.then_b`, `.else_b` (optional). |
| `NK_WHILE` | 23 | While loop. `.cond`, `.body`. |
| `NK_FOR` | 40 | For loop. `.for_init`, `.cond`, `.for_step`, `.body`. |
| `NK_DO_WHILE` | 41 | Do-while loop. `.body`, `.cond`. |
| `NK_BREAK` | 32 | Break statement. |
| `NK_CONTINUE` | 33 | Continue statement. |
| `NK_MATCH` | 47 | Match statement. `.expr`, `.cases` (linked list of case nodes). |
| `NK_DEFER` | 48 | Defer statement. `.expr` holds the deferred function call. |

### Declarations

| Kind | Value | Description |
|------|-------|-------------|
| `NK_FN` | 17 | Function declaration. `.name_ptr`, `.params`, `.return_type`, `.body`, `.generic_params`, `.is_variadic`, `.is_extern`. |
| `NK_USE` | 39 | Module import. `.name_ptr` (path), `.member_ptr` (alias). |

### Expressions

| Kind | Value | Description |
|------|-------|-------------|
| `NK_FNCALL` | 18 | Function call. `.name_ptr`, `.args` (linked list), `.generic_args`. |
| `NK_BLOCK` | 19 | Block expression. `.stmts` (linked list). |
| `NK_CAST` | 24 | Type cast. `.ty` (target type), `.expr` (source expression). |
| `NK_INDEX` | 27 | Array/pointer indexing. `.lhs` (base), `.rhs` (index). |
| `NK_SYSCALL` | 28 | Direct Linux syscall. `.args` (linked list of arguments). |
| `NK_MEMBER` | 29 | Member access. `.lhs` (object), `.member_ptr` (field name). |
| `NK_SIZEOF` | 38 | Sizeof operator. `.ty` (the type to measure). |
| `NK_ASM` | 22 | Inline assembly. `.name_ptr` (assembly string). |
| `NK_ENUM_LIT` | 46 | Enum literal. `.name_ptr` (enum name), `.member_ptr` (variant), `.discriminant`, `.payload_args`. |
| `NK_FN_PTR` | 49 | Function pointer creation. `.name_ptr` (function name). |

## Type Structure

Type annotations are represented by the `Type` struct:

```cst
struct Type {
    kind as i32;           // TY_* constant
    base as *Type;         // base type for pointers (*T) and arrays ([N]T)
    array_len as i32;      // array length (for TY_ARRAY)
    name_ptr as *u8;       // type name (for structs/enums)
    name_len as i32;
    size as i32;           // byte size of the type
    fields as *u8;         // struct/enum fields (filled by semantic)
    field_count as i32;
    variant_count as i32;
    // generic information
    gen_params as *u8;     // generic parameter names (packed array)
    gen_param_count as i32;
    gen_args as *u8;       // concrete type arguments (packed array of *Type)
    gen_arg_count as i32;
    // enum layout
    payload_offset as i32;
    max_payload as i32;
    base_name_ptr as *u8;  // base name before generic mangling
    base_name_len as i32;
}
```

### Type Kinds

| Kind | Value | Caustic Type | Size (bytes) |
|------|-------|-------------|------|
| `TY_VOID` | 0 | `void` | 0 |
| `TY_I8` | 1 | `i8` | 1 |
| `TY_I16` | 2 | `i16` | 2 |
| `TY_I32` | 3 | `i32` | 4 |
| `TY_I64` | 4 | `i64` | 8 |
| `TY_U8` | 5 | `u8` | 1 |
| `TY_U16` | 6 | `u16` | 2 |
| `TY_U32` | 7 | `u32` | 4 |
| `TY_U64` | 8 | `u64` | 8 |
| `TY_F32` | 9 | `f32` | 4 |
| `TY_F64` | 10 | `f64` | 8 |
| `TY_BOOL` | 11 | `bool` | 1 |
| `TY_CHAR` | 12 | `char` | 1 |
| `TY_STRING` | 13 | `string` | 8 |
| `TY_PTR` | 14 | `*T` | 8 |
| `TY_ARRAY` | 15 | `[N]T` | N * sizeof(T) |
| `TY_STRUCT` | 16 | `struct Name { ... }` | sum of field sizes |
| `TY_ENUM` | 17 | `enum Name { ... }` | tag + max payload |

### Singleton Types

Primitive types are created once at startup via `types_init()` and referenced by global pointers: `type_void`, `type_i8`, `type_i16`, `type_i32`, `type_i64`, `type_u8`, `type_u16`, `type_u32`, `type_u64`, `type_f32`, `type_f64`, `type_bool`, `type_char`, `type_string`. Pointer equality against these singletons is used throughout the compiler for type checks.

## Parsing Details

### Top-Level Declarations

The parser loops over tokens, dispatching on the current token kind:

- `TK_FN` -> function declaration
- `TK_STRUCT` -> struct declaration
- `TK_ENUM` -> enum declaration
- `TK_USE` -> module import
- `TK_LET` -> global variable
- `TK_EXTERN` -> extern function declaration
- `TK_IMPL` -> impl block (desugared to top-level functions with mangled names like `TypeName_method`)

Each top-level item becomes a `Node` appended to the linked list via `.next`.

### Expression Precedence

Binary expressions use a precedence climbing algorithm. Operator precedence (lowest to highest):

1. `||` (logical or)
2. `&&` (logical and)
3. `|` (bitwise or)
4. `^` (bitwise xor)
5. `&` (bitwise and)
6. `==`, `!=` (equality)
7. `<`, `<=`, `>`, `>=` (comparison)
8. `<<`, `>>` (shift)
9. `+`, `-` (additive)
10. `*`, `/`, `%` (multiplicative)

Unary operators (prefix `!`, `-`, `~`, `*`, `&`) are parsed before the precedence climb.

Postfix operations (function calls, member access `.`, indexing `[]`) are parsed after the primary expression.

### Variable Declarations

```cst
let is i32 as x = 10;              // immutable by default
let is i64 as count with mut = 0;  // mutable
let is i32 as MAX with imut = 100; // compile-time immutable global
let is *u8 as ptr;                 // uninitialized
let is [64]u8 as buffer;           // array
```

The parser produces an `NK_LET` node with:
- `.ty`: the declared type
- `.name_ptr`/`.name_len`: variable name
- `.init_expr`: initializer expression (or null)
- `.flags`: `VF_NONE` (0), `VF_MUT` (1), or `VF_IMUT` (2)

### Function Declarations

```cst
fn add(a as i32, b as i32) as i32 {
    return a + b;
}
```

Produces an `NK_FN` node with `.params` (linked list of parameter nodes), `.return_type`, and `.body` (an `NK_BLOCK`).

### Generic Syntax

Generic parameters and arguments are stored in packed arrays on the node:

```cst
// Generic function declaration
fn max gen T (a as T, b as T) as T { ... }

// Generic struct declaration
struct Pair gen T, U { first as T; second as U; }

// Usage with concrete types
let is i32 as m = max gen i32 (x, y);
let is Pair gen i32, i64 as p;
```

- `generic_params`: packed array of `{*u8 ptr, i32 len}` entries (12 bytes each)
- `generic_args`: packed array of `*Type` entries (8 bytes each)

### Impl Block Desugaring

The parser transforms impl blocks into top-level functions at parse time:

```cst
impl Point {
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
    fn new(x as i32, y as i32) as Point { ... }
}
```

Becomes:

```cst
fn Point_sum(self as *Point) as i32 {
    return self.x + self.y;
}
fn Point_new(x as i32, y as i32) as Point { ... }
```

The `impl_ptr`/`impl_len` fields on the function node record the original type name for later method resolution by the semantic phase.
