# Strength Reduction v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite `pass_strength_reduction` to convert `MUL iv, invariant` and array indexing chains (`base + iv*stride*8`) into pointer increments, reducing matmul from ~169ms to ~80-100ms.

**Architecture:** Single function rewrite in `src/ir/opt_passes.cst`. Four phases: build analysis arrays → detect induction variables → match derived expression chains → transform IR. Processes loops bottom-to-top (innermost first). Only active under -O1.

**Tech Stack:** Caustic self-hosted compiler (.cst files), x86_64 IR

**Spec:** `docs/specs/strength-reduction-v2.md`

---

## CRITICAL RULES

1. **After EVERY task:** run code reviewer on changes, then run full validation (below)
2. **Bootstrap gen4 -O1 MUST pass** after every task. If it fails, DO NOT proceed — fix first.
3. **These changes are -O1 only.** -O0 must remain untouched and working.
4. **`rm -rf .caustic` before every rebuild** (cache goes stale).

### Full validation command (run after EVERY task)

```bash
rm -rf .caustic
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1

# -O0 examples
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O0: $?"
done

# -O1 examples
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 -O1 examples/$ex.cst && ./caustic-as examples/$ex.cst.s && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t && timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex -O1: $?"
done

# Matmul correctness
/tmp/gen1 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b | grep matmul
# MUST show: matmul(64x64) = 95692

# Bootstrap gen4 -O1
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done
# gen2, gen3, gen4 MUST be same size

# Bench
/tmp/b
```

---

## File map

| File | Change |
|------|--------|
| `src/ir/opt_passes.cst:1049-1210` | Delete current `pass_strength_reduction` (~162 lines). Write new version (~250-300 lines). |

No other files change. The function signature stays the same: `fn pass_strength_reduction(func as *d.IRFunction) as void`.

---

### Task 1: Scaffold — empty pass that does nothing

Replace the current `pass_strength_reduction` with a new empty version that just returns. This ensures nothing breaks before we add logic.

**Files:**
- Modify: `src/ir/opt_passes.cst:1049-1210`

- [ ] **Step 1: Replace pass_strength_reduction with empty stub**

Delete everything from `fn pass_strength_reduction` to the closing `}` (lines 1049-1210). Replace with:

```cst
fn pass_strength_reduction(func as *d.IRFunction) as void {
    return;
}
```

- [ ] **Step 2: Run full validation**

```bash
rm -rf .caustic
./caustic src/main.cst && ./caustic-as src/main.cst.s && ./caustic-ld src/main.cst.s.o -o /tmp/gen1
# All examples -O0 and -O1 must pass
# Bootstrap gen4 -O1 must pass (gen2=gen3=gen4)
```

Expected: everything passes (the old strength reduction had minimal impact).

