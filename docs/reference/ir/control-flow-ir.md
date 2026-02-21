# Control Flow in IR

This document describes how Caustic's high-level control flow constructs are lowered to the flat label-and-jump IR representation.

## If / Else

An `if` statement with an optional `else` branch is lowered to:

```
    <evaluate condition into vreg C>
    IR_JZ C, else_label
    <then body>
    IR_JMP end_label
IR_LABEL else_label
    <else body>       (omitted if no else branch)
IR_LABEL end_label
```

The `ctx_has_returned` flag is saved before entering either branch and restored afterward. If both the then-branch and the else-branch set `ctx_has_returned`, the flag remains set after the if statement, indicating the function has returned on all paths. Otherwise it is restored to its pre-if value.

The `defer_count` is also saved and restored around each branch to ensure that defers registered inside a branch are scoped to that branch.

## While Loop

A `while` loop is lowered to:

```
IR_LABEL start_label
    <evaluate condition into vreg C>
    IR_JZ C, end_label
    <loop body>
    IR_JMP start_label
IR_LABEL end_label
```

The loop's start, end, and continue labels are pushed onto the loop stack via `push_loop(start, end, start)`. For a while loop, the continue label is the same as the start label since `continue` should re-evaluate the condition.

## Do-While Loop

A `do-while` loop is lowered to:

```
IR_LABEL start_label
    <loop body>
IR_LABEL cond_label
    <evaluate condition into vreg C>
    IR_JNZ C, start_label
IR_LABEL end_label
```

Note the use of `IR_JNZ` (jump if not zero) to loop back when the condition is true. The continue label points to `cond_label` so that `continue` jumps to the condition check rather than the loop start.

## For Loop

A `for` loop `for (init; cond; step) { body }` is lowered to:

```
    <init statement or expression>
IR_LABEL start_label
    <evaluate condition into vreg C>  (if condition exists)
    IR_JZ C, end_label                (if condition exists)
    <loop body>
IR_LABEL step_label
    <step expression>                 (if step exists)
    IR_JMP start_label
IR_LABEL end_label
```

The loop stack is set up as `push_loop(start, end, step)`. This means `continue` jumps to the step label, not the start label, ensuring the step expression is evaluated before re-checking the condition.

The init statement can be either a `NK_LET` (variable declaration) or an expression.

## Break and Continue

`break` emits `IR_JMP` to the current loop's end label (retrieved from the loop stack via `get_loop_end()`).

`continue` emits `IR_JMP` to the current loop's continue label (retrieved via `get_loop_continue()`). For `while` loops this is the start label; for `for` loops it is the step label; for `do-while` loops it is the condition label.

If emitted outside a loop (`ctx_loop_depth == 0`), both are silently ignored.

## Loop Stack

The loop stack is a fixed-size array supporting up to 32 levels of nesting. Each entry stores three i32 values (12 bytes total):

- **start**: the label at the top of the loop
- **end**: the label after the loop (break target)
- **continue**: the label for continue (re-check or step)

## Short-Circuit Logical AND (&&)

The `&&` operator uses short-circuit evaluation:

```
    <evaluate LHS into vreg L>
    IR_JZ L, false_label         -- short-circuit: if LHS is false, skip RHS
    <evaluate RHS into vreg R>
    IR_JZ R, false_label         -- if RHS is false, result is false
    IR_IMM dest, 1               -- both true
    IR_JMP end_label
IR_LABEL false_label
    IR_IMM dest, 0               -- at least one false
IR_LABEL end_label
```

The same destination vreg `dest` is written on both paths. This is not true SSA but works because the register allocator extends the live interval across both branches.

## Short-Circuit Logical OR (||)

The `||` operator:

```
    <evaluate LHS into vreg L>
    IR_JNZ L, true_label         -- short-circuit: if LHS is true, skip RHS
    <evaluate RHS into vreg R>
    IR_JNZ R, true_label         -- if RHS is true, result is true
    IR_IMM dest, 0               -- both false
    IR_JMP end_label
IR_LABEL true_label
    IR_IMM dest, 1               -- at least one true
IR_LABEL end_label
```

## Logical NOT (!)

Logical NOT is lowered to a comparison against zero:

```
    <evaluate expression into vreg S>
    IR_IMM zero, 0
    IR_EQ dest, S, zero          -- dest = (S == 0) ? 1 : 0
```

## Match Statement

A `match` statement on an enum or tagged union is lowered to a chain of comparisons:

```
    <evaluate match expression into vreg E>
    <load discriminant from E into vreg D>   (if tagged union)

    -- case 0:
    IR_IMM imm0, <discriminant_value_0>
    IR_EQ cmp0, D, imm0
    IR_JZ cmp0, next0_label
    <bind payload variables if any>
    <case 0 body>
    IR_JMP end_label
IR_LABEL next0_label

    -- case 1:
    IR_IMM imm1, <discriminant_value_1>
    IR_EQ cmp1, D, imm1
    IR_JZ cmp1, next1_label
    <case 1 body>
    IR_JMP end_label
IR_LABEL next1_label

    -- else (optional):
    <else body>

IR_LABEL end_label
```

For tagged unions, the discriminant is loaded with an `IR_LOAD` of type `i32` from the base address. Payload bindings are created by computing the payload address (base + payload_offset) and storing the loaded value into the binding variable's stack slot.

Like `if` statements, `ctx_has_returned` is tracked per-arm to determine whether all arms return.

## Defer Interaction

The `defer_count` is saved and restored around loops, if/else blocks, and match arms. This ensures that deferred calls registered inside a control flow block are scoped to that block. Before any `IR_RET`, `emit_deferred_calls()` iterates the defer stack in LIFO order and emits the deferred function calls.
