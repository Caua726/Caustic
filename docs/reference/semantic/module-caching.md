# Module Caching

When a source file is imported with `use`, Caustic parses and analyzes it only once. Subsequent imports of the same file return the cached result. This prevents redundant work and avoids infinite loops in circular import chains.

## Import Syntax

```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;
use "mylib.cst" as lib;
```

The string is a file path (relative to the importing file or project root). The alias (`mem`, `io`, `lib`) is the name used to access the module's exports in the importing file.

## Caching Mechanism

### First Import

When a file is imported for the first time:

1. The file is read and tokenized (lexer).
2. The token stream is parsed into an AST (parser).
3. The AST undergoes semantic analysis (resolve_fields, register_vars, walk).
4. A `Module` entry is created and stored in a global cache, keyed by the resolved file path.
5. The `Module` entry stores the analyzed AST body pointer (`modd.body = body_ptr`).

### Subsequent Imports

When the same file is imported again (from a different source file):

1. The resolved file path is looked up in the module cache.
2. If found, the cached `Module` is returned immediately.
3. No re-parsing or re-analysis occurs.

### Cache Key

The cache key is the **resolved file path**, not the alias name. Two imports with different aliases but the same path share the same cached module:

```cst
use "std/mem.cst" as mem;
use "std/mem.cst" as memory;   // same file, same cached module
```

## Body Storage

When creating a `Module` entry, the body pointer must be stored (`modd.body = body_ptr`). Later passes such as `resolve_fields`, `register_vars`, and `analyze_bodies` use `cached.body` to recurse into the module's declarations. Failing to set this field causes silent failures in downstream passes.

## Circular Import Prevention

Module caching naturally prevents infinite recursion. If file A imports file B and file B imports file A:

1. A begins analysis, registers itself in the cache (partially analyzed).
2. A imports B, triggering B's analysis.
3. B imports A, finds A in the cache, and uses the (partially available) cached version.
4. B completes analysis.
5. A completes analysis.

Because the cache entry is created before recursive analysis, circular dependencies do not loop infinitely. However, symbols from A that have not yet been analyzed when B accesses them may not be fully resolved.

## Recursive Pass Visits

Each semantic pass (resolve_fields, register_vars, analyze_bodies) recursively visits imported modules using the cached body:

```
resolve_fields(ast):
    for each use-declaration in ast:
        cached = lookup_cache(path)
        resolve_fields(cached.body)
    for each struct in ast:
        compute offsets and sizes
```

This ensures that types defined in imported modules are fully resolved before the importing file uses them.

## Practical Implications

- **Performance**: Large projects with many cross-imports benefit significantly, as each standard library file (`std/mem.cst`, `std/io.cst`, etc.) is analyzed only once.
- **Consistency**: All files that import a module see the same type objects and symbol definitions.
- **Order independence**: Because modules are cached after full analysis, the import order within a single file does not affect correctness.
