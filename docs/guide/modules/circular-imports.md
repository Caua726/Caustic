# Circular Imports

Caustic uses module caching to handle cases where two modules import each other.

## How caching works

Each source file is parsed and semantically analyzed only once. The first time a file is imported, it is fully processed and stored in a cache. Any subsequent `use` statement referencing the same file returns the cached module immediately.

```cst
// a.cst
use "b.cst" as b;

// b.cst
use "a.cst" as a;    // uses cached version of a.cst, no re-parse
```

## Limitations

When a circular dependency exists, the second import receives the cached module in whatever state it was when caching occurred. Some symbols may not be fully resolved yet, which can lead to missing or incomplete type information.

## Best practices

- **Avoid circular dependencies.** Structure modules in a hierarchy where lower-level modules do not import higher-level ones.
- **Extract shared code.** If two modules both need the same functionality, move it into a third module that both import.

```
// Instead of A <-> B, use:
//   common.cst        (shared definitions)
//   a.cst             use "common.cst" as common;
//   b.cst             use "common.cst" as common;
```

- **Keep modules focused.** A module that only defines data types and pure functions is easy to import without creating cycles.
