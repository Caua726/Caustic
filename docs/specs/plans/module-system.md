# Module System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `only` keyword and submodule dot-notation access (`sdl.video.create_window()`) to Caustic's import system.

**Architecture:** Add `TK_ONLY` token, extend `parse_use()` to parse the `only ident, ident as alias` syntax, add `SubModule` struct to `defs.cst`, extend `visit_use_module()` to build submodule lists, and extend `walk_fncall()` Case 2 to resolve chained dot access through submodules.

**Tech Stack:** Caustic self-hosted compiler (.cst), x86_64 Linux

**Spec:** `docs/specs/module-system.md`

---

## File Structure

| File | Responsibility | Action |
|------|---------------|--------|
| `src/lexer/tokens.cst` | Token constants + keyword lookup | Modify: add TK_ONLY = 78 |
| `src/parser/ast.cst` | AST node definitions | Modify: add only_names/only_count to Node |
| `src/parser/top.cst` | Use statement parsing | Modify: extend parse_use() |
| `src/semantic/defs.cst` | Data structures | Modify: add SubModule struct, add fields to Module |
| `src/semantic/walk.cst` | Semantic analysis passes | Modify: extend visit_use_module() + walk_fncall() |

---

### Task 1: Add TK_ONLY token

**Files:**
- Modify: `src/lexer/tokens.cst`

- [ ] **Step 1:** In `src/lexer/tokens.cst`, add the constant after TK_TYPE (line ~77):

```cst
let is i32 as TK_ONLY with imut = 78;
```

- [ ] **Step 2:** In the `keyword_lookup` function, find the `len == 4` branch (handles `with`, `else`, `cast`, `enum`, `impl`, `type`). Add `only` check:

```cst
if (s[0] == 111 && s[1] == 110 && s[2] == 108 && s[3] == 121) { return TK_ONLY; } // only
```

- [ ] **Step 3:** Build and test — compile should still work since nothing uses TK_ONLY yet:

```bash
rm -rf .caustic && ./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
/tmp/gen1 examples/hello.cst && ./caustic-as examples/hello.cst.s && ./caustic-ld examples/hello.cst.s.o -o /tmp/t && /tmp/t
```

- [ ] **Step 4:** Bootstrap gen4 to verify no regression:

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
# gen2=gen3=gen4
```

- [ ] **Step 5:** Commit:

```bash
git add src/lexer/tokens.cst
git commit -m "lexer: add TK_ONLY keyword token"
```

---

### Task 2: Add only_names/only_count to AST Node

**Files:**
- Modify: `src/parser/ast.cst`

- [ ] **Step 1:** Find the `Node` struct in `src/parser/ast.cst` (~line 106). Add two fields at the end of the struct (before the closing `}`):

```cst
    only_names as *u8;    // array of (ptr, len) pairs for "only" filter. null = import all
    only_count as i32;    // number of names in only filter. 0 = import all
