# Fix: Module System Bootstrap Failure

## Problem

The `feature/module-system` branch added submodule support (dot notation: `sdl.video.create_window()`). Examples work, but **gen1 cannot compile itself** — bootstrap fails at gen2:

```
Error in sem:213:57: submodulo nao encontrado
```

Gen1 (built by the old compiler) compiles fine. But when gen1 tries to compile `src/main.cst` (which uses `use` statements internally), the submodule resolution code incorrectly treats internal `use` statements as submodules and fails to resolve them.

## Root Cause

The submodule-building code in `visit_use_module()` (pass 1) creates SubModule entries for ALL `use` statements inside any imported module. But the compiler's own modules (like `std/mem.cst`, `std/io.cst`) also contain `use` statements. When the compiler compiles itself, these internal `use` statements get misinterpreted as submodules, causing "submodulo nao encontrado" errors.

The issue is in `src/semantic/walk.cst` — the code that builds submodule lists from nested `use` statements runs unconditionally for every module, not just for index files that are imported with submodule access.

## What to Fix

In `src/semantic/walk.cst`, the submodule list building code (the loop that creates `SubModule` entries after `register_structs(body)`) should only create submodules when the parent module is actually used as a submodule container. Options:

1. **Only build submodule lists for modules that have child `use` statements AND are accessed via dot notation** — but we don't know at parse time how they'll be accessed
2. **Build submodule lists always, but don't error when a variable access matches a module that has no submodules** — more defensive
3. **Only build submodule lists when the importing `use` statement has the `only` keyword** — too restrictive, breaks `use "sdl3.cst" as sdl; sdl.video.func()`
4. **Gate submodule creation: only create SubModule entries when the child module's body ALSO contains `use` statements** — prevents leaf modules from polluting the submodule namespace

The most likely fix is that `walk_fncall` and `walk_member` (the chained dot access code added in commits `1f50205` and `3d3f90a`) are matching too aggressively. When a module has no SubModule entries, the code should fall through to normal member/method resolution instead of erroring.

## How to Reproduce

```bash
git checkout feature/module-system
rm -rf .caustic
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
rm -rf .caustic
/tmp/gen1 src/main.cst  # THIS FAILS with "submodulo nao encontrado"
```

## How to Verify Fix

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
# gen2=gen3=gen4 must be equal
```

Also test that submodule examples still work:

```bash
# Create test files
echo 'use "test_math.cst" as math;' > /tmp/test_lib.cst
echo 'fn add(a as i32, b as i32) as i32 { return a + b; }' > /tmp/test_math.cst
echo 'use "test_lib.cst" as lib; fn main() as i32 { return lib.math.add(3, 4); }' > /tmp/test_sub.cst
/tmp/gen4 /tmp/test_sub.cst && ./caustic-as /tmp/test_sub.cst.s && ./caustic-ld /tmp/test_sub.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
# Expected: Exit: 7
```

## Files to Check

- `src/semantic/walk.cst` — `visit_use_module()` (submodule list building), `walk_fncall()` (Case 2 chained dot access), `walk_member()` (chained variable access)
- `src/semantic/defs.cst` — `SubModule` struct, `Module.submodules` field

## Additional Context

- The float ABI fix (`src/codegen/emit.cst`, `src/ir/gen.cst`, `src/lexer/util.cst`) is also uncommitted on this branch
- The caustic-mk depend changes (`caustic-maker/`, `src/main.cst`, `src/semantic/scope.cst`) are also uncommitted
- All of these need the bootstrap to work before they can be committed properly
- Read CLAUDE.md for build commands and language syntax
