# Optimization Pipeline (-O1)

The Caustic compiler includes a multi-pass optimization pipeline activated by the `-O1` flag. Optimizations operate on the virtual-register IR before register allocation and assembly emission. The pipeline runs in three phases:

1. **Per-function optimization** -- all 8 IR passes on each reachable function
2. **Inlining** -- a separate pass over all functions (callees are already optimized, so stack-op detection is accurate post-mem2reg)
3. **Cleanup** -- re-runs all 8 IR passes on functions that received inlined code

Source files:

- `src/ir/opt.cst` -- pipeline orchestration
- `src/ir/opt_prop.cst` -- addr_fusion, const/copy propagation, store-load forwarding
- `src/ir/opt_helpers.cst` -- fold_imm, DCE, final_resolve
- `src/ir/opt_loop.cst` -- LICM, inlining
- `src/ir/opt_sr.cst` -- strength reduction
- `src/ir/ssa.cst` -- SSA / mem2reg
- `src/ir/cfg.cst` -- CFG construction
- `src/ir/dom.cst` -- dominator tree, dominance frontiers
- `src/codegen/peephole.cst` -- post-emission peephole optimizer

## Pass overview

| # | Pass | Source | Description |
|---|------|--------|-------------|
| 1 | addr_fusion | opt_prop.cst | Address expression simplification |
| 2 | mem2reg (SSA) | ssa.cst, cfg.cst, dom.cst | Promote stack variables to virtual registers |
| 3 | const_copy_prop | opt_prop.cst | Constant propagation + copy elimination |
| 4 | store_load_fwd | opt_prop.cst | Eliminate redundant loads |
| 5 | fold_imm | opt_helpers.cst | Fold constants into instruction operands |
| 6 | LICM | opt_loop.cst | Loop-invariant code motion |
| 7 | strength_reduction | opt_sr.cst | Algebraic simplification of loop operations |
| 8 | DCE | opt_helpers.cst | Dead code elimination |
| 9 | inlining | opt_loop.cst | Inline small non-recursive functions |
| 10 | peephole | peephole.cst | Post-emission assembly patterns |

The pipeline also includes a `pass_final_resolve` step after DCE that resolves remaining copy chains in all operands.

## Pass details

### 1. addr_fusion

Simplifies address computation expressions. Counts uses of each vreg and tracks which vregs originate from `IR_ADDR` with an immediate (local variable address). When an address vreg is only used once in a load or store, the pass fuses the address computation directly into the memory operation, eliminating the intermediate vreg.

### 2. mem2reg (SSA)

Promotes scalar stack variables to SSA virtual registers using phi nodes. This is the most impactful optimization -- it eliminates the majority of stack loads and stores for local variables.

**Algorithm:**

1. **Build CFG** (`cfg.cst`): Identifies basic blocks by scanning for leaders (labels, instructions after branches). Sets successors from terminators (JMP, JZ, JNZ, RET). Maximum 4096 blocks per function.

2. **Build predecessors**: Counts and records predecessor blocks for each basic block.

3. **Compute reverse postorder**: Iterative DFS over the CFG to establish RPO numbering.

4. **Compute dominators** (`dom.cst`): Cooper-Harvey-Kennedy algorithm using RPO to build the immediate dominator tree, then computes dominance frontiers.

5. **Find promotable slots**: Scans for `IR_LOAD`/`IR_STORE` with immediate offsets (stack slots). Slots that have their address taken (`IR_ADDR`) are marked non-promotable. Maximum 256 slots tracked. Uses an O(1) offset-to-slot lookup table (direct-mapped, max offset 8192).

6. **Insert phi nodes**: For each promotable slot, collects definition blocks (blocks containing stores to that offset) and inserts phi nodes at dominance frontier blocks using a worklist algorithm.

7. **SSA rename**: Iterative DFS over the dominator tree. Maintains a definition stack per slot. Replaces `IR_LOAD` from promotable offsets with `IR_MOV` from the current reaching definition. Replaces `IR_STORE` to promotable offsets with `IR_MOV` to a new vreg and pushes it on the definition stack. Fills phi arguments from predecessor definitions.