```

`only_names` stores a flat array: `[ptr0, len0, ptr1, len1, ...]` where each pair is `(*u8, i32)` = 12 bytes per entry (8 + 4).

- [ ] **Step 2:** Build and bootstrap gen4:

```bash
rm -rf .caustic && ./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
rm -rf .caustic && /tmp/gen1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen2
echo "gen1=$(stat -c%s /tmp/gen1) gen2=$(stat -c%s /tmp/gen2)"
```

- [ ] **Step 3:** Commit:

```bash
git add src/parser/ast.cst
git commit -m "ast: add only_names/only_count fields to Node"
```

---

### Task 3: Extend parse_use() to parse `only` syntax

**Files:**
- Modify: `src/parser/top.cst`

- [ ] **Step 1:** Find `parse_use()` in `src/parser/top.cst` (~line 218). Currently:

```cst
fn parse_use() as *ast.Node {
    let is i32 as tidx = pu.p_pos;
    pu.expect(tok.TK_USE);
    if (pu.cur().kind != tok.TK_STRING) { pu.error_expected("expected path string after 'use'"); }
    let is *ast.Node as n = ast.new_node(ast.NK_USE);
    n.tok_idx = tidx;
    n.name_ptr = pu.cur().ptr;
    n.name_len = pu.cur().len;
    pu.advance();
    if (pu.cur().kind == tok.TK_AS) {
        pu.advance();
        if (pu.cur().kind != tok.TK_IDENT) { pu.error_expected("expected alias after 'as'"); }
        n.member_ptr = pu.cur().ptr;
        n.member_len = pu.cur().len;
        pu.advance();
    }
    pu.expect(tok.TK_SEMICOLON);
    return n;
}
```

Replace with:

```cst
fn parse_use() as *ast.Node {
    let is i32 as tidx = pu.p_pos;
    pu.expect(tok.TK_USE);
    if (pu.cur().kind != tok.TK_STRING) { pu.error_expected("expected path string after 'use'"); }
    let is *ast.Node as n = ast.new_node(ast.NK_USE);
    n.tok_idx = tidx;
    n.name_ptr = pu.cur().ptr;
    n.name_len = pu.cur().len;
    pu.advance();
    // Parse optional "only name1, name2, ..."
    if (pu.cur().kind == tok.TK_ONLY) {
        pu.advance();
        // Collect names until we hit TK_AS
        let is [32]i64 as name_ptrs;
        let is [32]i32 as name_lens;
        let is i32 as count with mut = 0;
        while (pu.cur().kind == tok.TK_IDENT && count < 32) {
            name_ptrs[count] = cast(i64, pu.cur().ptr);
            name_lens[count] = pu.cur().len;
            count = count + 1;
            pu.advance();
            if (pu.cur().kind == tok.TK_COMMA) { pu.advance(); }
        }
        if (count > 0) {
            // Pack into flat array: 12 bytes per entry (8 ptr + 4 len)
            let is *u8 as buf = mem.galloc(cast(i64, count) * 12);
            let is i32 as oi with mut = 0;
            while (oi < count) {
                let is *i64 as pp = cast(*i64, cast(i64, buf) + cast(i64, oi) * 12);
                *pp = name_ptrs[oi];
                let is *i32 as lp = cast(*i32, cast(i64, buf) + cast(i64, oi) * 12 + 8);
                *lp = name_lens[oi];
                oi = oi + 1;
            }
            n.only_names = buf;
            n.only_count = count;
        }
    }
    if (pu.cur().kind == tok.TK_AS) {
        pu.advance();
        if (pu.cur().kind != tok.TK_IDENT) { pu.error_expected("expected alias after 'as'"); }
        n.member_ptr = pu.cur().ptr;
        n.member_len = pu.cur().len;
        pu.advance();
    }
    pu.expect(tok.TK_SEMICOLON);
    return n;
}
```

- [ ] **Step 2:** Make sure `mem` is imported in `top.cst` (check if `use "../../std/mem.cst" as mem;` exists at the top).

- [ ] **Step 3:** Test that existing `use` statements still parse correctly:

```bash
rm -rf .caustic && ./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
for ex in hello fibonacci structs enums linked_list; do
    /tmp/gen1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 10 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done
```

- [ ] **Step 4:** Write a test file that uses `only`:

```cst
// examples/test_only.cst
use "std/mem.cst" only galloc, gfree as mem;

fn main() as i32 {
    return 0;
}
```

Compile to verify parsing works (semantic will fail since we haven't implemented filtering yet, but it should parse):

```bash
/tmp/gen1 examples/test_only.cst 2>&1
# Should get past parsing (semantic error is OK at this point)
```

- [ ] **Step 5:** Bootstrap gen4:

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
```

- [ ] **Step 6:** Commit:

```bash
git add src/parser/top.cst
git commit -m "parser: parse 'use path only name1, name2 as alias' syntax"
```

---

### Task 4: Add SubModule struct and extend Module

**Files:**
- Modify: `src/semantic/defs.cst`

- [ ] **Step 1:** Add the `SubModule` struct after the existing `Module` struct:

```cst
struct SubModule {
    alias_ptr as *u8;
    alias_len as i32;
    module_ref as *u8;
    next as *u8;
}
```

