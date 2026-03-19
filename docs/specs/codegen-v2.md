# Codegen v2

Rewrite of `src/codegen/emit.cst` and `src/codegen/alloc.cst` to generate quality x86_64 code. Only active under -O1.

## 1. Instruction selection by pattern matching

Current: each IR instruction maps to a fixed template (load_op → operate → store_op).

New: look at IR instruction + its operands' definitions to select the best x86 encoding.

### Patterns:

**Store immediate to memory:**
```
IR_IMM v1 = 0
IR_STORE [addr] = v1
→ mov BYTE/DWORD/QWORD PTR [addr], 0
```

**Array access with index:**
```
IR_ADDR v1 = base_offset
IR_LOAD v2 = [v1]     (base pointer)
IR_SHL v3 = index, 3  (scale by 8)
IR_ADD v4 = v2, v3    (base + offset)
IR_LOAD v5 = [v4]     (load element)
→ mov reg, QWORD PTR [base_reg + index_reg * 8]
```

**Increment i32 on stack:**
```
IR_LOAD v1 = [rbp-X]
IR_ADD v2 = v1, 1
IR_STORE [rbp-X] = v2
→ add DWORD PTR [rbp-X], 1
```

**Compare and branch (already partially done):**
```
IR_CMP v1, v2
IR_JZ label
→ cmp reg1, reg2; jz label
```

**LEA for address computation:**
```
IR_MUL v1 = i, n
IR_ADD v2 = v1, k
IR_SHL v3 = v2, 3
IR_ADD v4 = base, v3
→ lea reg, [base + v2*8]  (when possible)
  or: imul reg, i, n; add reg, k; lea reg, [base + reg*8]
```

### Implementation:

In `gen_inst`, before emitting the standard template, check if `inst` + `inst.next` (+ `inst.next.next`) match a known pattern. If so, emit the fused version and mark the consumed instructions as dead.

This is a peephole on the IR, not on the assembly. Runs during codegen, not as a separate pass.

## 2. Addressing modes

Support `[base + index*scale + displacement]` in the assembly output.

### What changes:

- `gen_inst_memory` for IR_LOAD/IR_STORE: detect when the address vreg was computed by `base + index * scale` and emit the addressing mode directly instead of computing the address in a register.

- The IR_LOAD/IR_STORE instructions carry the address as a vreg. Look at how that vreg was defined (trace back through the IR). If it's `ADD(base_vreg, SHL(index_vreg, 3))`, emit `[base_reg + index_reg * 8]`.

- Need a helper: `try_match_address(vreg, ctx) → (base_reg, index_reg, scale, disp)` that pattern-matches the address computation.

- Scales supported by x86: 1, 2, 4, 8.

## 3. Register caching during emission

Track which physical register holds which value during codegen of a basic block.

### What changes:

- Maintain a map: `reg_cache[phys_reg] = vreg` (which vreg is currently in this register).
- Before `load_op(vreg, target)`, check if `vreg` is already in `target` or any other register. If so, skip the load or emit `mov target, cached_reg`.
- Invalidate cache at: labels, calls, stores through pointers (anything that might alias).
- This eliminates the repeated `movsxd rax, r14d` pattern (if r14 already has the sign-extended value, don't redo it).

## 4. Eliminate redundant sign-extension

Track which vregs are already sign-extended from i32 to i64.

### What changes:

- When codegen emits `movsxd reg, reg32` (for IR_LOAD with i32 type or IR_CAST), mark `reg` as "sign-extended".
- On subsequent use of the same vreg, skip the movsxd if already marked.
- Clear marks at basic block boundaries.
- This is part of the register cache (item 3): the cache tracks not just which vreg is in which register, but also whether it's already sign-extended.

## 5. Graph coloring register allocator (-O1 only)

Replace linear scan with graph coloring for -O1.

### What changes:

- Build interference graph: two vregs interfere if their live ranges overlap.
- Color the graph with K colors (K = number of physical registers = 10).
- Vregs that can't be colored are spilled.
- Coalescing: if two vregs connected by IR_MOV don't interfere, merge them (same color). This eliminates MOV instructions.

### Key difference from linear scan:

- Linear scan assigns registers in order of interval start. A vreg that starts early but is rarely used can block a frequently-used vreg.
- Graph coloring assigns based on interference, not order. Frequently-used vregs in tight loops are more likely to get registers.
- Coalescing is built-in: MOV-connected vregs that don't interfere get the same register automatically.

### Spill cost:

- Weight uses inside loops by 10x (use the backward jump detection from `build_intervals`).
- Spill the vreg with lowest weighted cost.

### Keep linear scan for -O0:

- Linear scan is faster to compile (O(n) vs O(n²)).
- -O0 uses linear scan, -O1 uses graph coloring.

## 6. Inlining (-O1 only)

Inline small functions at the call site.

### What changes:

- In the IR optimizer, before codegen, scan for IR_CALL to functions with:
  - Body size < 20 IR instructions
  - No recursion
  - No variadic args
- Replace the CALL with a copy of the function's IR body, with vreg renaming.
- This eliminates call/return overhead and enables further optimization of the inlined code (constant propagation, DCE).

### Impact:

- `fib` won't benefit (recursive).
- Small helpers like `collatz_steps` called 1M times will benefit hugely.
- Standard library functions (strlen, memcpy) could be inlined.

## 7. Loop strength reduction

Reduce expensive operations inside loops to cheaper ones.

### What changes:

- Detect `i * n + k` patterns where `n` is loop-invariant.
- Replace with an induction variable: `addr += n_stride` on each iteration instead of recomputing.
- Example: matmul inner loop computes `i*n+k` every iteration. Replace with `ptr_a += 8` (stride).

### Implementation:

- In the IR optimizer, after LICM.
- For each loop, find multiply/add sequences that can be converted to an induction variable (initial value + increment).
- Replace the multiply with an add of the stride.

## Order of implementation

1. Instruction selection patterns (item 1) — biggest impact, foundation for everything else
2. Register caching (item 3) + sign-extension tracking (item 4) — eliminates redundant loads/movsxd
3. Addressing modes (item 2) — reduces instruction count for array access
4. Graph coloring allocator (item 5) — better register decisions
5. Loop strength reduction (item 7) — eliminates multiplications in loops
6. Inlining (item 6) — eliminates call overhead

## Files affected

- `src/codegen/emit.cst` — items 1, 2, 3, 4 (major rewrite)
- `src/codegen/alloc.cst` — item 5 (new allocator, keep old for -O0)
- `src/ir/opt_passes.cst` — item 7 (new pass)
- `src/ir/opt.cst` — item 6 (inlining pass), item 7 hookup
- `src/main.cst` — gate items 5, 6, 7 behind -O1

## 8. Peephole optimizer improvements

Current peephole (`src/codegen/peephole.cst`) runs on the .s output. These improvements make it faster and more effective.

### 8.1 Buffered write
Current: 460K+ syscalls (2x `linux.write` per line). Replace with a single buffered write — accumulate output in memory, write once at the end.

### 8.2 Early exit
If no patterns matched (stat_removed == 0 && stat_fused == 0), skip rewriting the file — the output is already correct.

### 8.3 Pre-filter
Before calling pattern matchers, check if the first non-whitespace byte is `m` (109) — only `mov`/`movsxd` instructions can match patterns 1 and 3. Eliminates ~90% of match_inst calls.

### 8.4 AoS layout
Current line table uses separate offset/length arrays. Interleave {offset, length} in a single array with stride 12 for better cache locality during the main scan loop.

### 8.5 File-backed mmap
Use mmap with MAP_PRIVATE on the .s file instead of anonymous mmap + read. Eliminates one copy of the file data.