- [ ] **Step 3: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction: stub vazio pra reescrita"
```

---

### Task 2: Phase 0 — Build analysis arrays

Add the analysis infrastructure inside `pass_strength_reduction`: `def_pos[]`, `def_count[]`, `use_count[]`, `label_pos[]`, loop detection.

**Files:**
- Modify: `src/ir/opt_passes.cst` (inside `pass_strength_reduction`)

- [ ] **Step 1: Implement Phase 0**

```cst
fn pass_strength_reduction(func as *d.IRFunction) as void {
    let is i32 as inst_count with mut = 0;
    let is *d.IRInst as inst with mut = func.instructions;
    while (cast(i64, inst) != 0) { inst_count = inst_count + 1; inst = inst.next; }
    if (inst_count < 6) { return; }

    let is i32 as vc = func.vreg_count;
    let is i32 as lc = func.label_count + 1;

    // def_pos[v] = instruction position of v's definition (-1 = none)
    let is *i32 as def_pos = cast(*i32, mem.galloc(cast(i64, vc) * 4));
    // def_count[v] = number of definitions (>1 = phi)
    let is *i32 as def_count = cast(*i32, mem.galloc(cast(i64, vc) * 4));
    // use_count[v] = number of uses as src1/src2 (function-wide)
    let is *i32 as use_count = cast(*i32, mem.galloc(cast(i64, vc) * 4));
    // label_pos[label] = instruction position of label
    let is *i32 as label_pos = cast(*i32, mem.galloc(cast(i64, lc) * 4));

    // Init arrays
    let is i32 as i with mut = 0;
    while (i < vc) {
        let is *i32 as dp = cast(*i32, cast(i64, def_pos) + cast(i64, i) * 4);
        *dp = cast(i32, 0 - 1);
        let is *i32 as dc = cast(*i32, cast(i64, def_count) + cast(i64, i) * 4);
        *dc = 0;
        let is *i32 as uc = cast(*i32, cast(i64, use_count) + cast(i64, i) * 4);
        *uc = 0;
        i = i + 1;
    }
    i = 0;
    while (i < lc) {
        let is *i32 as lp = cast(*i32, cast(i64, label_pos) + cast(i64, i) * 4);
        *lp = cast(i32, 0 - 1);
        i = i + 1;
    }

    // Scan all instructions
    let is i32 as pos with mut = 0;
    inst = func.instructions;
    while (cast(i64, inst) != 0) {
        if (inst.is_dead == 0) {
            // Labels
            if (inst.op == d.IR_LABEL && inst.dest.label < lc) {
                let is *i32 as lp = cast(*i32, cast(i64, label_pos) + cast(i64, inst.dest.label) * 4);
                *lp = pos;
            }
            // Defs (dest vreg, excluding STORE/COPY which use dest as address)
            if (inst.dest.op_type == d.OP_VREG && inst.op != d.IR_STORE && inst.op != d.IR_COPY) {
                let is i32 as dv = inst.dest.vreg;
                if (dv >= 0 && dv < vc) {
                    let is *i32 as dp = cast(*i32, cast(i64, def_pos) + cast(i64, dv) * 4);
                    *dp = pos;
                    let is *i32 as dc = cast(*i32, cast(i64, def_count) + cast(i64, dv) * 4);
                    *dc = *dc + 1;
                }
            }
            // Uses (src1, src2)
            if (inst.src1.op_type == d.OP_VREG && inst.src1.vreg >= 0 && inst.src1.vreg < vc) {
                let is *i32 as uc = cast(*i32, cast(i64, use_count) + cast(i64, inst.src1.vreg) * 4);
                *uc = *uc + 1;
            }
            if (inst.src2.op_type == d.OP_VREG && inst.src2.vreg >= 0 && inst.src2.vreg < vc) {
                let is *i32 as uc = cast(*i32, cast(i64, use_count) + cast(i64, inst.src2.vreg) * 4);
                *uc = *uc + 1;
            }
            // STORE/COPY dest is a use (address)
            if ((inst.op == d.IR_STORE || inst.op == d.IR_COPY) && inst.dest.op_type == d.OP_VREG) {
                let is *i32 as uc = cast(*i32, cast(i64, use_count) + cast(i64, inst.dest.vreg) * 4);
                *uc = *uc + 1;
            }
        }
        pos = pos + 1;
        inst = inst.next;
    }

    // TODO: Phase 1-3 go here

    // Cleanup
    mem.gfree(cast(*u8, def_pos));
    mem.gfree(cast(*u8, def_count));
    mem.gfree(cast(*u8, use_count));
    mem.gfree(cast(*u8, label_pos));
}
```

- [ ] **Step 2: Run code reviewer**

Dispatch code reviewer on the changes.

- [ ] **Step 3: Run full validation**

All examples -O0/-O1 pass, gen4 bootstrap passes, matmul=95692.

- [ ] **Step 4: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction: phase 0 — arrays de analise"
```

---

### Task 3: Phase 1 — Loop detection + induction variable detection

Find back-edges (loops), then detect induction variables in each loop (both stack-based and SSA patterns).

**Files:**
- Modify: `src/ir/opt_passes.cst` (inside `pass_strength_reduction`, after Phase 0)

- [ ] **Step 1: Add loop iteration and IV detection**

After the cleanup section marker (`// TODO: Phase 1-3 go here`), add:

