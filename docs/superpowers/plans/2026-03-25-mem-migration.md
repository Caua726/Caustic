# Memory Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate IR struct allocation from freelist to pool+bins, add free-after-codegen to reclaim memory.

**Architecture:** Pool for IRInst (fixed-size, O(1), thousands of allocations), Bins for other IR structs and strings (variable-size, O(1)). Cleanup function in defs.cst called from main.cst after codegen.

**Tech Stack:** Caustic compiler, std/mem/pool.cst, std/mem/bins.cst

**Spec:** `docs/superpowers/specs/2026-03-25-mem-migration-design.md`

---

## ⚠️ Part 1 Deferred

The spec's Part 1 (optimizer temp arrays → bins) is **not feasible** with the current bins allocator. Verification shows optimizer arrays routinely exceed 4096 bytes:

- `vc_alloc * 8` where vc_alloc reaches 4601 → **36808 bytes** (opt.cst, opt_prop.cst)
- `inst_count * 8` where inst_count reaches thousands → **16000+ bytes** (opt_helpers.cst, opt_sr.cst)
- `nblocks * nblocks * 4` → **40000+ bytes** (dom.cst)

Bins max slot is 4096. All these allocations would return NULL and crash. Part 1 requires either increasing bins max size or adding a mmap fallback for large allocations. **Deferred to a separate plan.**

---

## Task 1: Add global Pool and Bins instances to defs.cst

**Files:**
- Modify: `src/ir/defs.cst`

- [ ] **Step 1: Change import**

In `src/ir/defs.cst`, change:
```cst
use "../../std/mem.cst" as mem;
```
to:
```cst
use "../../std/mem.cst" only bins, pool as mem;
use "../../std/mem.cst" only freelist as fl;
```
`mem` gives access to bins+pool. `fl` gives access to memcpy/memzero via freelist (needed by existing `memzero` function and `str_dup`).

- [ ] **Step 2: Add global allocator instances**

After the imports, add:
```cst
let is mem.Pool as _inst_pool with mut;
let is mem.Bins as _ir_bins with mut;
let is i32 as _ir_alloc_ready with mut = 0;
```

- [ ] **Step 3: Add init and destroy functions**

```cst
fn ir_alloc_init() as void {
    if (_ir_alloc_ready == 0) {
        _inst_pool = mem.create(sizeof(IRInst), 131072, 0);
        _ir_bins = mem.bins_new(65536);
        _ir_alloc_ready = 1;
    }
}

fn ir_alloc_destroy() as void {
    if (_ir_alloc_ready == 1) {
        mem.destroy(&_inst_pool);
        mem.bins_destroy(&_ir_bins);
        _ir_alloc_ready = 0;
    }
}
```

- [ ] **Step 4: Verify compilation**

```bash
./caustic src/main.cst 2>&1 | tail -3
```
Expected: `wrote: src/main.cst.s` (no errors)

- [ ] **Step 5: Commit**

```bash
git add src/ir/defs.cst
git commit -m "ir/defs: add pool+bins instances for IR allocation"
```

---

## Task 2: Migrate new_inst to Pool

**Files:**
- Modify: `src/ir/defs.cst`

- [ ] **Step 1: Change new_inst to use pool**

Replace:
```cst
fn new_inst(op as i32) as *IRInst {
    let is *IRInst as i = cast(*IRInst, mem.galloc(sizeof(IRInst)));
    memzero(cast(*u8, i), sizeof(IRInst));
    i.op = op;
    return i;
}
```
With:
```cst
fn new_inst(op as i32) as *IRInst {
    let is *IRInst as i = cast(*IRInst, mem.alloc(&_inst_pool));
    memzero(cast(*u8, i), sizeof(IRInst));
    i.op = op;
    return i;
}
```

Note: `memzero` MUST be preserved — pool does not zero slots. `memzero` itself uses a byte loop, no dependency on freelist.

- [ ] **Step 2: Change memzero to not depend on freelist**

