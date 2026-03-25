# Node Kinds

The Caustic parser produces an AST where each node has a `kind` field identifying its type. There are 53 node kinds, numbered from `NK_NUM=0` through `NK_CALL_INDIRECT=52`.

## Complete Node Kind Table

### Literal and Identifier Nodes (0-2)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_NUM` | 0 | Numeric literal (integer or float) | `int_val`, `float_val`, `ty` |
| `NK_IDENT` | 1 | Identifier reference | `name`, `ty` |
| `NK_STRING` | 2 | String literal | `str_val`, `str_len`, `ty` |

### Arithmetic Binary Operators (3-7)

| Kind | Value | Operation | Key Fields |
|------|-------|-----------|------------|
| `NK_ADD` | 3 | Addition (`+`) | `lhs`, `rhs` |
| `NK_SUB` | 4 | Subtraction (`-`) | `lhs`, `rhs` |
| `NK_MUL` | 5 | Multiplication (`*`) | `lhs`, `rhs` |
| `NK_DIV` | 6 | Division (`/`) | `lhs`, `rhs` |
| `NK_MOD` | 7 | Modulo (`%`) | `lhs`, `rhs` |

### Comparison Operators (8-13)

| Kind | Value | Operation | Key Fields |
|------|-------|-----------|------------|
| `NK_EQ` | 8 | Equal (`==`) | `lhs`, `rhs` |
| `NK_NEQ` | 9 | Not equal (`!=`) | `lhs`, `rhs` |
| `NK_LT` | 10 | Less than (`<`) | `lhs`, `rhs` |
| `NK_GT` | 11 | Greater than (`>`) | `lhs`, `rhs` |
| `NK_LE` | 12 | Less or equal (`<=`) | `lhs`, `rhs` |
| `NK_GE` | 13 | Greater or equal (`>=`) | `lhs`, `rhs` |

### Statements (14-16)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_ASSIGN` | 14 | Assignment (`=` and compound `+=`, etc.) | `lhs` (target), `rhs` (value) |
| `NK_EXPR_STMT` | 15 | Expression statement (expression used as statement) | `lhs` (the expression) |
| `NK_RETURN` | 16 | Return statement | `lhs` (return value, or null for void) |

### Declarations and Blocks (17-20)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_FN` | 17 | Function declaration | `name`, `params`, `body`, `ret_type`, `generic_params`, `is_extern` |
| `NK_FNCALL` | 18 | Function call expression | `name`, `args`, `module_name`, `generic_args` |
| `NK_BLOCK` | 19 | Block of statements (`{ ... }`) | `body` (linked list of statements) |
| `NK_LET` | 20 | Variable declaration | `name`, `let_type`, `let_init`, `is_global`, `mutability` |

### Control Flow (21, 23)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_IF` | 21 | If / else-if / else | `cond`, `then_body`, `else_body` |
| `NK_WHILE` | 23 | While loop | `cond`, `body` |

### Special Expressions (22, 24)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_ASM` | 22 | Inline assembly | `str_val` (assembly string) |
| `NK_CAST` | 24 | Type cast | `cast_type`, `lhs` (expression to cast) |

### Memory Operations (25-27, 29)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_ADDR` | 25 | Address-of (`&expr`) | `lhs` |
| `NK_DEREF` | 26 | Pointer dereference (`*expr`) | `lhs` |
| `NK_INDEX` | 27 | Array/pointer indexing (`expr[index]`) | `lhs` (base), `rhs` (index) |
| `NK_MEMBER` | 29 | Struct member access (`expr.field`) | `lhs` (struct expression), `member_name` |

### System Call (28)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_SYSCALL` | 28 | Direct Linux syscall | `args` (syscall number + arguments) |

### Logical Operators (30-31)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_LAND` | 30 | Logical AND (`&&`) | `lhs`, `rhs` |
| `NK_LOR` | 31 | Logical OR (`\|\|`) | `lhs`, `rhs` |

