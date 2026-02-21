# Scopes

Caustic uses a scope stack for lexical name resolution. Each scope contains a symbol table that maps names to their corresponding AST nodes (and therefore their types).

## Scope Stack Model

The scope stack is a linked list of scope frames. When a new block is entered, a scope is pushed. When the block exits, the scope is popped. Name lookups search from the innermost scope outward until a match is found or the global scope is exhausted.

```
 [innermost scope]  -- local block variables
        |
 [function scope]   -- parameters, local lets
        |
 [global scope]     -- top-level functions, globals, structs
```

## Operations

### push_scope()

Creates a new empty scope and pushes it onto the stack. Called at the start of:

- Function bodies
- `if` / `else` blocks
- `while` / `for` loop bodies
- `match` arms
- Bare `{ }` blocks

### pop_scope()

Removes the current innermost scope from the stack. All symbols declared in that scope become inaccessible.

### declare(name, node)

Adds a name-to-node binding in the current (innermost) scope. If the name already exists in the current scope, a redeclaration error is reported. Shadowing names from outer scopes is permitted.

### lookup(name)

Searches for a name starting from the innermost scope and working outward. Returns the first matching node, or null if not found. This implements standard lexical scoping.

## Scope Kinds

### Global Scope

The outermost scope, created at the start of compilation. Contains:

- Top-level function declarations
- Global variable declarations (`let ... with mut` or `let ... with imut`)
- Struct and enum type names
- Extern function declarations
- Module aliases (from `use` statements)

Global declarations are registered during the `register_vars` pass before function bodies are analyzed, enabling forward references.

### Function Scope

Created when entering a function body. Contains the function's parameters as local variables:

```cst
fn add(a as i32, b as i32) as i32 {
    // scope contains: a, b
    return a + b;
}
```

### Block Scope

Created for any `{ }` block. Variables declared inside are local to that block:

```cst
fn example() as void {
    let is i32 as x = 1;
    {
        let is i32 as y = 2;   // y only visible in this block
        let is i32 as x = 3;   // shadows outer x
    }
    // y is no longer accessible here
    // x refers to the outer x (value 1)
}
```

### Loop Scope

`for` loops create a scope that includes the loop variable:

```cst
for let is i32 as i = 0; i < 10; i = i + 1 {
    // i is declared in the loop's scope
}
// i is no longer accessible here
```

## Shadowing

A variable in an inner scope can shadow a variable with the same name in an outer scope. The inner declaration takes precedence for all lookups within that scope:

```cst
let is i32 as x = 10;

fn foo() as void {
    let is i64 as x = 20;   // shadows global x; different type is fine
    // x here is i64
}
// x here is i32 (global)
```

## Symbol Table Implementation

Each scope's symbol table is a simple linear structure (linked list or flat array of name-node pairs). Lookup is a linear scan. This is efficient for typical scope sizes in practice, and the implementation avoids external dependencies.

## Module Scope

When a module is imported via `use`, its exported symbols are not merged into the current scope. Instead, the module alias is registered, and member access (`alias.name`) triggers a lookup into the cached module's symbol table:

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    let is *u8 as p = mem.galloc(1024);  // looks up galloc in mem's exports
    mem.gfree(p);
    return 0;
}
```

This prevents name collisions between modules and the importing file.