```cst
    // Phase 1: Find back-edges and detect IVs
    // Process loops bottom-to-top (highest backedge first) for position stability
    // Collect all back-edges first, then sort by position descending

    // Max 32 loops per function
    let is [32]i32 as loop_headers;
    let is [32]i32 as loop_backs;
    let is i32 as num_loops with mut = 0;

    pos = 0;
    inst = func.instructions;
    while (cast(i64, inst) != 0) {
        if (inst.is_dead == 0 && (inst.op == d.IR_JMP || inst.op == d.IR_JZ || inst.op == d.IR_JNZ) && inst.dest.label < lc) {
            let is *i32 as lp = cast(*i32, cast(i64, label_pos) + cast(i64, inst.dest.label) * 4);
            let is i32 as header = *lp;
            if (header >= 0 && header < pos && num_loops < 32) {
                // Check no calls in loop
                let is i32 as has_call with mut = 0;
                let is *d.IRInst as chk with mut = func.instructions;
                let is i32 as cp with mut = 0;
                while (cast(i64, chk) != 0 && cp <= pos) {
                    if (cp > header && cp < pos && chk.is_dead == 0) {
                        if (chk.op == d.IR_CALL || chk.op == d.IR_SYSCALL) { has_call = 1; }
                    }
                    cp = cp + 1; chk = chk.next;
                }
                if (has_call == 0) {
                    loop_headers[num_loops] = header;
                    loop_backs[num_loops] = pos;
                    num_loops = num_loops + 1;
                }
            }
        }
        pos = pos + 1; inst = inst.next;
    }

    // Sort loops by backedge position descending (bottom-to-top)
    let is i32 as si with mut = 0;
    while (si < num_loops - 1) {
        let is i32 as sj with mut = si + 1;
        while (sj < num_loops) {
            if (loop_backs[sj] > loop_backs[si]) {
                let is i32 as th = loop_headers[si]; loop_headers[si] = loop_headers[sj]; loop_headers[sj] = th;
                let is i32 as tb = loop_backs[si]; loop_backs[si] = loop_backs[sj]; loop_backs[sj] = tb;
            }
            sj = sj + 1;
        }
        si = si + 1;
    }

    // For each loop, detect IVs and transform
    let is i32 as li with mut = 0;
    while (li < num_loops) {
        let is i32 as header = loop_headers[li];
        let is i32 as backedge = loop_backs[li];

        // === Detect induction variables ===
        // Max 8 IVs per loop
        let is [8]i32 as iv_vreg;       // body vreg (used in expressions)
        let is [8]i32 as iv_phi_vreg;   // phi vreg (multi-def, for SSA)
        let is [8]i64 as iv_step_imm;   // constant step value
        let is [8]i32 as iv_step_vreg;  // variable step vreg (-1 = constant)
        let is [8]i32 as iv_is_stack;   // 1 = stack-based, 0 = SSA
        let is [8]i64 as iv_offset;     // stack offset (if stack-based)
        let is i32 as num_ivs with mut = 0;

        // --- Stack-based IV detection ---
        // Pattern: LOAD [rbp-X] → v_i ... ADD v_next = v_i, step ... STORE [rbp-X] = v_next
        let is *d.IRInst as scan with mut = func.instructions;
        let is i32 as sp with mut = 0;
        while (cast(i64, scan) != 0 && sp <= backedge) {
            if (sp > header && sp < backedge && scan.is_dead == 0) {
                if (scan.op == d.IR_LOAD && scan.src1.op_type == d.OP_IMM && scan.dest.op_type == d.OP_VREG) {
                    let is i64 as slot = scan.src1.imm;
                    let is i32 as load_vreg = scan.dest.vreg;
                    // Find ADD load_vreg, step
                    let is *d.IRInst as ascan with mut = scan.next;
                    let is i32 as asp with mut = sp + 1;
                    while (cast(i64, ascan) != 0 && asp <= backedge) {
                        if (ascan.is_dead == 0 && ascan.op == d.IR_ADD && ascan.src1.op_type == d.OP_VREG && ascan.src1.vreg == load_vreg && ascan.src2.op_type == d.OP_IMM) {
                            let is i32 as add_dest = ascan.dest.vreg;
                            // Find STORE [rbp-X] = add_dest
                            let is *d.IRInst as sscan with mut = ascan.next;
                            let is i32 as ssp with mut = asp + 1;
                            while (cast(i64, sscan) != 0 && ssp <= backedge) {
                                if (sscan.is_dead == 0 && sscan.op == d.IR_STORE && sscan.dest.op_type == d.OP_IMM && sscan.dest.imm == slot && sscan.src1.op_type == d.OP_VREG && sscan.src1.vreg == add_dest) {
                                    if (num_ivs < 8) {
                                        iv_vreg[num_ivs] = load_vreg;
                                        iv_phi_vreg[num_ivs] = cast(i32, 0 - 1);
                                        iv_step_imm[num_ivs] = ascan.src2.imm;
                                        iv_step_vreg[num_ivs] = cast(i32, 0 - 1);
                                        iv_is_stack[num_ivs] = 1;
                                        iv_offset[num_ivs] = slot;
                                        num_ivs = num_ivs + 1;
                                    }
                                    ssp = backedge + 1; // break
                                }
                                ssp = ssp + 1; sscan = sscan.next;
                            }
                            asp = backedge + 1; // break
                        }
                        asp = asp + 1; ascan = ascan.next;
                    }
                }
            }
            sp = sp + 1; scan = scan.next;
        }

        // --- SSA IV detection ---
        // Pattern: ADD v_next = v_i, step where v_i comes from CAST/MOV of v_phi (def_count > 1)
        //          and IR_MOV v_phi = v_next exists in loop (back-edge copy)
        scan = func.instructions; sp = 0;
        while (cast(i64, scan) != 0 && sp <= backedge) {
            if (sp > header && sp < backedge && scan.is_dead == 0) {
                if (scan.op == d.IR_ADD && scan.src2.op_type == d.OP_IMM && scan.src1.op_type == d.OP_VREG && scan.dest.op_type == d.OP_VREG) {
                    let is i32 as v_i = scan.src1.vreg;
                    let is i32 as v_next = scan.dest.vreg;
                    if (v_i >= 0 && v_i < vc) {
                        // Follow v_i back through CAST/MOV to find phi vreg
                        let is i32 as phi_v with mut = cast(i32, 0 - 1);
                        let is *d.IRInst as dscan with mut = func.instructions;
                        let is i32 as dsp with mut = 0;
                        while (cast(i64, dscan) != 0 && dsp <= backedge) {
                            if (dscan.is_dead == 0 && dscan.dest.op_type == d.OP_VREG && dscan.dest.vreg == v_i) {
                                if ((dscan.op == d.IR_MOV || dscan.op == d.IR_CAST) && dscan.src1.op_type == d.OP_VREG) {
                                    let is i32 as candidate = dscan.src1.vreg;
                                    if (candidate >= 0 && candidate < vc) {
                                        let is *i32 as dcp = cast(*i32, cast(i64, def_count) + cast(i64, candidate) * 4);
                                        if (*dcp > 1) { phi_v = candidate; }
                                    }
                                }
                            }
                            dsp = dsp + 1; dscan = dscan.next;
                        }
                        if (phi_v >= 0) {
                            // Verify: IR_MOV phi_v = v_next exists in loop
                            let is i32 as found_backedge with mut = 0;
                            let is *d.IRInst as bscan with mut = func.instructions;
                            let is i32 as bsp with mut = 0;
                            while (cast(i64, bscan) != 0 && bsp <= backedge) {
                                if (bsp > header && bscan.is_dead == 0 && bscan.op == d.IR_MOV && bscan.dest.op_type == d.OP_VREG && bscan.dest.vreg == phi_v && bscan.src1.op_type == d.OP_VREG && bscan.src1.vreg == v_next) {
                                    found_backedge = 1;
                                }
                                bsp = bsp + 1; bscan = bscan.next;
                            }
                            if (found_backedge == 1 && num_ivs < 8) {
                                // Check not already detected as stack IV
                                let is i32 as dup with mut = 0;
                                let is i32 as di with mut = 0;
                                while (di < num_ivs) { if (iv_vreg[di] == v_i) { dup = 1; } di = di + 1; }
                                if (dup == 0) {
                                    iv_vreg[num_ivs] = v_i;
                                    iv_phi_vreg[num_ivs] = phi_v;
                                    iv_step_imm[num_ivs] = scan.src2.imm;
                                    iv_step_vreg[num_ivs] = cast(i32, 0 - 1);
                                    iv_is_stack[num_ivs] = 0;
                                    iv_offset[num_ivs] = 0;
                                    num_ivs = num_ivs + 1;
                                }
                            }
                        }
                    }
                }
            }
            sp = sp + 1; scan = scan.next;
        }

        // TODO: Phase 2-3 (pattern matching + transformation)

        li = li + 1;
    }
```