Check that `memzero` in defs.cst is self-contained (inline byte loop). It should be — verify:
```bash
grep -A5 "fn memzero" src/ir/defs.cst
```
Expected: `while (i < n) { p[i] = cast(u8, 0); i = i + 1; }` — no mem.* calls.

- [ ] **Step 3: Verify compilation**

```bash
./caustic src/main.cst 2>&1 | tail -3
```

- [ ] **Step 4: Commit**

```bash
git add src/ir/defs.cst
git commit -m "ir/defs: new_inst uses pool instead of freelist"
```

---

## Task 3: Migrate other IR allocators to Bins

**Files:**
- Modify: `src/ir/defs.cst`

- [ ] **Step 1: Change new_irfunc, new_irglobal, new_irprog, new_string_entry**

Replace each `mem.galloc(sizeof(X))` with `mem.bins_alloc(&_ir_bins, sizeof(X))`:

```cst
fn new_irfunc() as *IRFunction {
    let is *IRFunction as f = cast(*IRFunction, mem.bins_alloc(&_ir_bins, sizeof(IRFunction)));
    memzero(cast(*u8, f), sizeof(IRFunction));
    return f;
}

fn new_irglobal() as *IRGlobal {
    let is *IRGlobal as g = cast(*IRGlobal, mem.bins_alloc(&_ir_bins, sizeof(IRGlobal)));
    memzero(cast(*u8, g), sizeof(IRGlobal));
    return g;
}

fn new_irprog() as *IRProgram {
    let is *IRProgram as p = cast(*IRProgram, mem.bins_alloc(&_ir_bins, sizeof(IRProgram)));
    memzero(cast(*u8, p), sizeof(IRProgram));
    return p;
}

fn new_string_entry() as *StringEntry {
    let is *StringEntry as s = cast(*StringEntry, mem.bins_alloc(&_ir_bins, sizeof(StringEntry)));
    memzero(cast(*u8, s), sizeof(StringEntry));
    return s;
}
```

- [ ] **Step 2: Change str_dup**

Replace:
```cst
fn str_dup(src as *u8, len as i32) as *u8 {
    let is *u8 as dst = mem.galloc(cast(i64, len) + 1);
```
With:
```cst
fn str_dup(src as *u8, len as i32) as *u8 {
    let is *u8 as dst = mem.bins_alloc(&_ir_bins, cast(i64, len) + 1);
```

`str_dup` also calls `mem.memcpy`. With `only bins, pool as mem`, `mem.memcpy` resolves to `mem.cst`'s top-level `memcpy` wrapper (which delegates to `freelist.memcpy`). This works because the `only` filter exposes submodule functions but the parent's top-level functions are still accessible as fallback. Verified by testing.

- [ ] **Step 3: Verify compilation**

```bash
./caustic src/main.cst 2>&1 | tail -3
```

- [ ] **Step 4: Commit**

```bash
git add src/ir/defs.cst
git commit -m "ir/defs: IR structs + str_dup use bins instead of freelist"
```

---

## Task 4: Call ir_alloc_init from gen_ir

**Files:**
- Modify: `src/ir/gen_program.cst`

- [ ] **Step 1: Add ir_alloc_init call**

In `gen_ir()`, at the very start (before any `new_inst` or `new_irprog` call), add:
```cst
d.ir_alloc_init();
```

Check the current first lines of `gen_ir`:
```bash
grep -A5 "fn gen_ir" src/ir/gen_program.cst | head -8
```

Add `d.ir_alloc_init();` as the first line after the depth increment.

- [ ] **Step 2: Verify compilation**

```bash
./caustic src/main.cst 2>&1 | tail -3
```

- [ ] **Step 3: Bootstrap O1 4th gen**

```bash
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/g1 && /tmp/g1 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/o1g1 && /tmp/o1g1 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/o1g2 && /tmp/o1g2 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/o1g3 && diff <(xxd /tmp/o1g2) <(xxd /tmp/o1g3) && echo "fixpoint" && /tmp/o1g3 -O1 src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/o1g4 && diff <(xxd /tmp/o1g3) <(xxd /tmp/o1g4) && echo "4th gen" && cp /tmp/o1g4 ./caustic
```
Expected: `fixpoint` then `4th gen`