8. **SSA destruction**: Converts phi nodes back to conventional form by inserting `IR_MOV` instructions at the end of predecessor blocks.

### 3. const_copy_prop

Forward pass combining constant propagation and copy elimination.

- **Copy chains**: Resolves transitive copy chains (up to 16 hops) in all operands, including store/copy destinations and branch conditions.
- **Constant tracking**: When `IR_IMM` defines a vreg with exactly one definition (single-def guard), records the vreg as a constant with its immediate value.
- **Constant propagation through MOVs**: If a MOV source is a known constant, propagates that to the destination (single-def guard applies).
- **Identity elimination**: For binary ops (`ADD`, `SUB`, `MUL`, `AND`, `OR`, `XOR`, `SHL`, `SHR`), when both operands are known constants, folds the result into an `IR_IMM`.

The **single-def guard** is critical for correctness in loops: a vreg redefined in a loop body would have multiple definitions, and treating the first definition's value as a constant would be unsound.

### 4. store_load_fwd

Eliminates redundant loads from stack slots when the stored value is still available in a vreg.

Maintains a table of up to 64 recently-stored {offset, vreg, size} triples. When a load from a known offset matches a tracked store (same offset and size), the load is replaced with a MOV from the stored vreg.

**Invalidation:** The entire table is cleared at:
- Labels and branches (JMP, JZ, JNZ) -- control flow merge points
- Calls and syscalls -- may modify memory
- Indirect stores (store through vreg pointer) -- may alias any slot
- Memory copies (`IR_COPY`) -- may overwrite stack slots

### 5. fold_imm

Folds constant values into instruction operands. When a vreg has exactly one definition that is `IR_IMM`, and it appears as `src2` of a binary operation (`ADD`, `SUB`, `MUL`, `AND`, `OR`, `XOR`, `SHL`, `SHR`) or comparison (`EQ`, `NE`, `LT`, `LE`, `GT`, `GE`), the vreg reference is replaced with the immediate value directly in the operand.

This reduces register pressure and enables more efficient instruction selection in codegen (e.g., `add rax, 42` instead of loading 42 into a register first).

### 6. LICM (Loop-Invariant Code Motion)

Hoists loop-invariant computations out of loops. Conservative: only processes loops that contain **no function calls** (calls may have side effects or modify global state).

**Algorithm:**

1. Builds label-position and per-vreg definition-position/count maps.
2. Detects loops by finding backward jumps (branch target position < branch position).
3. Scans the loop header block for invariant instructions: `IR_IMM`, `IR_ADD`, `IR_SUB`, `IR_MUL`, `IR_SHL`, `IR_SHR`, `IR_AND`, `IR_OR`, `IR_XOR`.
4. An instruction is invariant if all its vreg operands are defined outside the loop.

**Multi-def guard:** Vregs with multiple definitions (e.g., phi vregs from SSA) are not considered loop-invariant, preventing incorrect hoisting of values that change across iterations.

Invariant instructions are unlinked from the loop body and reinserted before the loop header.

### 7. strength_reduction

Converts expensive loop operations into cheaper equivalents:

- **Division/modulo by power-of-2**: `v / 2^n` becomes `v >> n`, `v % 2^n` becomes `v & (2^n - 1)`.
- **Multiplication to pointer increment**: Inside loops, `iv * stride` patterns (where `iv` is an induction variable) are converted to incremental additions. The pass also handles `base + iv * stride` chains and `SHL`-based multiplications.

The pass detects induction variables by finding loop-internal SSA definitions, checks for loop-invariant strides, and recursively hoists invariant sub-expressions before the loop header when needed.

### 8. DCE (Dead Code Elimination)

Marks instructions as dead when their result vreg has zero uses and they have no side effects.

**Side-effect instructions** (never eliminated): `IR_STORE`, `IR_COPY`, `IR_CALL`, `IR_CALL_INDIRECT`, `IR_SYSCALL`, `IR_ASM`, `IR_RET`, `IR_JMP`, `IR_JZ`, `IR_JNZ`, `IR_LABEL`, `IR_SET_ARG`, `IR_SET_SYS_ARG`, `IR_SET_CTX`, `IR_VA_START`.

