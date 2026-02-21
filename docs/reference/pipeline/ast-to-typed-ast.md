# AST to Typed AST

The semantic analysis phase is the third phase of the Caustic compiler. It walks the AST produced by the parser, checks types, resolves names, processes module imports, instantiates generics, and annotates every node with its resolved type.

## Files

| File | Purpose |
|------|---------|
| `src/semantic/walk.cst` | Main semantic walker (`walk()` and `analyze()`), module loading |
| `src/semantic/types.cst` | Type checking, coercion, struct/enum lookup, type resolution |
| `src/semantic/scope.cst` | Scope stack management, variable lookup, path resolution |
| `src/semantic/modules.cst` | Module caching, file reading for imports |
| `src/semantic/generics.cst` | Generic instantiation (monomorphization), template management |
| `src/semantic/defs.cst` | Data structure definitions: Variable, Scope, Function, Module, StructDef, EnumDef, GenericTemplate, GenericInstance |

## Input and Output

**Input**: Untyped `*Node` linked list from the parser, plus filename and filename length.

**Output**: The same `*Node` linked list, mutated in place. After semantic analysis:

- Every node has `.ty` set to a valid `*Type` pointer
- Variables have `.var_offset` set (stack position within their function)
- Functions have `.stack_size` set (total stack frame bytes)
- Functions and variables have `.asm_name_ptr`/`.asm_name_len` set (assembly-level names, including module prefixes)
- Generic templates have been instantiated: concrete `NK_FN` and struct `Type` nodes appended to the AST

```cst
sem.analyze(cast(*u8, root), filename, fname_len);
```

## The walk() Function

The core of semantic analysis is `walk()`, which takes a `*Node` and processes it based on its `.kind`. It is a large switch dispatching on node kinds:

- **NK_NUM**: Sets `.ty` based on whether the literal has a float value or integer value. Integer literals default to `type_i64` but can narrow to smaller types during assignment.
- **NK_IDENT**: Looks up the name in the scope chain. Sets `.ty` to the variable's type. Sets `.var_offset` and `.is_global`.
- **NK_STRING**: Sets `.ty` to `type_string` (which is `*u8` / 8 bytes).
- **NK_LET**: Resolves the declared type, walks the initializer if present, checks type compatibility, registers the variable in the current scope with a stack offset.
- **NK_FN**: Pushes a new scope, registers parameters, walks the body, pops the scope. Records the function in a global function table with its return type and parameter types.
- **NK_FNCALL**: Looks up the function name, matches argument types against parameter types, resolves generic arguments if present. Handles three cases: (1) simple function call, (2) module-qualified call (`mod.func()`), and (3) method call desugaring (`obj.method()` -> `Type_method(&obj)`).
- **NK_ASSIGN**: Walks both sides, checks that the left side is assignable (mutable variable, dereference, or indexed access), checks type compatibility.
- **NK_IF / NK_WHILE / NK_FOR / NK_DO_WHILE**: Walks conditions, pushes scopes for bodies, walks bodies.
- **NK_MATCH**: Walks the match expression, checks each case pattern against the match type, walks case bodies.
- **NK_RETURN**: Walks the return expression, checks it against the current function's return type.
- **Binary operators** (NK_ADD, NK_SUB, etc.): Walks both operands, checks type compatibility, sets `.ty` to the result type.
- **NK_CAST**: Resolves the target type, walks the source expression, validates the cast.
- **NK_MEMBER**: Resolves struct field access. Looks up the field name in the struct's field list, sets `.ty` to the field type.
- **NK_INDEX**: Validates array or pointer indexing.
- **NK_SYSCALL**: Walks arguments, sets `.ty` to `type_i64`.
- **NK_DEFER**: Validates that the deferred expression is a function call.
- **NK_USE**: Triggers module loading (see below).

## Scope Management

Scopes form a stack connected via `.parent` pointers:

```cst
struct Scope {
    vars as *Variable;    // linked list of variables in this scope
    parent as *Scope;     // enclosing scope
    is_global as i32;     // 1 for the global scope
    next_all as *Scope;   // linked list of all scopes (for cleanup)
}
```

```cst
struct Variable {
    name_ptr as *u8;
    name_len as i32;
    ty as *u8;            // *Type, cast to *u8 for cross-module compat
    flags as i32;         // VF_NONE, VF_MUT, VF_IMUT
    offset as i32;        // stack offset within function
    is_global as i32;
    is_extern as i32;
    is_module as i32;     // 1 if this is a module alias
    module_ref as *u8;    // pointer to Module struct
    asm_name_ptr as *u8;
    asm_name_len as i32;
    next as *Variable;
}
```

