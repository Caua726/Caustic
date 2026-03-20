# Strength Reduction v2: MUL by variable + full array indexing

## Overview

Rewrite `pass_strength_reduction` in `src/ir/opt_passes.cst` to detect `MUL v_iv, v_invariant` inside loops (in addition to existing SHL) and convert array indexing expressions (`base + iv * stride`) into pointer increment (`ptr += stride`). Supports both constant and variable strides, both stack-based and SSA induction variables.

**Target:** matmul from ~169ms to ~80-100ms. Overall bench from ~780ms to ~700ms.

**File:** `src/ir/opt_passes.cst` — rewrite `pass_strength_reduction` (~250-300 lines, currently ~130).

**Runs after:** LICM (which hoists invariants). **Runs before:** DCE (which cleans up dead instructions).

---

## Induction Variable Detection

An induction variable (IV) is a vreg that increments by a constant step each loop iteration.

### Stack-based pattern
```
LOAD v_i = [rbp-X]           ; load from stack slot
... loop body uses v_i ...
ADD v_next = v_i, step        ; increment (step is OP_IMM)
STORE [rbp-X] = v_next        ; store back to same slot
JMP .L_header
```

Detection: scan loop body for `STORE dest=OP_IMM`. Check if `STORE.src1` comes from an `ADD` whose `src1` comes from a `LOAD` of the same stack offset. Both `LOAD` and `STORE` must be inside the loop.

Result:
```
InductionVar:
  vreg = v_i (the LOAD dest)
  step_imm = step value from ADD
  step_vreg = -1 (constant step)
  is_stack = 1
  stack_offset = X
  add_inst = pointer to the ADD instruction
```

### SSA pattern
After mem2reg promotes a loop counter, phi destruction + SSA rename creates:
```
IR_MOV v_phi = v_init         ; phi entry copy (before header)
.L_header:
IR_CAST v_i = v_phi           ; sign-extension if i32 (cast_to.size < 8)
                               ; OR: IR_MOV v_i = v_phi (if i64, no cast needed)
... loop body uses v_i ...
ADD v_next = v_i, step         ; increment
IR_MOV v_phi = v_next          ; phi back-edge copy (before JMP)
JMP .L_header
```

**CRITICAL:** The phi vreg (`v_phi`) has def_count > 1. But the ADD's operand is `v_i` (the CAST/MOV result), which has def_count == 1. The detection must follow the chain BACKWARD from the ADD through the CAST/MOV to find the phi vreg.

Detection algorithm:
1. Find `ADD v_next = v_i, step` where `step` is OP_IMM
2. Look at how `v_i` is defined. If by IR_CAST or IR_MOV from `v_phi`:
3. Check if `v_phi` has def_count > 1 (confirms it is a phi vreg)
4. Check that `IR_MOV v_phi = v_next` exists in the loop (the back-edge copy)
5. If all checks pass: `v_i` is the IV body vreg, `v_phi` is the phi vreg

Result:
```
InductionVar:
  vreg = v_i (the CAST/MOV result used in the body)
  phi_vreg = v_phi (the multi-def phi vreg)
  step_imm = step
  step_vreg = -1
  is_stack = 0
  add_inst = pointer to the ADD
```

### Variable step (rare, but supported)
```
ADD v_next = v_i, v_step      ; step is a vreg, not immediate
```
If `v_step` is loop-invariant (defined outside the loop), this is still an IV with variable step.

Result:
```
InductionVar:
  step_imm = 0
  step_vreg = v_step (the loop-invariant vreg)
```

---

## Loop-Invariant Detection

A vreg is loop-invariant if its (single) definition is outside the loop range [header_pos, backedge_pos]. Reuse the same `def_pos[]` array from the existing pass.

```
fn is_loop_invariant(vreg, def_pos, def_count, header, backedge):
    if def_count[vreg] != 1: return 0   ; multi-def = phi = NOT invariant
    if def_pos[vreg] < header: return 1  ; defined before loop
    if def_pos[vreg] > backedge: return 1 ; defined after loop (unreachable, but safe)
    return 0
```