The pass builds a reverse instruction array and scans backward, counting uses of each vreg. Instructions defining unused vregs are marked dead. Runs iteratively until no more instructions are removed.

**Multi-def guard for phi back-edges**: Vregs with multiple definitions (from phi nodes) are treated carefully to avoid removing a definition that feeds a phi argument in a loop back-edge.

### 9. Inlining

Runs as a separate phase after all per-function optimizations, so `inline_has_stack_ops()` accurately reflects the post-mem2reg state of each callee.

**Eligibility criteria:**
- Callee has <= 60 live instructions
- Callee is not recursive (no self-calls)
- Callee is not variadic
- Callee has no stack allocation (`alloc_stack_size == 0`)
- Callee has no remaining stack operations (loads/stores to immediate offsets, address-of-local)
- Callee has <= 6 arguments
- Combined vreg count (caller + callee) <= 512
- Not the same function as the caller

**Per-callee limit:** Each distinct callee can be inlined at most 4 times per caller (8 times for callees with <= 15 instructions). This prevents code size explosion from hot callees.

**Process:**
1. Collects `IR_SET_ARG` instructions feeding the call to map argument vregs
2. Clones all callee instructions with vreg and label offsets
3. Replaces `IR_RET` in the clone with a jump to an after-label
4. Splices the cloned instruction sequence in place of the call
5. After inlining, the cleanup phase re-runs all IR optimizations on the caller

### 10. Peephole (post-emission)

Operates on the generated `.s` assembly text, not the IR. Runs up to 4 iterations until convergence. Three patterns:

**Pattern 1 -- MOV + movsxd fusion:**
```asm
mov rA, rB        ;  \
movsxd rA, eA     ;  / fused into: movsxd rA, eBd
```
Eliminates a redundant MOV when the destination is immediately sign-extended.

**Pattern 2 -- MOV chain bypass:**
```asm
mov rA, rB        ;  kept
mov rC, rA        ;  rewritten to: mov rC, rB
```
When rC == rB, the second MOV is deleted entirely (self-move).

**Pattern 3 -- Dead MOV elimination:**
```asm
mov rA, X         ;  deleted (rA overwritten before use)
mov rA, Y         ;  kept (only if Y does not reference rA)
```
Removes a MOV whose destination is immediately overwritten by the next instruction.

The peephole uses a pre-filter on line content: only lines starting with `m` (for `mov`/`movsxd`) are considered, avoiding unnecessary parsing of other instructions.

## Register cache

The code generator includes a 16-entry register cache (`src/codegen/emit_output.cst`) that tracks which physical register currently holds the value of a spilled virtual register.

**Structure:** An array `rc_vreg[16]` where `rc_vreg[reg_idx]` stores the vreg number cached in that physical register (99999 = empty).

**Operations:**
- `rc_init()` -- clears all entries (called at function start)
- `rc_set(idx, vreg)` -- records that register `idx` holds spilled vreg
- `rc_find(vreg)` -- returns which register holds a vreg, or -1
- `rc_invalidate_reg(idx)` -- clears entry when a register is overwritten
- `rc_invalidate_vreg(vreg)` -- clears all entries for a vreg when it is redefined

When loading a spilled vreg (`load_op`), the cache is checked first. If the value is already in a register:
- If the target register matches, the load is skipped entirely
- If it is in a different register, a register-to-register MOV replaces the memory load

The cache is fully invalidated at control flow boundaries (labels, branches, calls). This targeted invalidation strategy avoids unnecessary memory loads, resulting in approximately 21% fewer spill-reload instructions.

## Profiling

The optimizer includes built-in per-pass profiling. When enabled (`opt_prof_init()`), each pass is timed using `clock_gettime(CLOCK_MONOTONIC)` and results are printed as a table with milliseconds, percentages, and a bar chart. Pass indices:

| Index | Pass |
|-------|------|
| 0 | addr_fusion |
| 1 | mem2reg |
| 2 | const_copy_prop |
| 3 | store_load_fwd |
| 4 | fold_imm |
| 5 | licm |
| 6 | strength_red |
| 7 | dce |
| 8 | final_resolve |
| 9 | inline |