Name resolution walks upward from the current scope through parent scopes until a matching name is found or the global scope is exhausted.

The `stack_offset` is a global variable that tracks the current stack position within a function. Each local variable declaration increments it by the variable's type size. When a function finishes, its total `stack_size` is recorded on the `NK_FN` node.

## Module Resolution

When the walker encounters an `NK_USE` node:

1. The import path is resolved relative to the current source file's directory
2. The module cache is checked; if already loaded, the cached AST and scope are reused
3. If not cached, the file is read, lexed, and parsed (the semantic phase recursively calls the lexer and parser)
4. The imported AST is walked (analyzed) recursively
5. A `Module` entry is created with the module's scope, prefix, and body pointer
6. A variable with `is_module = 1` is registered in the current scope under the import alias

```cst
struct Module {
    name_ptr as *u8;       // alias name
    name_len as i32;
    scope as *Scope;       // module's top-level scope
    prefix_ptr as *u8;     // name prefix for assembly names
    prefix_len as i32;
    body as *u8;           // *Node pointer to module AST
    source_dir_ptr as *u8;
    source_dir_len as i32;
    vars_done as i32;      // flag: global vars registered
    bodies_done as i32;    // flag: function bodies analyzed
    next as *Module;
}
```

Module loading preserves and restores the current source directory, current scope, and module prefix to correctly handle nested imports.

## Multi-Pass Analysis

Semantic analysis processes the AST in multiple passes:

1. **Register struct/enum types**: First pass collects all struct and enum declarations, creating `StructDef` and `EnumDef` entries so that types can reference each other.
2. **Resolve struct fields**: Second pass resolves the types of struct fields and enum variants, computing sizes.
3. **Register global variables**: Third pass processes global `let` declarations.
4. **Register function signatures**: Fourth pass collects function signatures (name, parameter types, return type) into the global function table.
5. **Analyze function bodies**: Final pass walks all function bodies, performing full type checking and name resolution.

This ordering ensures that forward references work: a function can call another function declared later in the file, and structs can reference each other (via pointers).

## Generic Instantiation

Generics use monomorphization. Each generic function or struct is a template that gets cloned and specialized for each concrete type combination used.

```cst
struct GenericTemplate {
    name_ptr as *u8;       // template name (e.g., "max", "Pair")
    name_len as i32;
    params as *u8;         // generic parameter names
    param_count as i32;
    ast as *u8;            // *Node pointer to the template AST
    struct_type as *u8;    // *Type for generic struct templates
    is_function as i32;    // 1 for function templates, 0 for struct templates
    mod_prefix_ptr as *u8;
    mod_prefix_len as i32;
    next as *GenericTemplate;
}
```

When a generic usage is encountered (e.g., `max gen i32`):

1. Look up the template by base name
2. Check if this specialization already exists (by mangled name, e.g., `max_i32`)
3. If not, deep-clone the template AST
4. Substitute all generic parameter types with concrete types throughout the clone
5. Mangle the name: `max` + `_` + `i32` = `max_i32`; `Pair` + `_i32_i64` = `Pair_i32_i64`
6. Append the instantiated AST to the end of the main AST linked list
7. Walk (analyze) the instantiation immediately

The `GenericInstance` struct tracks which specializations have already been created to avoid duplicates.

## Impl Method Resolution

When the semantic phase encounters a function call on a member expression:

```cst
p.sum()       // where p is of type Point
Point.new()   // associated function (no self)
```

It resolves these using the following logic:

- **Method call** (`p.sum()`): If `p` has struct type `Point`, look for a function named `Point_sum`. Desugar to `Point_sum(&p)`, inserting the address of `p` as the first argument.
- **Associated function** (`Point.new()`): If `Point` is a known struct or enum name (checked via `lookup_struct` / `lookup_enum`), look for `Point_new`. No implicit `self` argument.

## Type Checking Rules

The type checker enforces:

- **Assignment compatibility**: The right-hand side type must match the left-hand side, with automatic narrowing for integer literals (e.g., `i64` literal 42 can narrow to `i32`)
- **Binary operations**: Both operands must have compatible types; result type follows standard promotion rules
- **Function calls**: Argument count and types must match the function signature (variadic functions accept extra arguments)
- **Return types**: Every return statement's expression type must match the function's declared return type
- **Pointer arithmetic**: Adding/subtracting integers to pointers is allowed
- **Cast validation**: Casts between numeric types, and between pointers and integers, are permitted
- **Struct field access**: The field name must exist in the struct type
- **Array indexing**: The index must be an integer type; the base must be an array or pointer

## Error Reporting

Semantic errors include the filename, line, and column from the originating token. Errors call `linux.exit(1)` immediately -- there is no error recovery or collection of multiple errors.