- [ ] **Step 2:** Add `new_submodule` function:

```cst
fn new_submodule() as *SubModule {
    let is *SubModule as s = cast(*SubModule, mem.galloc(sizeof(SubModule)));
    memzero(cast(*u8, s), sizeof(SubModule));
    return s;
}
```

- [ ] **Step 3:** Add two fields to the `Module` struct:

```cst
    submodules as *u8;       // head of SubModule linked list (null if none)
    submodule_count as i32;
```

- [ ] **Step 4:** Build and bootstrap gen2 to verify struct layout doesn't break anything:

```bash
rm -rf .caustic && ./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
rm -rf .caustic && /tmp/gen1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen2
echo "gen1=$(stat -c%s /tmp/gen1) gen2=$(stat -c%s /tmp/gen2)"
```

- [ ] **Step 5:** Commit:

```bash
git add src/semantic/defs.cst
git commit -m "defs: add SubModule struct and submodule fields to Module"
```

---

### Task 5: Extend visit_use_module() to build submodule lists

**Files:**
- Modify: `src/semantic/walk.cst`

This is the core change. When a module's body contains `use` statements, those become submodules of the parent.

- [ ] **Step 1:** In `visit_use_module()`, in pass 1 (the `if (pass_id == 1)` block, ~line 961-995), after loading and caching the module, check if the loaded module body contains NK_USE nodes. For each one, create a SubModule entry:

After the line `register_structs(body);` (recursive call), add:

```cst
// Build submodule list from nested use statements
let is *ast.Node as child with mut = cast(*ast.Node, body);
while (cast(i64, child) != 0) {
    if (child.kind == ast.NK_USE && child.member_len > 0) {
        // This nested use becomes a submodule
        let is *d.Module as child_cached = mod.find_module(
            sc.get_mod_prefix(child.name_ptr + 1, child.name_len - 2),
            sc.get_mod_prefix_len(child.name_ptr + 1, child.name_len - 2));
        if (cast(i64, child_cached) != 0) {
            let is *d.SubModule as sm = d.new_submodule();
            sm.alias_ptr = d.str_dup(child.member_ptr, child.member_len);
            sm.alias_len = child.member_len;
            sm.module_ref = cast(*u8, child_cached);
            sm.next = mm.submodules;
            mm.submodules = cast(*u8, sm);
            mm.submodule_count = mm.submodule_count + 1;
        }
    }
    child = child.next;
}
```

Note: `mm` is the parent Module being created. This code must be inside the block where `mm` is available (after `let is *d.Module as mm = d.new_module();`).

- [ ] **Step 2:** Handle the `only` filter. In the same pass 1 block, before recursing into `register_structs(body)`, check `n.only_count`. If > 0, skip loading submodules not in the list:

Add a helper function before `visit_use_module`:

```cst
fn is_in_only_list(name_ptr as *u8, name_len as i32, only_names as *u8, only_count as i32) as i32 {
    if (only_count == 0) { return 1; } // no filter = allow all
    let is i32 as i with mut = 0;
    while (i < only_count) {
        let is *i64 as pp = cast(*i64, cast(i64, only_names) + cast(i64, i) * 12);
        let is *i32 as lp = cast(*i32, cast(i64, only_names) + cast(i64, i) * 12 + 8);
        if (d.str_eq(name_ptr, name_len, cast(*u8, *pp), *lp) == 1) { return 1; }
        i = i + 1;
    }
    return 0;
}
```

Then in the recursive processing, the parent `use` node's `only_count`/`only_names` should propagate to filter which child uses are processed. This needs careful threading — the `only` filter belongs to the import site, not the module itself.

The simplest approach: store the parent's `only_names`/`only_count` in a global during `visit_use_module`, and check it before recursing into child uses.

- [ ] **Step 3:** Build and test with existing code (no `only` used yet, should not affect behavior):

```bash
rm -rf .caustic && ./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
for ex in hello fibonacci structs enums linked_list bench; do
    /tmp/gen1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done
```

