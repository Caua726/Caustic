# Enable store-load forwarding for i32 variables

## Overview

The `pass_store_load_fwd` in `src/ir/opt_passes.cst` only forwards 8-byte (i64/pointer) stores and loads. i32 variables (4-byte) are skipped. With i32 SSA promotion now active, many i32 STORE→LOAD pairs survive in the IR and create unnecessary vreg pressure. Enabling i32 forwarding eliminates these pairs, reducing vreg count and stack spills.

## The problem

Current code (`src/ir/opt_passes.cst`, around line 507-550):

```cst
// Store tracking (line ~509):
let is i32 as st_sz with mut = 8;
if (cast(i64, inst.cast_to) != 0) {
    let is *i32 as stzp = cast(*i32, cast(i64, inst.cast_to) + 28);
    st_sz = *stzp;
}
if (st_sz == 8) {    // ← ONLY 8-byte stores tracked
    // ... track store ...
}

// Load forwarding (line ~544):
let is i32 as ld_sz with mut = 8;
if (cast(i64, inst.cast_to) != 0) {
    let is *i32 as ldzp = cast(*i32, cast(i64, inst.cast_to) + 28);
    ld_sz = *ldzp;
}
if (ld_sz == 8) {    // ← ONLY 8-byte loads forwarded
    // ... forward to MOV ...
}
```

The comment says "only track 8-byte stores (i32 truncates, unsafe to forward)". But with i32 SSA promotion, the STORE and LOAD of an i32 slot both have `cast_to.size == 4`. Forwarding is safe when BOTH the store and load have the same size.

## The fix

Change the condition to forward when store and load have matching sizes.

### Step 1: Track stores of ANY size (not just 8)

Around line 509, change:
```cst
// BEFORE:
if (st_sz == 8) {

// AFTER:
if (st_sz == 8 || st_sz == 4 || st_sz == 2 || st_sz == 1) {
```

Also store the size alongside the offset and vreg. Currently the forwarding table has:
```cst
let is [64]i64 as slf_offsets;
let is [64]i32 as slf_vregs;
```

Add a size array:
```cst
let is [64]i32 as slf_sizes;   // ← ADD THIS
```

When tracking a store, record the size:
```cst
slf_sizes[slf_count] = st_sz;  // alongside slf_offsets and slf_vregs
```

### Step 2: Forward loads only when sizes match

Around line 544, change:
```cst
// BEFORE:
if (ld_sz == 8) {
    // ... forward ...
}

// AFTER (forward when sizes match):
if (ld_sz >= 1) {
    let is i64 as off = inst.src1.imm;
    let is i32 as si with mut = 0;
    let is i32 as fwd with mut = 0;
    while (si < slf_count) {
        if (slf_offsets[si] == off && slf_sizes[si] == ld_sz) {  // ← SIZE CHECK
            inst.op = d.IR_MOV;
            inst.src1.op_type = d.OP_VREG;
            inst.src1.vreg = slf_vregs[si];
            fwd = 1;
            si = slf_count;
        }
        si = si + 1;
    }
    // ... rest of forwarding logic (first-load tracking) ...
}
```

### Step 3: Partial-width store invalidation (already exists)

The existing code at line ~528-540 invalidates entries when a store of different size hits the same offset. This stays unchanged — it correctly handles the case where an i32 store overwrites part of an i64 slot.

## Impact

Each forwarded i32 STORE→LOAD pair:
- Eliminates 1 LOAD instruction (becomes IR_MOV from the stored vreg)
- The IR_MOV may be further eliminated by copy propagation
- Reduces vreg count → less register pressure → fewer spills
- Each spill saved = 1 load + 1 store per use = ~8 cycles per iteration

For bubble_sort: the inner loop currently has 54 stack loads. Many are i32 loop counters reloaded multiple times. Forwarding could eliminate 10-20 of them.

## Files

| File | Change | Lines |
|------|--------|-------|
| `src/ir/opt_passes.cst` | Add `slf_sizes[64]` array, change size checks, match sizes on forward | ~10 changed |

## Validation

```bash
rm -rf .caustic
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1

# All examples -O0 and -O1
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done

# Bootstrap gen4 -O1
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done

# Bench
/tmp/gen4 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b

# Verify stack load reduction in sort
grep -c "QWORD PTR \[rbp" examples/bench.cst.s
# Should be significantly fewer than current ~54 in bubble_sort
```

## Gotchas

- `rm -rf .caustic` before rebuild
- `with mut` for mutable vars
- The `slf_sizes` array must be initialized alongside `slf_offsets` and `slf_vregs`
- When invalidating on partial-width store (line ~528), also remove entries with different sizes at the same offset
- Commits: lowercase, sem Co-Authored-By
