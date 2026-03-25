# Memory Migration: Optimizer + IR Defs

## Summary

Migrate compiler allocations from `mem.galloc` (freelist) to bins/pool via `only` imports. Two parts: optimizer temp arrays → bins, IR defs → pool + bins.

## Part 1: Optimizer temp arrays → Bins

**Files:** `opt.cst`, `opt_loop.cst`, `opt_prop.cst`, `opt_helpers.cst`, `opt_sr.cst`, `cfg.cst`, `dom.cst`

**Import change per file:**
```cst
// Before:
use "../../std/mem.cst" as mem;
// After (files that only alloc/free):
use "../../std/mem.cst" only bins as mem;
// After (files that also need memcpy):
use "../../std/mem.cst" only bins, freelist as mem;
```

**API mapping:**
- `mem.galloc(size)` → `mem.bins_alloc(&_opt_bins, size)`
- `mem.gfree(ptr)` → `mem.bins_free(&_opt_bins, ptr)`
- `mem.memcpy(...)` → `mem.memcpy(...)` (stays, from freelist)

**Global instance:** One `mem.Bins` in `opt.cst`, initialized in `optimize()`, shared by all passes.

**Files needing freelist (for memcpy):** `alloc_core.cst` (merge sort uses memcpy)

## Part 2: IR defs → Pool + Bins

**File:** `defs.cst`

**Import:**
```cst
use "../../std/mem.cst" only bins, pool as mem;
```

**API mapping:**
- `new_inst()` → `mem.alloc(&_inst_pool)` (Pool, fixed size sizeof(IRInst))
- `new_irfunc()` → `mem.bins_alloc(&_ir_bins, sizeof(IRFunction))`
- `new_irglobal()` → `mem.bins_alloc(&_ir_bins, sizeof(IRGlobal))`
- `new_irprog()` → `mem.bins_alloc(&_ir_bins, sizeof(IRProgram))`
- `new_string_entry()` → `mem.bins_alloc(&_ir_bins, sizeof(StringEntry))`
- `str_dup(ptr, len)` → `mem.bins_alloc(&_ir_bins, len + 1)` then memcpy

**Global instances in `defs.cst`:**
- `_inst_pool`: Pool, slot_size = sizeof(IRInst), count = 65536
- `_ir_bins`: Bins, page_size = 65536

**Init function:** `ir_alloc_init()` called from `gen_ir()` before IR generation.

## Part 2b: Free IR after codegen

**File:** `main.cst`

After `codegen()` returns, call cleanup:
- `pool.destroy(&_inst_pool)` — frees all IRInst memory
- `bins.bins_destroy(&_ir_bins)` — frees IRFunction, IRGlobal, strings
- `bins.bins_destroy(&_opt_bins)` — frees optimizer temp arrays

This is new behavior — today nothing is freed.

## Implementation order

1. Part 1 (optimizer) — no lifetime risk, alloc+free already paired
2. Part 2 (defs.cst) — changes allocation backend
3. Part 2b (free after codegen) — changes lifetime

Each step: implement, bootstrap O1 4th gen, test 13 examples.

## memcpy/memset/memcmp

These are utility functions, not allocator functions. Files that need them import freelist alongside bins/pool:
```cst
use "../../std/mem.cst" only bins, freelist as mem;
```

Or they can be moved to a separate utility module later.
