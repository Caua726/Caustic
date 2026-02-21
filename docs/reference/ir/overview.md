# IR Overview

The Caustic intermediate representation (IR) sits between the semantic analysis phase and code generation. It transforms the typed AST into a linear sequence of low-level operations that map closely to machine instructions while remaining architecture-independent.

## Design Philosophy

The IR follows a register-transfer model with unlimited virtual registers (vregs). Each expression evaluation produces a result in a fresh vreg, and vregs are never reused. This simplifies IR generation because the front-end does not need to worry about register pressure or liveness -- that responsibility falls entirely on the register allocator in codegen.

The IR is intentionally simple. There is no SSA phi-node insertion (IR_PHI exists as a constant but is unused), no dominance-based optimizations, and no complex control-flow graph construction. Instead, control flow is expressed through labels and conditional/unconditional jumps, which the codegen phase translates directly to x86_64 branch instructions.

## Unlimited Virtual Registers

Every intermediate value gets its own vreg, allocated sequentially from 0 via `new_vreg()`. A function's total vreg count is tracked in `IRFunction.vreg_count`. There is no upper limit on the number of vregs per function -- the linear scan register allocator in codegen handles the mapping to physical registers and spills excess vregs to the stack.

This design means:
- No register coalescing is needed at the IR level.
- Every `emit_binary`, `emit_imm`, `emit_cast`, `emit_call`, etc. allocates a new destination vreg.
- The IR generator never reads from a vreg that was not previously written.
- Vregs are function-scoped; vreg 0 in one function has no relation to vreg 0 in another.

## Instruction Representation

Each IR instruction is an `IRInst` struct containing:
- An opcode (`op`) identifying the operation.
- Up to three operands: `dest`, `src1`, `src2`, each of type `Operand`.
- A `next` pointer forming a singly-linked list.
- Optional metadata fields for assembly strings, call target names, cast types, and global names.

Instructions are appended to the current function's instruction list via the `emit()` function, which maintains `ctx_head` and `ctx_tail` pointers. This yields a flat, linear instruction stream per function with no basic block boundaries.

## Program Structure

The top-level `IRProgram` struct contains:
- A linked list of `IRFunction` nodes (all functions in the program and its imported modules).
- A pointer to the `main` function (required unless compiling with `-c`).
- A linked list of `IRGlobal` nodes (global variables).
- A linked list of `StringEntry` nodes (string literal constants used in `.rodata`).

## IR Generation Flow

The entry point is `gen_ir(root, no_main)`, which walks the AST top-level nodes:
1. **Global variables** (`NK_LET` with `is_global=1`) become `IRGlobal` entries with compile-time constant initialization via `eval_const_expr`.
2. **Functions** (`NK_FN`) reset the per-function context, emit `GET_ARG` + `STORE` for parameter binding, then recursively generate IR for the function body via `gen_stmt` and `gen_expr`.
3. **Imports** (`NK_USE`) recursively call `gen_ir` on the module body, merging the resulting functions and globals into the main program. A deduplication array (`ir_done_bodies`) prevents re-processing the same module.

After all functions are generated, `mark_reachable` performs a transitive closure from `main` (or marks everything reachable in `-c` mode) to identify which functions and globals should appear in the output.

## Constant Folding

The IR generator performs basic constant folding for integer arithmetic at the expression level. When both operands of an `NK_ADD`, `NK_SUB`, `NK_MUL`, `NK_DIV`, or `NK_MOD` node are `NK_NUM` literals, the result is computed at compile time and emitted as a single `IR_IMM` instruction rather than two loads and an arithmetic operation.

## Labels

Labels are represented as integer indices allocated sequentially via `new_label()`. Each function has its own label namespace tracked by `IRFunction.label_count`. Labels appear in the instruction stream as `IR_LABEL` instructions and are referenced by `IR_JMP`, `IR_JZ`, and `IR_JNZ` instructions using the `OP_LABEL` operand type.