- [ ] **Step 4:** Bootstrap gen4:

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
```

- [ ] **Step 5:** Commit:

```bash
git add src/semantic/walk.cst
git commit -m "semantic: build submodule lists from nested use statements"
```

---

### Task 6: Extend walk_fncall() for chained dot access

**Files:**
- Modify: `src/semantic/walk.cst`

- [ ] **Step 1:** In `walk_fncall()`, find Case 2 (member access call, ~line 521). Currently it handles `mod.func()` by checking `var.is_module == 1`. We need to also handle `mod.submod.func()`.

The key insight: `sdl.video.create_window()` is parsed as a nested NK_MEMBER:
```
FNCALL
  lhs: MEMBER
    lhs: MEMBER
      lhs: IDENT "sdl"
      member: "video"
    member: "create_window"
  args: ...
```

So `node.lhs` is `MEMBER(MEMBER(IDENT "sdl", "video"), "create_window")`.

The current code checks if `node.lhs.lhs` is NK_IDENT. For chained access, `node.lhs.lhs` is NK_MEMBER. We need to handle this case.

Add this code BEFORE the existing `if (lhs_expr.kind == ast.NK_IDENT)` block in Case 2:

```cst
// Chained module access: sdl.video.func()
if (lhs_expr.kind == ast.NK_MEMBER && lhs_expr.lhs.kind == ast.NK_IDENT) {
    // lhs_expr = MEMBER(IDENT "sdl", "video")
    // node.lhs = MEMBER(lhs_expr, "create_window")
    let is *d.Variable as parent_var = sc.lookup_var(lhs_expr.lhs.name_ptr, lhs_expr.lhs.name_len);
    if (cast(i64, parent_var) != 0 && parent_var.is_module == 1) {
        let is *d.Module as parent_mod = cast(*d.Module, parent_var.module_ref);
        // Find submodule by alias
        let is *d.SubModule as sm with mut = cast(*d.SubModule, parent_mod.submodules);
        while (cast(i64, sm) != 0) {
            if (d.str_eq(sm.alias_ptr, sm.alias_len, lhs_expr.member_ptr, lhs_expr.member_len) == 1) {
                // Found submodule — lookup function in it
                let is *d.Module as sub_mod = cast(*d.Module, sm.module_ref);
                func = sc.lookup_fn_qualified(node.lhs.member_ptr, node.lhs.member_len,
                                              sub_mod.prefix_ptr, sub_mod.prefix_len);
                if (cast(i64, func) == 0) {
                    sc.sem_error_at(node, "funcao nao encontrada no submodulo");
                }
                sm = cast(*d.SubModule, 0); // break
            } else {
                sm = cast(*d.SubModule, sm.next);
            }
        }
        if (cast(i64, func) == 0 && cast(i64, sm) == 0) {
            sc.sem_error_at(node, "submodulo nao encontrado");
        }
    }
}
```

- [ ] **Step 2:** Create test files:

```cst
// examples/test_submod_lib.cst (index)
use "test_submod_math.cst" as math;
```

```cst
// examples/test_submod_math.cst
fn add(a as i32, b as i32) as i32 { return a + b; }
fn mul(a as i32, b as i32) as i32 { return a * b; }
```

```cst
// examples/test_submod.cst
use "test_submod_lib.cst" as lib;

fn main() as i32 {
    let is i32 as r = lib.math.add(3, 4);
    return r; // exit code 7
}
```

- [ ] **Step 3:** Test:

```bash
/tmp/gen1 examples/test_submod.cst && ./caustic-as examples/test_submod.cst.s && ./caustic-ld examples/test_submod.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
# Expected: Exit: 7
```

- [ ] **Step 4:** Test `only` filter:

```cst
// examples/test_only2.cst
use "test_submod_lib.cst" only math as lib;

fn main() as i32 {
    let is i32 as r = lib.math.mul(5, 6);
    return r; // exit code 30
}
```

```bash
/tmp/gen1 examples/test_only2.cst && ./caustic-as examples/test_only2.cst.s && ./caustic-ld examples/test_only2.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
# Expected: Exit: 30
```

- [ ] **Step 5:** Bootstrap gen4:

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
```

