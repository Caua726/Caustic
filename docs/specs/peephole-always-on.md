# Move peephole optimizer out of -O1 gate

The peephole runs after codegen and cleans up redundant MOVs that the codegen generates by design. It should run always, not just under -O1.

## Change

In `src/main.cst`, find the two places where the peephole is called (gated by `g_opt_level >= 1`). Remove the gate — call unconditionally.
