# Register Allocation

Caustic provides two register allocators, selected by the optimization level:

- **-O0**: Linear scan (fast compilation, decent code)
- **-O1**: Graph coloring with Iterative Register Coalescing (slow compilation, better code)

Both allocators use the same 10 allocatable registers (see [available-registers.md](available-registers.md)) and produce a `vreg_to_loc` mapping: `>= 0` means a physical register index, `< 0` means a spill slot (offset = `arg_spill_base + (-loc) * 8`), `-1000` means unallocated/dead.

## Data Structures

### LiveInterval

```cst
struct LiveInterval {
    vreg       as i32;   // virtual register index
    start      as i32;   // first instruction position
    end_pos    as i32;   // last instruction position
    reg        as i32;   // assigned register (-1 if unassigned)
    spill_loc  as i32;   // spill slot (-1 if not spilled)
    use_count  as i32;   // source operand references
    spill_cost as i32;   // cost heuristic (higher = more expensive to spill)
}
```

### AllocCtx

```cst
struct AllocCtx {
    intervals          as *LiveInterval;
    count              as i32;
    vreg_to_loc        as *i32;      // vreg -> register or spill slot
    stack_slots        as i32;
    next_spill         as i32;
    is_leaf            as i32;
    arg_spill_base     as i32;       // stack offset where spill slots begin
    aligned_stack_size as i32;       // total aligned stack frame
    func               as *u8;       // -> IRFunction
    va_save_offset     as i32;       // variadic save area offset
    call_positions     as *i32;      // sorted array of call/SET_ARG positions
    call_count         as i32;       // size of call_positions
    active_idx         as *i32;      // 10-slot active set (one per allocatable register)
    used_callee_mask   as i32;       // bitmask of callee-saved regs actually used
}
```

## Building Live Intervals

`build_intervals(func, ctx)` scans the instruction list:

1. **Label mapping**: record position of each `IR_LABEL`
2. **Call position collection**: record positions of `IR_CALL`, `IR_CALL_INDIRECT`, `IR_SYSCALL`, `IR_SET_ARG`, `IR_SET_SYS_ARG` — these are points where caller-saved registers are clobbered
3. **Vreg usage scan**: for each instruction at position `pos`:
   - `src1`/`src2` as OP_VREG: update start (min), end_pos (max), use_count++, spill_cost += 10
   - `dest` as OP_VREG: update start, end_pos, spill_cost += 1
4. **Backward jump extension**: for `IR_JMP`/`IR_JZ`/`IR_JNZ` jumping backward (loops), extend intervals of vregs defined at/before the loop header and alive at the header to span the full loop
5. **Spill cost normalization**: `cost = cost * 100 / (end_pos - start + 1)` — frequently-used short-lived vregs are expensive to spill
6. **Sort**: merge sort by start position (ties broken by shorter end first)

## -O0: Linear Scan

### Algorithm

1. Initialize `vreg_to_loc[v] = -1000` for all vregs
2. Build MOV coalescing hints: `mov_partner[v]` tracks MOV-connected vregs
3. Initialize 10-slot active set (one per allocatable register, all empty)
4. Walk sorted intervals:
   - **Expire**: clear active entries whose interval ended before current start
   - **Compute hint**: if vreg has a MOV partner already allocated, prefer that register
   - **find_free_reg**: try hint first, then callee-saved (rbx, r12, r13, r14, r15), then caller-saved (r8, r9, r10, rsi, rdi) — but only if interval does not span a call (checked via `spans_call()` binary search)
   - **Spill if full**: `spill_register()` evicts the interval with lowest spill cost (tie-break: longest remaining range)
   - **Track callee usage**: set `used_callee_mask` bits for assigned callee-saved registers
5. Set `stack_slots = next_spill - 1`

### spans_call (Binary Search)

Given an interval `[start, end]`, binary search the sorted `call_positions` array to check if any call position falls within the range. O(log n) per query.

If the interval crosses a call, caller-saved registers are excluded from the candidate set for that interval.

## -O1: Graph Coloring (Iterative Register Coalescing)

Implemented in `src/codegen/alloc_gc.cst`. Based on George & Appel 1996.

### Steps

1. **Build interference graph**: bit matrix `n x n` where `n` = vreg count. Two vregs interfere if their live intervals overlap.
2. **Collect MOV instructions**: tracked as `GCMove(src, dst)` for coalescing.
3. **Compute degrees and classify**: each vreg gets a degree (number of interfering neighbors). Classify into worklists:
   - `simplify_wl`: degree < k (trivially colorable)
   - `freeze_wl`: degree < k but has MOV-related edges
   - `spill_wl`: degree >= k
   - Where k = 10 (or 5 if interval crosses a call)
4. **Main loop** (repeat until all worklists empty):
   - **Simplify**: pop from simplify_wl, push onto stack, decrement neighbor degrees
   - **Coalesce**: try George criterion — merge MOV-related vregs if safe
   - **Freeze**: give up coalescing for a freeze_wl vreg, move to simplify
   - **Spill**: select lowest-cost vreg from spill_wl, push onto stack
5. **Select (pop stack)**: assign colors (physical registers) in LIFO order. Exclude colors used by already-colored neighbors. Exclude caller-saved if interval crosses a call. Try coalesce hint first, then first-fit.
6. **Propagate**: copy colors to coalesced aliases.
7. **Verify**: safety net — fix any remaining conflicts (call-crossing + interference).

### Spill Cost

Loop-depth-weighted: `cost = base_cost * pow10[loop_depth]` where pow10 = [1, 100, 10000, 100000]. Inner loop vregs are extremely expensive to spill.

### GET_ARG ABI Exclusion

For vregs that receive function arguments (IR_GET_ARG), the allocator excludes other parameters' ABI registers to prevent sequential GET_ARG emission from clobbering later params.

## Register Cache (-O1 only)

The instruction emitter maintains a 16-entry cache tracking which physical register currently holds which spilled vreg value. When a spilled vreg needs to be loaded, the cache is checked first — if the value is already in a register, the load is skipped.

Targeted invalidation:
- At labels, calls, and jumps: full cache clear
- At each instruction: invalidate scratch registers used
- At SET_ARG: invalidate the argument register

Eliminates ~21% of redundant memory loads.