---

## Derived Expression Patterns

After finding IVs, scan the loop body for expressions that derive from them. Match from most complex to simplest:

### Pattern 5: Full array index — `base + (iv * invariant + iv2) * const`
Matmul pattern: `a[i*n+k]` → `base + ((i*n + k) << 3)`
```
MUL v1 = v_iv_outer, v_n      ; i*n (outer IV * invariant)
ADD v2 = v1, v_iv_inner        ; i*n + k (+ inner IV)
SHL v3 = v2, 3                 ; (i*n+k) * 8
ADD v4 = v_base, v3            ; &a[i*n+k]
LOAD/STORE [v4]
```
This decomposes into: inner loop (k) → ptr += 8, outer loop (i) → ptr += n*8.
Only transform the **innermost** loop's IV usage. The outer loop transformation happens when pass_strength_reduction processes the outer loop.

### Pattern 4: Base + MUL — `base + iv * invariant`
```
MUL v1 = v_iv, v_stride        ; iv * stride (variable stride)
ADD v2 = v_base, v1             ; base + iv*stride
LOAD/STORE [v2]
```
Acumulador: `ptr += step * v_stride` per iteration.

### Pattern 3: Base + SHL — `base + iv << const`
```
SHL v1 = v_iv, const_shift     ; iv * (1 << shift)
ADD v2 = v_base, v1             ; base + iv*const
LOAD/STORE [v2]
```
Acumulador: `ptr += step << const_shift` per iteration. (Already partially handled by existing pass.)

### Pattern 2: MUL by invariant — `iv * invariant`
```
MUL v1 = v_iv, v_invariant
; v1 used directly (not as array index)
```
Acumulador: `acc += step * v_invariant` per iteration.

### Pattern 1: SHL by constant — `iv << const`
```
SHL v1 = v_iv, const_shift
```
Acumulador: `acc += step << const_shift` per iteration. (Existing pass.)

### IR_CAST in chains (i32 variables)

**IMPORTANT:** When variables are i32 (promoted by mem2reg), the IR inserts IR_CAST (sign-extension) between operations. The actual chain for `(i*n+k)*8` with i32 variables is:

```
CAST v_i_64 = v_i_phi          ; i32 → i64 sign-extension
CAST v_n_64 = v_n_phi          ; n i32 → i64
MUL v1 = v_i_64, v_n_64        ; i*n (in i64)
CAST v_k_64 = v_k_phi          ; k i32 → i64
ADD v2 = v1, v_k_64            ; i*n+k
SHL v3 = v2, 3                 ; (i*n+k)*8
ADD v4 = v_base, v3            ; base + offset
```

The chain-follower must **traverse through IR_CAST nodes** — treat them as transparent passthrough when following operand chains. When the chain-follower sees a vreg, check if it was defined by IR_CAST, and if so, follow through to the CAST's source.