- [ ] **Step 2: Run code reviewer**

- [ ] **Step 3: Run full validation**

- [ ] **Step 4: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction: phase 1 — detecao de loops e induction variables"
```

---

### Task 4: Phase 2 — Pattern matching (SHL and MUL chains)

For each IV in each loop, find derived expressions: SHL, MUL by invariant, and full chains (MUL→ADD→SHL→ADD). Follow through IR_CAST nodes.

**Files:**
- Modify: `src/ir/opt_passes.cst` (replace `// TODO: Phase 2-3`)

- [ ] **Step 1: Implement chain detection**

After the IV detection, where `// TODO: Phase 2-3` is, add pattern matching. For each instruction in the loop body, check if it's a MUL or SHL using an IV vreg. If so, follow the chain forward (ADD base, SHL, ADD base) checking single-use at each step. Store matched chains in flat arrays.

Key helper: to check if a vreg is an IV, scan `iv_vreg[0..num_ivs]`. To check if a vreg is loop-invariant, check `def_count[v] == 1 && def_pos[v] < header`. To follow through IR_CAST, when looking at a vreg's definition, if it's IR_CAST, follow to the CAST's src1.

Store matches as:
```
chain_iv_idx[k]     — which IV this chain uses
chain_start_inst[k] — first instruction in chain (MUL or SHL)
chain_end_inst[k]   — last instruction (ADD base, producing the address)
chain_stride_imm[k] — constant stride (if SHL: 1<<shift; if MUL by const: the const)
chain_stride_vreg[k]— variable stride vreg (-1 if constant)
chain_base_vreg[k]  — the base pointer vreg
chain_shift[k]      — the SHL shift amount (0 if no SHL in chain)
```

