# Memory Migration: Optimizer + IR Defs

## Summary

Migrate compiler allocations from `mem.galloc` (freelist) to bins/pool via `only` imports. Two parts: optimizer temp arrays → bins, IR defs → pool + bins.

## Part 1: Optimizer temp arrays → Bins

**Files:** `opt.cst`, `opt_loop.cst`, `opt_prop.cst`, `opt_helpers.cst`, `opt_sr.cst`, `cfg.cst`, `dom.cst`, `ssa.cst`

**Not in scope:** `alloc_core.cst` and `alloc_gc.cst` (register allocator in `src/codegen/`, separate subsystem — future migration).

**Import change per file:**
```cst
// Files that only alloc/free:
use "../../std/mem.cst" only bins as mem;
// Files that also need memcpy:
use "../../std/mem.cst" only bins, freelist as mem;
```

**API mapping:**
- `mem.galloc(size)` → `mem.bins_alloc(&_opt_bins, size)`
- `mem.gfree(ptr)` → `mem.bins_free(&_opt_bins, ptr)`
- `mem.memcpy(...)` → `mem.memcpy(...)` (from freelist submodule)

**Global instance:** One `mem.Bins` in `opt.cst`, initialized in `optimize()`, shared by all passes.

**Large allocations (>4096 bytes):** `ssa.cst` allocates arrays up to hundreds of KB (e.g. `nslots * MAX_DEPTH * 4`). Bins max slot is 4096. For allocations >4096, keep using `mem.galloc` from freelist, or add a mmap fallback in the bins wrapper. Decision: use `only bins, freelist as mem;` for `ssa.cst` and call `mem.galloc` for large arrays, `mem.bins_alloc` for small ones. Threshold: `size > 4096 → galloc, else → bins_alloc`.

## Part 2: IR defs → Pool + Bins

**File:** `defs.cst`

**Import:**
```cst
use "../../std/mem.cst" only bins, pool as mem;
```

**API mapping:**
- `new_inst()` → `mem.alloc(&_inst_pool)` then `memzero(...)` (Pool, fixed size. memzero MUST be preserved — pool does not zero slots)
- `new_irfunc()` → `mem.bins_alloc(&_ir_bins, sizeof(IRFunction))`
- `new_irglobal()` → `mem.bins_alloc(&_ir_bins, sizeof(IRGlobal))`
- `new_irprog()` → `mem.bins_alloc(&_ir_bins, sizeof(IRProgram))`
- `new_string_entry()` → `mem.bins_alloc(&_ir_bins, sizeof(StringEntry))`
- `str_dup(ptr, len)` → `mem.bins_alloc(&_ir_bins, len + 1)` then memcpy (strings always <4096)

**Global instances in `defs.cst`:**
- `_inst_pool`: Pool, slot_size = sizeof(IRInst), count = 131072 (enough for large compilations; pool returns null on overflow — caller must check or pool must be sized conservatively. The compiler self-compile generates ~50K IR instructions max, so 131072 has 2.5x margin)
- `_ir_bins`: Bins, page_size = 65536

**Init/cleanup API in `defs.cst`:**
```cst
fn ir_alloc_init() as void { ... }    // called from gen_ir()
fn ir_alloc_destroy() as void { ... }  // called from main.cst after codegen
```

This keeps globals and destroy logic in `defs.cst`. `main.cst` only calls `ir_alloc_destroy()` — no need to access `_inst_pool` or `_ir_bins` directly.

## Part 2b: Free IR after codegen

**File:** `main.cst`

After `codegen()` returns, call `defs.ir_alloc_destroy()`. This frees all IRInst pool memory, IR bins, and optimizer bins.

## Implementation order

1. Part 1 (optimizer) — no lifetime risk, alloc+free already paired
2. Part 2 (defs.cst) — changes allocation backend
3. Part 2b (free after codegen) — changes lifetime

Each step: implement, bootstrap O1 4th gen, test 13 examples.

## memcpy/memset/memcmp

Utility functions, not allocator functions. Files that need them import freelist alongside bins/pool:
```cst
use "../../std/mem.cst" only bins, freelist as mem;
```

Future: move memcpy/memset/memcmp to a separate utility module.
