# Semantic Analysis Overview

Semantic analysis is the third phase of the Caustic compiler pipeline. It receives the raw AST produced by the parser and transforms it into a fully typed AST where every node has its `node.ty` field set to a valid `Type` pointer.

## Purpose

The semantic analyzer performs three core tasks:

1. **Type resolution** -- Every expression, variable, and declaration is assigned a concrete type.
2. **Validation** -- Type mismatches, undeclared identifiers, and invalid operations are detected and reported as compile-time errors.
3. **Desugaring** -- High-level constructs like method calls (`obj.method()`) and generic instantiations (`func gen i32(x)`) are rewritten into simpler forms the IR generator can consume.

## Sub-Phases

Semantic analysis runs in three ordered passes over the AST:

### 1. resolve_fields

Iterates over all struct and enum declarations to compute field offsets and total type sizes. This pass uses packed layout (no alignment padding). Generic template structs (those with `generic_param_count > 0`) are skipped -- they are resolved later when instantiated with concrete type arguments.

```
resolve_fields(ast_head)
  for each NODE_KIND_STRUCT:
    compute field offsets (cumulative sum)
    set struct.size = total
  for each NODE_KIND_ENUM:
    compute tag size (4 bytes)
    compute max_payload across variants
    set enum.size = 4 + max_payload
```

### 2. register_vars

Registers all top-level declarations (functions, globals, extern functions) into the global scope. This makes forward references possible -- a function can call another function declared later in the file.

### 3. analyze_bodies (walk)

The main recursive walk over the AST. Each node is visited by the `walk()` function, which dispatches on `node.kind` to perform type checking, scope management, and type assignment. This is where the bulk of semantic work happens.

Generic template functions (those with `generic_param_count > 0`) are skipped during this pass. They are only analyzed when instantiated with concrete types.

## Source Files

| File | Purpose |
|------|---------|
| `walk.cst` | Main AST walker; visits each node kind and sets `node.ty` |
| `types.cst` | Type constructors, singleton globals, type comparison utilities |
| `scope.cst` | Scope stack for lexical name resolution |
| `modules.cst` | Module import, caching, and cross-module symbol lookup |
| `generics.cst` | Generic template storage, AST cloning, type substitution |
| `defs.cst` | Shared constants and definitions (node kinds, type kinds) |

## Pipeline Position

```
Source (.cst)
    |
  Lexer      -- tokenize
    |
  Parser     -- build AST
    |
  Semantic   -- type-check and annotate   <-- this phase
    |
  IR         -- generate virtual-register IR
    |
  Codegen    -- emit x86_64 assembly
    |
  Assembly (.s)
```

## Key Invariants

- After semantic analysis completes, every node in the AST has a non-null `node.ty`.
- All type pointers for primitive types use global singletons (`type_i32`, `type_i64`, etc.). Pointer equality is used for type comparison, so constructing a new `Type` struct with the same kind will break checks.
- The `stack_offset` global variable tracks the current stack frame size during function body analysis. It must be saved and restored around any re-entrant walk calls (such as generic instantiation).
- Module caching ensures each file is parsed and analyzed at most once, even if imported from multiple locations.