- [ ] **Step 4: Test all 13 examples**

```bash
for f in examples/fibonacci.cst examples/bench.cst examples/sorting.cst examples/strings.cst examples/enums.cst examples/hello.cst examples/test_arr.cst examples/test_sum.cst examples/linked_list.cst examples/generics.cst examples/test_qs.cst examples/random.cst examples/test_cache.cst; do name=$(basename "$f" .cst); ./caustic -O1 "$f" 2>/dev/null && ./caustic-as "${f}.s" 2>/dev/null && ./caustic-ld "${f}.s.o" -o "/tmp/ex_${name}" 2>/dev/null && /tmp/ex_${name} > /dev/null 2>&1; echo "$name: $?"; rm -f "${f}.s" "${f}.s.o" "/tmp/ex_${name}" 2>/dev/null; done
```
Expected: all pass (same exit codes as before)

- [ ] **Step 5: Commit**

```bash
git add src/ir/gen_program.cst src/ir/defs.cst caustic
git commit -m "ir: pool+bins allocation for IR structs, bootstrap verified"
```

---

## Task 5: Add ir_alloc_destroy call after codegen (Part 2b)

**Files:**
- Modify: `src/main.cst`

- [ ] **Step 1: Add destroy call in compile_backend**

`main.cst` imports `defs.cst` as `ird` (line 9: `use "ir/defs.cst" as ird;`).

`codegen` is called in two places:
- `compile_backend()` line 495 — used by both single-file and multi-file paths
- `run_single_file()` line 663 — direct single-file path

Add `ird.ir_alloc_destroy();` after each `codegen()` call. Since `compile_backend` covers both paths, adding it there handles multi-file automatically:

In `compile_backend()`, after line 495 (`cgd.codegen(...)`):
```cst
ird.ir_alloc_destroy();
```

In `run_single_file()`, after line 663 (`cgd.codegen(...)`):
```cst
ird.ir_alloc_destroy();
```

- [ ] **Step 2: Multi-file mode is already handled**

In `run_multi_file()`, each file calls `compile_one()` → `compile_backend()` → `codegen()` → `ir_alloc_destroy()`. Then `gheapreset` runs. Next iteration, `gen_ir()` calls `ir_alloc_init()` which re-creates pool+bins (guarded by `_ir_alloc_ready == 0`). The order is correct: destroy before reset, init before use.

- [ ] **Step 4: Bootstrap O1 4th gen + test all examples**

Same commands as Task 4 Steps 3-4.

- [ ] **Step 5: Rebuild assembler and linker**

```bash
./caustic -O1 caustic-assembler/main.cst && ./caustic-as caustic-assembler/main.cst.s && ./caustic-ld caustic-assembler/main.cst.s.o -o /tmp/new-as && cp /tmp/new-as ./caustic-as
./caustic -O1 caustic-linker/main.cst && ./caustic-as caustic-linker/main.cst.s && ./caustic-ld caustic-linker/main.cst.s.o -o /tmp/new-ld && cp /tmp/new-ld ./caustic-ld
```

- [ ] **Step 6: Final bootstrap with new AS/LD + test**

Repeat bootstrap O1 4th gen with new assembler/linker. Test all examples.

- [ ] **Step 7: Commit**

```bash
git add src/main.cst src/ir/gen_program.cst caustic caustic-as caustic-ld
git commit -m "main: free IR memory after codegen via ir_alloc_destroy"
git push
```

---

## Verification checklist

After all tasks:
- [ ] O1 4th gen fixpoint passes
- [ ] All 13 examples produce correct results
- [ ] Self-compile memory usage decreased (check with RSS measurement)
- [ ] No segfaults on any example with O0 or O1
- [ ] Multi-file compilation still works (`run_multi_file` path)
