# Operand Types

Every IR instruction has three operand slots (`dest`, `src1`, `src2`), each of type `Operand`. The `op_type` field of an `Operand` determines which of the four operand kinds is active and which union field holds the value.

## OP_NONE (0)

Indicates an unused operand slot. Most instructions do not use all three operand positions. For example, `IR_NEG` uses only `dest` and `src1`, leaving `src2` as `OP_NONE`.

Created by `op_none()`:

```cst
fn op_none() as Operand {
    let is Operand as o;
    memzero(cast(*u8, &o), sizeof(Operand));
    o.op_type = OP_NONE;
    return o;
}
```

Codegen checks `op_type != OP_NONE` before accessing an operand, particularly for `IR_RET` where `src1` may be `OP_NONE` for void returns.

## OP_VREG (1)

References a virtual register by index. Virtual registers are function-scoped integers allocated sequentially from 0 by `new_vreg()`. The `vreg` field of the `Operand` struct holds the register index.

Created by `op_vreg(v)`:

```cst
fn op_vreg(v as i32) as Operand {
    let is Operand as o;
    memzero(cast(*u8, &o), sizeof(Operand));
    o.op_type = OP_VREG;
    o.vreg = v;
    return o;
}
```

This is by far the most common operand type. Virtually every instruction destination is a vreg, and most source operands are vregs as well.

During register allocation, each vreg is assigned either a physical register index (>= 0) or a spill slot (< 0) via the `vreg_to_loc` mapping in `AllocCtx`.

## OP_IMM (2)

Holds a 64-bit immediate integer value in the `imm` field. Used in several contexts:

- **IR_IMM**: `src1` carries the literal value to load.
- **IR_LOAD / IR_STORE**: `src1` or `dest` carries a stack frame offset (when accessing local variables by offset rather than by address vreg).
- **IR_SET_ARG / IR_GET_ARG / IR_SET_SYS_ARG**: `dest` or `src1` carries the argument index.
- **IR_COPY**: `src2` carries the byte count.
- **IR_GET_ALLOC_ADDR**: `src1` carries the allocation offset.

Created by `op_imm(i)`:

```cst
fn op_imm(i as i64) as Operand {
    let is Operand as o;
    memzero(cast(*u8, &o), sizeof(Operand));
    o.op_type = OP_IMM;
    o.imm = i;
    return o;
}
```

Note that float literals are stored as `OP_IMM` values where the `i64` holds the IEEE 754 bit pattern of the f64 value (produced by `f64_to_i64` in the AST layer).

## OP_LABEL (3)

References a label by index. Labels are function-scoped integers allocated sequentially from 0 by `new_label()`. The `label` field of the `Operand` struct holds the label index.

Created by `op_label(l)`:

```cst
fn op_label(l as i32) as Operand {
    let is Operand as o;
    memzero(cast(*u8, &o), sizeof(Operand));
    o.op_type = OP_LABEL;
    o.label = l;
    return o;
}
```

Used exclusively in:
- **IR_LABEL**: `dest` defines the label.
- **IR_JMP**: `dest` is the target label.
- **IR_JZ / IR_JNZ**: `dest` is the target label.

In codegen, labels are emitted as `.LN:` where N is the label index.

## Operand Struct Layout

```cst
struct Operand {
    op_type as i32;   // OP_NONE, OP_VREG, OP_IMM, or OP_LABEL
    vreg as i32;      // active when op_type == OP_VREG
    imm as i64;       // active when op_type == OP_IMM
    label as i32;     // active when op_type == OP_LABEL
}
```

Since Caustic structs are packed (no padding), the total size of an `Operand` is 4 + 4 + 8 + 4 = 20 bytes. All three fields (`vreg`, `imm`, `label`) are always present in memory, but only the one matching `op_type` is semantically meaningful.