Max 16 chains per loop.

- [ ] **Step 2: Run code reviewer**

- [ ] **Step 3: Run full validation**

The pass still doesn't transform anything — just detects. All tests must pass unchanged.

- [ ] **Step 4: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction: phase 2 — pattern matching de chains"
```

---

### Task 5: Phase 3 — Transformation

For each matched chain, create an accumulator vreg, insert initialization before the loop header, replace the chain with the accumulator, and insert increment after the IV's ADD.

**Files:**
- Modify: `src/ir/opt_passes.cst` (add transformation after pattern matching)

- [ ] **Step 1: Implement transformation**

For each matched chain:

1. **Create accumulator:** `let is i32 as acc = func.vreg_count; func.vreg_count = func.vreg_count + 1;`

2. **Insert init before header:** Find instruction before header label. If IV init is 0 (check step_imm and initial value), emit `IR_MOV acc = base_vreg`. Otherwise emit `IR_MUL tmp = init, stride; IR_SHL tmp2 = tmp, shift; IR_ADD acc = base, tmp2`.

3. **Replace chain:** Change `chain_end_inst` to `IR_MOV chain_end_dest = acc`. Mark all intermediate instructions (`chain_start` through `chain_end - 1`) as dead.

4. **Insert increment:** After the IV's ADD instruction, insert `IR_ADD acc = acc, stride_bytes`. For constant stride with SHL: `stride_bytes = step_imm << shift`. For variable stride: insert `IR_MUL tmp = step, stride_vreg; IR_ADD acc = acc, tmp` (or just `IR_ADD acc = acc, stride_vreg` when step==1).

- [ ] **Step 2: Run code reviewer**

- [ ] **Step 3: Run full validation**

**CRITICAL:** matmul(64x64) MUST output 95692. If not, the transformation has a bug.

- [ ] **Step 4: Benchmark**

```bash
/tmp/gen1 -O1 examples/bench.cst && ./caustic-as examples/bench.cst.s && \
./caustic-ld examples/bench.cst.s.o -o /tmp/b && /tmp/b
# Target: matmul < 100ms, total < 700ms
```

- [ ] **Step 5: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction: phase 3 — transformacao de chains em ptr increment"
```

---

### Task 6: Final cleanup and edge cases

Handle remaining edge cases: IV with non-zero init, variable step, multiple chains per loop. Remove any dead code from old implementation.

**Files:**
- Modify: `src/ir/opt_passes.cst`

- [ ] **Step 1: Handle edge cases**

- Non-zero IV init: insert full computation (MUL + SHL + ADD) before header
- Variable step (step_vreg != -1): insert MUL step, stride before ADD acc
- init == 0 fast path: just MOV acc = base (skip MUL 0, stride)

- [ ] **Step 2: Run code reviewer**

- [ ] **Step 3: Run full validation + gen4 bootstrap**

- [ ] **Step 4: Final benchmark**

```bash
taskset -c 11 /tmp/b
# Compare with baseline: matmul was ~169ms, target <100ms
```

- [ ] **Step 5: Commit**

```bash
git add src/ir/opt_passes.cst
git commit -m "strength-reduction v2: MUL por variavel + array indexing completo"
```

---

## Caustic Gotchas

- `with mut` for mutable variables
- `rm -rf .caustic` before every rebuild
- `fn` and `gen` are keywords
- `cast(i64, x)` for i32→i64
- `0 - 1` instead of `-1`; `cast(i32, 0 - 1)` for -1 as i32
- Array access: `cast(*i32, cast(i64, arr) + cast(i64, idx) * 4)`
- New vregs: `func.vreg_count = func.vreg_count + 1`
- Insert before header: `new.next = header_label; hprev.next = new`
- Insert after inst: `new.next = inst.next; inst.next = new`
- Mark dead: `inst.is_dead = 1`
- Commits: lowercase, sem Co-Authored-By
- Use `d.new_inst(opcode)` to create new IR instructions
- Use `d.op_vreg(v)`, `d.op_imm(val)`, `d.op_label(l)` for operands