### Match priority
Try Pattern 5 first, then 4, 3, 2, 1. When a complex pattern matches, it subsumes the simpler ones (don't also match the inner MUL/SHL separately).

### Safety: single-use check
Only transform if the intermediate vregs (v1, v2, v3 in the chain) each have **exactly one use** across the entire function (not just the loop body). If any intermediate is used elsewhere, the transformation would break those uses. Build a function-wide use-count array in Phase 0.

---

## Transformation

For each matched derived expression in the innermost loop:

### Step 1: Create accumulator vreg
```
let acc_vreg = func.vreg_count;
func.vreg_count += 1;
```

### Step 2: Compute initial value before loop header
Insert instructions before the header label that compute `acc_init = base + iv_init * stride`:

For Pattern 3 (`base + iv << 3` with iv starting at 0):
```
; If iv starts at 0, acc_init = base + 0 = base
IR_MOV acc = base               ; insert before header
```

For Pattern 4 (`base + iv * n` with iv starting at 0):
```
IR_MOV acc = base               ; insert before header (0 * n = 0)
```

For non-zero initial IV value:
```
IR_MUL tmp = iv_init, stride    ; compute initial offset
IR_ADD acc = base, tmp           ; acc = base + initial offset
; insert both before header
```

For stack-based IV, `iv_init` is obtained by inserting a `LOAD [rbp-X]` before the header.
For SSA IV, `iv_init` is the phi entry value (the IR_MOV src before the header).

### Step 3: Replace derived expression with accumulator
In the loop body, replace the final instruction of the chain (the ADD that produces the address) with:
```
IR_MOV v_addr = acc             ; use accumulator instead of computing address
```
Mark all intermediate instructions (MUL, SHL, inner ADDs) as dead.

### Step 4: Insert increment after IV increment
Right after the `ADD v_next = v_iv, step` (the IV increment), insert:

For constant stride:
```
IR_ADD acc = acc, stride_bytes   ; e.g., acc += 8
```

For variable stride:
```
IR_MUL tmp = step, v_stride      ; step * stride
IR_ADD acc = acc, tmp             ; acc += step * stride
```

If step == 1 and stride is a power of 2:
```
IR_ADD acc = acc, (1 << shift)   ; acc += 8 (simplified)
```

If step == 1 and stride is variable:
```
IR_ADD acc = acc, v_stride       ; acc += n (when stride = n, step = 1)
```

For the matmul case `b[k*n+j]` in the j-loop (step=1, stride=8):
```
IR_ADD acc = acc, 8              ; ptr_b += 8 each j iteration
```

### Step 5: Handle multiple derived expressions per loop
The matmul inner loop has 3 array accesses: `a[i*n+k]`, `b[k*n+j]`, `c[i*n+j]`. Each gets its own accumulator. The pass creates 3 accumulators, 3 initializations, 3 increments.

Process them independently — each has its own IV and stride. The accumulators don't interfere with each other.

---

## Concrete Example: matmul inner loop (j)

### Before (j-loop inner body)
```
.L_inner:                         ; j loop
  ; a[i*n+k] — FULLY INVARIANT in j loop (i,k,n,a don't change)
  ; This is NOT a strength reduction target — LICM should hoist it.
  ; If LICM doesn't hoist (body-only, not header), it stays but is not transformed.
  MUL v10 = v_i, v_n
  ADD v11 = v10, v_k
  SHL v12 = v11, 3
  ADD v13 = v_a, v12
  LOAD v14 = [v13]                ; a[i*n+k] — no IV involved, skip

  ; b[k*n+j] — j is IV, this IS a strength reduction target
  MUL v20 = v_k, v_n              ; k*n (invariant)
  ADD v21 = v20, v_j              ; k*n + j (IV)
  SHL v22 = v21, 3                ; (k*n+j)*8
  ADD v23 = v_b, v22              ; &b[k*n+j]
  LOAD v24 = [v23]

  ; c[i*n+j] — j is IV, this IS a strength reduction target
  MUL v30 = v_i, v_n              ; i*n (invariant)
  ADD v31 = v30, v_j              ; i*n + j (IV)
  SHL v32 = v31, 3                ; (i*n+j)*8
  ADD v33 = v_c, v32              ; &c[i*n+j]
  LOAD v34 = [v33]

  MUL v40 = v14, v24
  ADD v41 = v34, v40
  STORE [v33] = v41

  ADD v_j_next = v_j, 1           ; j++
  JMP .L_inner
```
**~20 instructions per iteration**. Two chains (b[] and c[]) are strength-reducible. a[] is invariant (no IV).

### After
```
  ; Before j-loop (initialization, j starts at 0):
  ; a[i*n+k] — invariant, stays as-is (or LICM hoists)

  ; ptr_b = &b[k*n + 0] (init for j=0)
  MUL t4 = v_k, v_n
  SHL t5 = t4, 3
  ADD v_ptr_b = v_b, t5

  ; ptr_c = &c[i*n + 0] (init for j=0)
  MUL t6 = v_i, v_n
  SHL t7 = t6, 3
  ADD v_ptr_c = v_c, t7

.L_inner:
  ; a[i*n+k] — still computed each iteration (invariant, ideally hoisted)
  MUL v10 = v_i, v_n
  ADD v11 = v10, v_k
  SHL v12 = v11, 3
  ADD v13 = v_a, v12
  LOAD v14 = [v13]

  LOAD v24 = [v_ptr_b]            ; *ptr_b (was 4 instructions)
  LOAD v34 = [v_ptr_c]            ; *ptr_c (was 4 instructions)

  MUL v40 = v14, v24
  ADD v41 = v34, v40
  STORE [v_ptr_c] = v41

  ADD v_ptr_b = v_ptr_b, 8        ; ptr_b += 8
  ADD v_ptr_c = v_ptr_c, 8        ; ptr_c += 8
  ADD v_j_next = v_j, 1
  JMP .L_inner
```
**~14 instructions per iteration** (was ~20). 8 MUL/ADD/SHL eliminated per iteration from the two b[] and c[] chains.

8 instructions eliminated × 64 iterations × 64×64 outer × 100 reps = **~20M instructions eliminated**.

---

## Implementation Structure

```
fn pass_strength_reduction(func as *d.IRFunction) as void {
    // Phase 0: Build def_pos, def_count, use_count arrays
    //   def_pos[v] = instruction position of v's definition
    //   def_count[v] = number of definitions (>1 = phi)
    //   use_count[v] = number of uses as src1/src2

    // Phase 1: Find loops (back-edges)
    //   Same as current: JMP/JZ/JNZ to earlier label
    //   For each loop [header_pos, backedge_pos]:

    // Phase 2: Find induction variables
    //   Scan loop body for stack-based and SSA IV patterns
    //   Store in flat array: iv_vreg[], iv_step_imm[], iv_step_vreg[],
    //                        iv_is_stack[], iv_offset[], iv_add_inst[]

    // Phase 3: Find derived expressions
    //   For each MUL/SHL in loop body whose operand is an IV:
    //     Follow the chain: MUL → ADD → SHL → ADD → LOAD/STORE
    //     Check single-use on intermediates
    //     Check other operand is loop-invariant
    //     Record: chain_start, chain_end, iv_index, stride info

    // Phase 4: Transform
    //   For each matched chain:
    //     Create accumulator vreg
    //     Insert init before header
    //     Replace chain with MOV from accumulator
    //     Insert ADD accumulator after IV increment
    //     Mark consumed instructions as dead
}
```

---

## Edge Cases

### IV starts at non-zero value
If the loop counter starts at a value other than 0, the accumulator initialization must compute `base + init * stride`. For stack-based IVs, insert a LOAD before the header to get the initial value. For SSA IVs, use the phi entry MOV source.

### Multiple IVs in same loop
The matmul inner loop has one IV (j). But the middle loop (k) is also an IV. When processing the inner loop, k is invariant. When processing the middle loop separately, k is the IV and j doesn't exist (it's the inner loop's scope).

The pass processes **one loop at a time**, innermost first. Each loop sees its own IVs and treats outer loop variables as invariants.

### Nested derived expressions
`(i*n + k) << 3` is a chain of MUL → ADD → SHL. The pass must follow the full chain, not stop at the first MUL. Use a recursive or iterative chain-following approach.

### IV used outside the derived expression
If `v_j` is used both in `b[k*n+j]` (array index) and in `if (j == 0)` (comparison), the strength reduction only transforms the array index usage. The IV itself (`v_j`) is NOT replaced — only the derived chain is.

### Loop with no matching patterns
If no MUL/SHL uses an IV, the pass does nothing (same as current behavior for loops without SHL).

### Loops with function calls
The pass **skips loops that contain IR_CALL or IR_SYSCALL** (same restriction as current implementation). This is safe — function calls may modify memory pointed to by the base pointers, invalidating the accumulator. The matmul inner loop has no calls, so this doesn't affect the target.

### IV init == 0 fast path
When the IV starts at 0 (the common case for `for i=0..n`), the initialization simplifies to:
```
; Instead of: MUL tmp = 0, stride; ADD acc = base, tmp
; Just: MOV acc = base  (since 0 * stride = 0)
```
The pass should detect `iv_init == 0` (either IMM 0 or LOAD of a slot that was just stored with 0) and emit the simplified init.

### Accumulator as multi-def vreg
The accumulator `acc` is defined twice: once before the loop (init) and once inside (increment). This means `def_count[acc] > 1`. Subsequent passes (const_copy_prop, fold_immediates) must NOT treat it as constant. The `ccp_defs` guard already handles this. **Note:** strength_reduction runs AFTER CCP and fold_imm, so accumulators are only seen by DCE (which has its own multi-def guard).

### Position stability after insertions
Inserting instructions before a loop header shifts positions of all subsequent instructions. To avoid stale position data when processing multiple loops, **process loops from bottom to top** (highest backedge position first). This ensures insertions for one loop don't affect position data of unprocessed loops above it.

### Outer loop initializations (known limitation)
After strength-reducing the j-loop, the accumulator init code (e.g., `MUL k, n; SHL; ADD base`) is inserted before the j-loop header, inside the k-loop. These expressions could themselves be strength-reduced in the k-loop. However, this pass runs **once** (single pass over back-edges), so outer-loop initializations are NOT strength-reduced. This is acceptable — the inner loop is where 95%+ of instructions execute. A future multi-pass version could address this.

---

## Files

| File | Change |
|------|--------|
| `src/ir/opt_passes.cst` | Rewrite `pass_strength_reduction` (~250-300 lines, currently ~130) |

No other files need changes. The pass interface (`fn pass_strength_reduction(func as *d.IRFunction) as void`) stays the same.

---

## Development Process

**IMPORTANT:** These changes are for `-O1` only. `-O0` uses linear scan and no optimization passes — it must remain untouched and working.

### Step-by-step workflow

The implementation MUST follow this process for EACH step/phase:

1. **Implement** the step (Phase 0, Phase 1, etc.)
2. **Run code reviewer** on the changes before proceeding
3. **Fix** any issues the reviewer finds
4. **Run full validation** (see below)
5. **Only proceed** to the next step after validation passes

### Full validation (run after EVERY step)

```bash
rm -rf .caustic

# Build gen1
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1

# Examples -O0 (must all pass — strength reduction only runs with -O1)
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O0: $?"
done

# Examples -O1 (must all pass, test_arr must exit 100)
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O1: $?"
done

# Matmul correctness: bench must output matmul(64x64) = 95692
/tmp/gen1 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b | grep matmul
# Must show: matmul(64x64) = 95692

# CRITICAL: Bootstrap gen4 -O1 (gen2=gen3=gen4, same size)
# This MUST pass after every step. If it fails, the step has a bug.
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done
# gen2, gen3, gen4 MUST have the same byte size. If not, there is a bug.

# Bench -O1 (target: matmul < 100ms, total < 700ms)
taskset -c 11 /tmp/b
```

### If bootstrap fails

If gen2 != gen3 or gen3 != gen4, or any gen crashes:
1. **DO NOT proceed** to the next step
2. **Bisect** which part of the step broke it
3. **Fix** the bug
4. **Re-run** full validation
5. **Only proceed** after gen2=gen3=gen4 passes

## Caustic Gotchas

- `with mut` for mutable variables
- `rm -rf .caustic` before rebuild (cache goes stale)
- `fn` and `gen` are keywords
- `cast(i64, x)` for i32→i64
- `0 - 1` instead of `-1`
- `cast(i32, 0 - 1)` for -1 as i32
- Array access: `cast(*i32, cast(i64, arr) + cast(i64, idx) * 4)`
- New vregs: `func.vreg_count = func.vreg_count + 1`
- Insert instruction before header: find `hprev` (instruction before header label), set `new_inst.next = header_label; hprev.next = new_inst`
- Insert after ADD: `new_inst.next = add_inst.next; add_inst.next = new_inst`
- Mark dead: `inst.is_dead = 1`
- Commits: lowercase, sem Co-Authored-By