- [ ] **Step 6:** Clean up test files and commit:

```bash
rm -f examples/test_submod*.cst examples/test_only*.cst
rm -f examples/test_submod*.cst.s examples/test_only*.cst.s
rm -f examples/test_submod*.cst.s.o examples/test_only*.cst.s.o
git add src/semantic/walk.cst
git commit -m "semantic: chained dot access for submodules (sdl.video.func())"
```

---

### Task 7: Handle submodule variable access (non-function)

**Files:**
- Modify: `src/semantic/walk.cst`

Submodules may also have constants: `sdl.init.VIDEO`. This is a variable access, not a function call. The `walk_member` function needs to handle this.

- [ ] **Step 1:** Find `walk_member` in `walk.cst` (the handler for NK_MEMBER nodes). Add chained module variable resolution similar to Task 6 but for variable lookup instead of function lookup.

When `sdl.init.VIDEO` is encountered:
1. `sdl` → module variable
2. `init` → submodule
3. `VIDEO` → variable in submodule's scope

Look for the existing module variable access pattern (where it checks `var.is_module`) and extend it to handle the chained case.

- [ ] **Step 2:** Test with constants:

```cst
// examples/test_submod_const_lib.cst
use "test_submod_const_vals.cst" as vals;

// examples/test_submod_const_vals.cst
let is i32 as MAGIC with imut = 42;

// examples/test_submod_const.cst
use "test_submod_const_lib.cst" as lib;

fn main() as i32 {
    return lib.vals.MAGIC; // exit code 42
}
```

```bash
/tmp/gen1 examples/test_submod_const.cst && ./caustic-as examples/test_submod_const.cst.s && ./caustic-ld examples/test_submod_const.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
# Expected: Exit: 42
```

- [ ] **Step 3:** Bootstrap gen4 and commit:

```bash
rm -f examples/test_submod_const*.cst*
git add src/semantic/walk.cst
git commit -m "semantic: submodule constant/variable access (sdl.init.VIDEO)"
```

---

### Task 8: Final integration test and cleanup

- [ ] **Step 1:** Create a comprehensive test that exercises all features:

```cst
// examples/test_modules_full.cst
use "test_mod_index.cst" as lib;
use "test_mod_index.cst" only math as lib2;

fn main() as i32 {
    // Full import — access all submodules
    let is i32 as a = lib.math.add(10, 20);
    let is i32 as b = lib.util.double(15);

    // Only import — access only math
    let is i32 as c = lib2.math.add(1, 2);

    // Return sum as exit code
    return a + b - c; // 30 + 30 - 3 = 57
}

// test_mod_index.cst
use "test_mod_math.cst" as math;
use "test_mod_util.cst" as util;

// test_mod_math.cst
fn add(a as i32, b as i32) as i32 { return a + b; }

// test_mod_util.cst
fn double(x as i32) as i32 { return x * 2; }
```

- [ ] **Step 2:** Test:

```bash
/tmp/gen1 examples/test_modules_full.cst && ./caustic-as examples/test_modules_full.cst.s && ./caustic-ld examples/test_modules_full.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
# Expected: Exit: 57
```

- [ ] **Step 3:** Run all existing examples to confirm no regressions:

```bash
for ex in hello fibonacci structs enums linked_list bench; do
    rm -rf .caustic && /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done
```

- [ ] **Step 4:** Final bootstrap gen4 -O1:

```bash
rm -rf .caustic
for g in 1 2 3 4; do
    rm -rf .caustic
    if [ $g -eq 1 ]; then ./caustic src/main.cst; else /tmp/gen$((g-1)) -O1 src/main.cst; fi
    ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
done
echo "gen2=$(stat -c%s /tmp/gen2) gen3=$(stat -c%s /tmp/gen3) gen4=$(stat -c%s /tmp/gen4)"
# gen2=gen3=gen4
```

- [ ] **Step 5:** Clean up test files and commit:

```bash
rm -f examples/test_mod*.cst*
git add -A
git commit -m "module system: only filter + submodule dot access complete"
```
