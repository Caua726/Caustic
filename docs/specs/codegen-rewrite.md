# Codegen rewrite: eliminate r15 scratch dependency + SSA i32

## Step 1: Rewrite emit.cst — replace r15 with dynamic scratch

New helper `get_scratch(dest_vreg, ctx)`: returns dest register if allocated, rax if spilled.

### Per function:
- gen_binop: use get_scratch(dest) instead of "r15"
- gen_inst_imm_mov: IR_MOV uses get_scratch(dest)
- gen_inst_compare: fused uses get_scratch(src1), unfused uses rcx to avoid xor eax conflict
- gen_inst_shift_bitwise: use get_scratch(dest)
- gen_inst_arith: MUL uses get_scratch + rcx for aux. DIV/MOD use rcx for divisor
- gen_inst_memory: LOAD uses get_scratch. STORE uses rcx for addr
- gen_inst_float: use rcx instead of r15
- gen_inst_control: JZ/JNZ use rax
- gen_inst_calls: GET_ARG uses get_scratch
- emit_mov_imm: large imm to spill uses rax
- Prologue/epilogue: r15 push/pop conditional via mask bit 16

### Allocator:
- Add r15(15) to allowed[] as slot 9 (lowest priority, callee-saved)
- Update reg_to_slot, active_idx size, all loops from 9→10

### Test: gen4 fixpoint + fibonacci + bench after each sub-function

## Step 2: SSA i32 — promote all stack variables

Move `ssa.mem2reg(func)` from before to after ADDR fusion in opt.cst.

Remove the pre-fusion SSA call. Add post-fusion call with tracking array reallocation:

```
passes.pass_addr_fusion(func, vc_alloc);
ssa.mem2reg(func);
if (func.vreg_count > vc) {
    vc = func.vreg_count;
    // realloc is_const, imm_val, copy_of with new vc + 256
}
```

i32 sign extension handled by IR_CAST in ssa_rename (already implemented).

### Test: gen4 fixpoint + fibonacci + all examples + bench
