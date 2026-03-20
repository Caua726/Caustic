# Typed Phi Destruction — Enable CCP MOV tracking for i32 SSA

## Overview

Re-enable `const_copy_prop` MOV copy tracking (currently disabled for i32 SSA safety). Expected improvement: **907ms → ~775ms** (-15%).

## Current state

The `pass_const_copy_prop` has MOV copy tracking disabled because `destroy_ssa` in `src/ir/ssa.cst:513` emits plain `IR_MOV` (no type info) for phi destruction. When CCP propagates these copies, i32 vregs get substituted where i64 pointers are expected, corrupting struct fields.

Example of the corruption:
```
// mem2reg promotes i32 'i' → phi vreg v5 (size=4)
// destroy_ssa emits: IR_MOV v5 = v3  (plain MOV, no cast_to)
// CCP chains: copy_of[v5] = v3, then copy_of[v7] = v5 → resolves to v3
// v7 was used as a pointer → codegen emits mov DWORD PTR instead of QWORD PTR
// GDB: ctx->intervals = 0x0000000b0000000a (two i32s packed as pointer)
```

## Root cause

`destroy_ssa` (`src/ir/ssa.cst:487-546`) creates IR_MOV for each phi arg→dest:
```cst
let is *d.IRInst as mov = d.new_inst(d.IR_MOV);  // line 513
mov.dest.op_type = d.OP_VREG;
mov.dest.vreg = phi_dst;
mov.src1.op_type = d.OP_VREG;
mov.src1.vreg = arg_vreg;
// NO cast_to, NO cast_from → CCP treats as pure copy
```

When the phi is for an i32 slot, this MOV should be annotated with the slot's type so CCP knows it's a size conversion, not a pure copy.

## The fix

### Step 1: Pass slot type info to destroy_ssa

The `SlotInfo` struct (ssa.cst:13-21) already has `type_ptr` (the Type* of the slot). The phi arrays have `phi_slot[i]` (slot index). So `destroy_ssa` can look up the type.

Add parameter `slots as *SlotInfo` to `destroy_ssa`:
```cst
fn destroy_ssa(blocks, nblocks, func,
               phi_block, phi_slot, phi_vreg, phi_args, max_preds, nphi,
               preds, pred_starts, pred_counts,
               slots, nslots) as void {   // ← ADD these
```

### Step 2: Annotate phi MOVs with type

In `destroy_ssa`, when creating the IR_MOV (line 513), look up the slot type and set `cast_to` if size < 8:

```cst
let is *d.IRInst as mov = d.new_inst(d.IR_MOV);
mov.dest.op_type = d.OP_VREG;
mov.dest.vreg = phi_dst;
mov.src1.op_type = d.OP_VREG;
mov.src1.vreg = arg_vreg;

// Annotate with slot type for i32 safety
let is *i32 as pslp = cast(*i32, cast(i64, phi_slot) + cast(i64, pi) * 4);
let is i32 as slot_idx = *pslp;
let is *SlotInfo as slot = cast(*SlotInfo, cast(i64, slots) + cast(i64, slot_idx) * sizeof(SlotInfo));
if (cast(i64, slot.type_ptr) != 0) {
    let is *i32 as szp = cast(*i32, cast(i64, slot.type_ptr) + 28);
    if (*szp < 8) {
        mov.cast_to = slot.type_ptr;
        mov.cast_from = slot.type_ptr;
    }
}
```

### Step 3: Update mem2reg call to pass slots

In `mem2reg` function (ssa.cst), the call to `destroy_ssa` (around line 638) needs the `slots` and `nslots` parameters added.

### Step 4: Re-enable CCP MOV tracking with cast_to guard

In `pass_const_copy_prop` (`opt_passes.cst`), re-enable the IR_MOV tracking block but with a guard:

```cst
if (op == d.IR_MOV && inst.src1.op_type == d.OP_VREG) {
    let is i32 as dv = inst.dest.vreg;
    let is i32 as sv = inst.src1.vreg;

    // Guard: if MOV has cast_to with size < 8, it's a type conversion
    // from phi destruction of an i32 slot — don't propagate as copy
    let is i32 as is_size_conv with mut = 0;
    if (cast(i64, inst.cast_to) != 0) {
        let is *i32 as szp = cast(*i32, cast(i64, inst.cast_to) + 28);
        if (*szp < 8) { is_size_conv = 1; }
    }

    if (is_size_conv == 0 && mov_single == 1) {
        let is *i32 as pp = cast(*i32, cast(i64, copy_of) + cast(i64, dv) * 4);
        *pp = sv;
        inst.is_dead = 1;
        // ... const propagation ...
    }
}
```

### Step 5: Remove vreg_size workaround

With typed phi MOVs, the `vreg_size` array in `opt.cst` and the `resolve_sized` function in `opt_passes.cst` become unnecessary. The guard is now on the instruction itself (`cast_to`), not on a separate tracking array.

Remove:
- `vreg_size` allocation and scan in `opt.cst`
- `resolve_sized()` in `opt_passes.cst`
- `vreg_size` parameter from `pass_final_resolve`

The regular `resolve()` function works because it chases `copy_of` chains, and the i32 phi MOVs no longer set `copy_of`.

## Files to change

| File | Change | Lines |
|------|--------|-------|
| `src/ir/ssa.cst` | Add `slots, nslots` params to `destroy_ssa`, annotate phi MOVs with `cast_to` | ~10 |
| `src/ir/ssa.cst` | Update `mem2reg` call to `destroy_ssa` with extra params | ~2 |
| `src/ir/opt_passes.cst` | Re-enable CCP MOV tracking with `cast_to` size guard | ~10 |
| `src/ir/opt_passes.cst` | Remove `resolve_sized`, revert to `resolve` | ~-20 |
| `src/ir/opt.cst` | Remove `vreg_size` array, simplify `pass_final_resolve` call | ~-15 |

Net: ~-15 lines (simpler code + better performance).

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

# Bootstrap gen4-O1
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done
# gen2=gen3=gen4

# Bench target: < 800ms
/tmp/gen4 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b
```

## Gotchas

- `rm -rf .caustic` always before rebuild
- `sizeof(SlotInfo)` for SlotInfo array stride
- `with mut` for mutable vars
- `0 - 1` for -1
- The `SlotInfo` struct fields: offset(i64), type_ptr(*u8), promotable(i32), store_count(i32), load_count(i32)
- Offset of `size` in Type struct: 28 bytes (`cast(*i32, cast(i64, type_ptr) + 28)`)
