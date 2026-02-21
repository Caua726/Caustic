# Register Allocation

Caustic uses a linear scan register allocator to map unlimited virtual registers (vregs) down to
a small set of physical registers. The allocator is implemented in `src/codegen/alloc.cst`.

## Algorithm Overview

The linear scan algorithm processes vregs in order of their first use (start position) and
assigns each one a physical register. When all registers are occupied, the allocator spills the
vreg whose interval extends furthest into the future, freeing a register for the current vreg.

Steps:

1. **Build live intervals**: Scan the instruction list to determine each vreg's start and end
   positions.
2. **Sort intervals**: Insertion sort by start position; ties broken by end position (shorter
   intervals first).
3. **Allocate**: Walk sorted intervals, assign registers or spill.

## LiveInterval Struct

```cst
struct LiveInterval {
    vreg      as i32;   -- virtual register index
    start     as i32;   -- first instruction position where vreg appears
    end_pos   as i32;   -- last instruction position where vreg appears
    reg       as i32;   -- assigned physical register index (-1 if unassigned)
    spill_loc as i32;   -- spill slot index (-1 if not spilled)
    use_count as i32;   -- number of times vreg is referenced as a source operand
    spill_cost as i32;  -- heuristic cost of spilling (higher = more expensive)
}
```

## AllocCtx Struct

```cst
struct AllocCtx {
    intervals        as *LiveInterval;  -- array of intervals (one per vreg)
    count            as i32;            -- total number of intervals
    vreg_to_loc      as *i32;          -- vreg index -> physical register or spill slot
    stack_slots      as i32;            -- number of spill slots consumed
    next_spill       as i32;            -- next available spill slot index
    is_leaf          as i32;            -- reserved
    arg_spill_base   as i32;            -- stack offset where spill slots begin
    aligned_stack_size as i32;          -- total aligned stack frame size in bytes
    func             as *u8;            -- pointer to the IRFunction
}
```

## Building Live Intervals

`build_intervals(func, ctx)` performs two passes over the instruction list.

### Pass 1: Map Labels to Positions

Walk all instructions and record the instruction index (position) of each `IR_LABEL`. Used in
pass 2 to detect backward jumps.

### Pass 2: Scan for Vreg Usage

For each instruction, at position `pos`:

- If `src1` or `src2` is `OP_VREG`, update that vreg's `start` (min) and `end_pos` (max).
  Increment `use_count` and add 10 to `spill_cost`.
- If `dest` is `OP_VREG`, update `start` and `end_pos`. Add 1 to `spill_cost` (definitions are
  cheaper to spill than uses).
- For `IR_JMP`, `IR_JZ`, `IR_JNZ` that jump backward (loop back edges): extend the live
  intervals of all vregs that overlap with the loop range `[target_pos, current_pos]`.

### Spill Cost Heuristic

After the scan, the raw cost is normalized by interval span:

```
spill_cost = spill_cost * 100 / (end_pos - start + 1)
```

Frequently-used vregs with short live ranges become expensive to spill. Infrequently-used vregs
with long live ranges become cheap to spill.

## Linear Scan Allocation

`linear_scan_alloc(func, ctx)` walks the sorted intervals and assigns each one a location.

### Available Registers

Only three callee-saved registers are available for allocation:

| Register | Index |
|----------|-------|
| rbx | 3 |
| r12 | 10 |
| r13 | 11 |

The caller-saved registers (rax, rcx, rdx, rsi, rdi, r8-r11) are not allocated because they
would be clobbered by any `call` instruction. `r14` and `r15` are reserved as scratch registers
by the instruction emitter.

### Register Assignment

For each interval (in start-position order):

1. Call `find_free_reg`: scan rbx, r12, r13 in order and return the first whose live intervals
   do not overlap with the current position.
2. If no register is free, call `spill_register`: find the interval assigned to rbx/r12/r13
   with the latest `end_pos` and spill it (assign a stack slot, clear its `reg` field).
3. If a register was freed by spilling, assign it to the current interval.
4. If spilling also failed, spill the current interval itself.

### Location Encoding

`vreg_to_loc[vreg]` encodes the location as follows:

- `>= 0`: physical register index (3=rbx, 10=r12, 11=r13).
- `< 0`: spill slot. Stack offset is `arg_spill_base + (-loc) * 8`.
- `-1000`: initial sentinel meaning "unallocated" (vreg is dead / never used).

## Accessing Vreg Values in the Emitter

Two helpers abstract the register-vs-memory distinction:

### `load_op(vreg, ctx, target)`

Loads the value of `vreg` into the named register:

```asm
; vreg in rbx:
  mov r15, rbx

; vreg spilled at offset 24:
  mov r15, QWORD PTR [rbp-24]
```

### `store_op(vreg, ctx, source)`

Stores the named register into `vreg`'s location:

```asm
; vreg in r12:
  mov r12, r15

; vreg spilled at offset 32:
  mov QWORD PTR [rbp-32], r15
```

## Limitations

- Only 3 physical registers are available for allocation. Functions with many simultaneously
  live vregs will spill heavily to the stack.
- No register coalescing, copy propagation, or interval splitting.
- Every instruction uses scratch registers (r14/r15) for operand loading, adding `mov`
  instructions even for vregs that are already in registers.
- The spill heuristic does not account for loop nesting depth.