### Loop Control (32-33)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_BREAK` | 32 | Break from loop | (no value fields) |
| `NK_CONTINUE` | 33 | Continue to next iteration | (no value fields) |

### Bitwise Operators (34-37)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_SHL` | 34 | Shift left (`<<`) | `lhs`, `rhs` |
| `NK_SHR` | 35 | Shift right (`>>`) | `lhs`, `rhs` |
| `NK_BAND` | 36 | Bitwise AND (`&`) | `lhs`, `rhs` |
| `NK_BOR` | 37 | Bitwise OR (`\|`) | `lhs`, `rhs` |

### Sizeof and Import (38-39)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_SIZEOF` | 38 | Size of a type | `sizeof_type` |
| `NK_USE` | 39 | Module import | `module_path`, `module_alias` |

### Extended Control Flow (40-41)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_FOR` | 40 | For loop | `for_init`, `cond`, `for_update`, `body` |
| `NK_DO_WHILE` | 41 | Do-while loop | `cond`, `body` |

### Extended Bitwise (42-43)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_BXOR` | 42 | Bitwise XOR (`^`) | `lhs`, `rhs` |
| `NK_BNOT` | 43 | Bitwise NOT (`~`) | `lhs` |

### Unary and Logic (44-45)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_LNOT` | 44 | Logical NOT (`!`) | `lhs` |
| `NK_NEG` | 45 | Unary negation (`-expr`) | `lhs` |

### Enum, Match, Defer, FnPtr (46-49)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_ENUM_LIT` | 46 | Enum literal / variant reference | `name`, `enum_variant` |
| `NK_MATCH` | 47 | Match expression | `cond`, `cases` (linked list of case arms) |
| `NK_DEFER` | 48 | Deferred function call | `lhs` (the function call node) |
| `NK_FN_PTR` | 49 | Function pointer expression | `name`, `module_name`, `generic_args` |

### VA Start, Type Alias, and Indirect Call (50-52)

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `NK_VA_START` | 50 | Variadic argument list init (`__builtin_va_start`) | `lhs` (VaList pointer) |
| `NK_TYPE_ALIAS` | 51 | Type alias declaration (`type Name = Type`) | `name`, `let_type` (the aliased type) |
| `NK_CALL_INDIRECT` | 52 | Indirect function call (`call(fn_ptr, args)`) | `lhs` (fn_ptr expression), `args` (argument list) |

## Node Kind Categories Summary

| Category | Kinds | Count |
|----------|-------|-------|
| Literals / Identifiers | 0-2 | 3 |
| Arithmetic operators | 3-7 | 5 |
| Comparison operators | 8-13 | 6 |
| Statements | 14-16 | 3 |
| Declarations / Blocks | 17-20 | 4 |
| Control flow | 21, 23, 32-33, 40-41, 47 | 7 |
| Special expressions | 22, 24, 28, 38, 49, 50, 52 | 7 |
| Memory operations | 25-27, 29 | 4 |
| Logical operators | 30-31, 44 | 3 |
| Bitwise operators | 34-37, 42-43 | 6 |
| Other | 39, 45, 46, 48, 51 | 5 |

## Notes

- All binary operator nodes (arithmetic, comparison, logical, bitwise) use `lhs` and `rhs` fields.
- All unary operator nodes (deref, addr, neg, lnot, bnot) use `lhs` only.
- The `ty` field on every node is set to `null` by the parser and filled in during semantic analysis.
- The `next` field links sibling nodes in lists (statements in a block, parameters in a function, etc.).
- `NK_ASSIGN` handles both simple assignment (`=`) and compound assignment (`+=`, `-=`, etc.). For compound assignment, the parser expands `x += y` into an assignment where the right-hand side is the corresponding binary operation.
- `NK_EXPR_STMT` wraps an expression node to use it as a statement. The wrapped expression is in `lhs`.
