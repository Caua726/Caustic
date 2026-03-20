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

### Module caching
No changes needed. The existing module cache handles deduplication — if two files import the same submodule, it's parsed once.

### Internal `use` visibility
All `use` statements inside a module become visible as submodules. There is no private/public distinction. A module's internal `use "std/mem.cst" as mem;` would technically be accessible as `sdl.mem.galloc()`, but this is acceptable — no performance cost and it's not harmful.

## Compiler changes

### Lexer
- Add `TK_ONLY` keyword (token type for `only`)

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

### Semantic (`visit_use_module`)
When processing a `use` with `only`:
1. Parse the target module (the index file)
2. For each `use` statement inside the index:
   - If `only` list is present and this `use` alias is NOT in the list → skip (don't parse/walk the submodule)
   - If `only` list is absent or alias IS in the list → process normally
3. Store submodule references for dot-notation resolution

### Semantic (dot notation)
Extend member access resolution to check if the left side is a module alias with submodules:
- `sdl.video` → resolve `sdl` as module, find submodule `video`
- `sdl.video.create_window` → resolve `sdl.video` as submodule, find function `create_window`

This likely extends `walk_member` or the FNCALL resolution in `walk_fncall`.

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
