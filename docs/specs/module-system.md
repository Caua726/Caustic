# Module System — `only` filter + submodules

## Overview

Extend Caustic's `use` statement to support submodule filtering and dot-notation access. Enables modular libraries (like SDL3 bindings) where users import only what they need.

## Current state

```cst
use "std/mem.cst" as mem;
mem.galloc(1024);
```

`use "path" as alias;` imports a whole file. No submodules, no partial imports, no re-export.

## New syntax

```cst
// Import everything (unchanged)
use "sdl3.cst" as sdl;
sdl.video.create_window("game", 800, 600, 0);
sdl.events.poll();

// Import only specific submodules (new)
use "sdl3.cst" only video, events as sdl;
sdl.video.create_window("game", 800, 600, 0);  // ok
sdl.events.poll();                               // ok
sdl.audio.open_device();                         // error: audio not imported
```

## Index file convention

A module that re-exports submodules is a normal `.cst` file with `use` statements. Each `use` inside it becomes a submodule accessible via dot notation:

```cst
// sdl3.cst (index file)
use "init.cst" as init;
use "video.cst" as video;
use "render.cst" as render;
use "events.cst" as events;
use "audio.cst" as audio;
```

When someone does `use "sdl3.cst" as sdl;`, all internal `use` aliases become submodules: `sdl.init`, `sdl.video`, `sdl.render`, etc.

## Semantics

### Without `only` (import everything)
- Parse and compile all submodules (all `use` statements in the index file)
- All submodules accessible via `alias.submodule.function()`

### With `only` (partial import)
- Parse and compile ONLY the listed submodules
- Skip parsing/compiling unlisted submodules — this is the performance win
- Accessing unlisted submodules is a semantic error

### Dot notation resolution
`sdl.video.create_window()` resolves as:
1. `sdl` → module alias (the index file)
2. `.video` → submodule (the `use "video.cst" as video` inside the index)
3. `.create_window()` → function inside `video.cst`

### Internal `use` visibility
All `use` statements inside a module become visible as submodules. There is no private/public distinction. A module's internal `use "std/mem.cst" as mem;` would technically be accessible as `sdl.mem.galloc()`, but this is acceptable — no performance cost and it's not harmful.

## Compiler changes

### Lexer
- Add `TK_ONLY` with imut = 78 (next available after TK_TYPE = 77)

### Parser (`parse_use`)
Current: `use STRING as IDENT ;`
New: `use STRING [only IDENT (, IDENT)*] as IDENT ;`

Parse steps:
1. Consume `TK_USE`
2. Consume string literal (path)
3. If next token is `TK_ONLY`:
   - Consume `TK_ONLY`
   - Parse comma-separated identifier list until `TK_AS`
   - Store list in AST node
4. Consume `TK_AS`
5. Consume identifier (alias)
6. Consume `;`

### AST
Add to the `use` node:
- `only_names` — array of identifiers (null if no `only`)
- `only_count` — number of identifiers in the filter

### Semantic (defs.cst)
Add a `SubModule` struct and submodule list to `Module`:

```cst
struct SubModule {
    alias_ptr as *u8;   // "video", "events", etc.
    alias_len as i32;
    module_ref as *u8;  // pointer to the child Module
    next as *u8;        // linked list
}
```

Add to `Module`:
- `submodules as *u8` — head of SubModule linked list (null if no submodules)
- `submodule_count as i32`

### Semantic (`visit_use_module`)
When processing a `use` with `only`:
1. Parse the target module (the index file)
2. For each `use` statement inside the index:
   - If `only` list is present and this `use` alias is NOT in the list → skip (don't parse/walk the submodule)
   - If `only` list is absent or alias IS in the list → process normally
3. For each processed submodule, create a `SubModule` entry and attach to the parent Module's `submodules` list

### Semantic (dot notation)
Extend member access resolution to check if the left side is a module alias with submodules:
- `sdl.video` → resolve `sdl` as Module, walk `submodules` list to find entry with alias `video`, return its `module_ref`
- `sdl.video.create_window` → resolve `sdl.video` as sub-Module, find function `create_window` via normal function lookup

This extends `walk_fncall` (Case 2 — module.function) to handle chained dot access.

### Module caching with `only`
The `only` filter does NOT affect what is cached — it restricts what is **accessible**. All submodules of the same index file are always parsed into the same Module cache entry. The `only` filter controls which SubModule entries are created in the parent Module's submodule list. This avoids the problem where two files importing the same index with different `only` filters would get incompatible cache states.

The "performance win" of `only` is therefore at the **name resolution** level (unused submodules are not linked/walked), not at the parse level. For large libraries, the main saving is in reduced codegen (functions from unimported submodules are not marked reachable).

## Files to change

| File | Change |
|------|--------|
| `src/lexer/tokens.cst` | Add `TK_ONLY` |
| `src/lexer/lexer.cst` | Recognize `only` keyword |
| `src/parser/top.cst` | Extend `parse_use` for `only` syntax |
| `src/parser/ast.cst` | Add `only_names`/`only_count` to use node |
| `src/semantic/walk.cst` | Filter submodules, resolve dot notation for submodules |

## Validation

```cst
// test_modules.cst
use "test_lib.cst" only math as lib;
let is i32 as r = lib.math.add(1, 2);
// lib.strings.concat() should error

// test_lib.cst (index)
use "test_math.cst" as math;
use "test_strings.cst" as strings;

// test_math.cst
fn add(a as i32, b as i32) as i32 { return a + b; }

// test_strings.cst
fn concat() as void { }
```

Bootstrap gen4 must remain deterministic after changes.
