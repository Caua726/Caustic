# Typed AST to IR

The IR generation phase is the fourth phase of the Caustic compiler. It transforms the typed AST into a virtual-register intermediate representation (IR) that abstracts away name resolution and type checking, producing a linear sequence of operations suitable for register allocation and code generation.

## Files

| File | Purpose |
|------|---------|
| `src/ir/gen.cst` | IR generation: walks the typed AST, emits IR instructions into function chains |
| `src/ir/defs.cst` | IR data structures: IRProgram, IRFunction, IRInst, IRGlobal, StringEntry, Operand, opcode constants |

## Input and Output

**Input**: Typed `*Node` linked list from semantic analysis, plus a `no_main` flag (set by `-c` for library compilation).

**Output**: `*IRProgram` containing linked lists of functions, globals, and string literals.

```cst
let is *ird.IRProgram as prog = irg.gen_ir(cast(*u8, root), no_main);
```

## IRProgram Structure

```cst
struct IRProgram {
    functions as *IRFunction;   // linked list of all functions
    main_func as *IRFunction;   // pointer to the main function (or null with -c)
    globals as *IRGlobal;       // linked list of global variables
    strings as *StringEntry;    // linked list of string literals
}
```

### IRFunction

Each function in the program becomes an `IRFunction`:

```cst
struct IRFunction {
    name_ptr as *u8;            // source-level name
    name_len as i32;
    asm_name_ptr as *u8;        // assembly-level name (may include module prefix)
    asm_name_len as i32;
    instructions as *IRInst;    // linked list of IR instructions
    vreg_count as i32;          // total virtual registers used
    label_count as i32;         // total labels used
    is_reachable as i32;        // 1 if called or main; 0 for dead code
    alloc_stack_size as i32;    // stack space for local arrays/structs
    stack_size as i32;          // total stack frame from semantic phase
    num_args as i32;            // number of parameters
    is_variadic as i32;         // 1 for variadic functions
    next as *IRFunction;        // next function in program
}
```

### IRGlobal

Global variables become `IRGlobal` entries:

```cst
struct IRGlobal {
    name_ptr as *u8;
    name_len as i32;
    size as i32;                // byte size
    init_value as i64;          // initial value (for initialized globals)
    is_initialized as i32;      // 1 if has initializer
    is_reachable as i32;        // 1 if referenced
    next as *IRGlobal;
}
```

### StringEntry

String literals are collected into a global table:

```cst
struct StringEntry {
    id as i32;                  // unique numeric ID (for label generation: .str0, .str1, ...)
    ptr as *u8;                 // pointer to string content
    len as i32;                 // byte length
    next as *StringEntry;
}
```

## IR Instructions

Each instruction is an `IRInst`:

```cst
struct IRInst {
    op as i32;                  // IR_* opcode
    next as *IRInst;            // next instruction in chain
    dest as Operand;            // destination operand
    src1 as Operand;            // first source operand
    src2 as Operand;            // second source operand
    line as i32;                // source line number (for debugging)
    is_dead as i32;             // dead code elimination flag
    asm_str_ptr as *u8;         // inline assembly string (IR_ASM)
    asm_str_len as i32;
    call_name_ptr as *u8;       // function name (IR_CALL)
    call_name_len as i32;
    is_variadic_call as i32;    // variadic call flag
    global_name_ptr as *u8;     // global name (IR_ADDR_GLOBAL)
    global_name_len as i32;
    is_extern_global as i32;    // extern global flag
    cast_to as *u8;             // *Type target (IR_CAST)
    cast_from as *u8;           // *Type source (IR_CAST)
}
```

### Operand Types

Each operand has a type tag and corresponding value:

```cst
struct Operand {
    op_type as i32;     // OP_NONE, OP_VREG, OP_IMM, OP_LABEL
    vreg as i32;        // virtual register number (OP_VREG)
    imm as i64;         // immediate value (OP_IMM)
    label as i32;       // label number (OP_LABEL)
}
```

| Operand Type | Value | Description |
|-------------|-------|-------------|
| `OP_NONE` | 0 | No operand |
| `OP_VREG` | 1 | Virtual register reference (v0, v1, v2, ...) |
| `OP_IMM` | 2 | Immediate constant value |
| `OP_LABEL` | 3 | Label reference (for jumps) |

## IR Opcodes

There are 46 IR opcodes (IR_IMM through IR_FNEG):

### Data Movement

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_IMM` | 0 | `dest = imm` (load immediate) |
| `IR_MOV` | 1 | `dest = src1` (register copy) |
| `IR_COPY` | 35 | Block memory copy (structs/arrays) |
| `IR_CAST` | 24 | Type conversion: `dest = cast(to, src1)` |

### Integer Arithmetic

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_ADD` | 2 | `dest = src1 + src2` |
| `IR_SUB` | 3 | `dest = src1 - src2` |
| `IR_MUL` | 4 | `dest = src1 * src2` |
| `IR_DIV` | 5 | `dest = src1 / src2` |
| `IR_MOD` | 6 | `dest = src1 % src2` |
| `IR_NEG` | 7 | `dest = -src1` |

### Float Arithmetic

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_FADD` | 41 | `dest = src1 + src2` (float) |
| `IR_FSUB` | 42 | `dest = src1 - src2` (float) |
| `IR_FMUL` | 43 | `dest = src1 * src2` (float) |
| `IR_FDIV` | 44 | `dest = src1 / src2` (float) |
| `IR_FNEG` | 45 | `dest = -src1` (float) |

### Comparison

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_EQ` | 8 | `dest = (src1 == src2)` |
| `IR_NE` | 9 | `dest = (src1 != src2)` |
| `IR_LT` | 10 | `dest = (src1 < src2)` |
| `IR_LE` | 11 | `dest = (src1 <= src2)` |
| `IR_GT` | 12 | `dest = (src1 > src2)` |
| `IR_GE` | 13 | `dest = (src1 >= src2)` |

### Bitwise Operations

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_SHL` | 25 | `dest = src1 << src2` |
| `IR_SHR` | 26 | `dest = src1 >> src2` |
| `IR_AND` | 27 | `dest = src1 & src2` |
| `IR_OR` | 28 | `dest = src1 \| src2` |
| `IR_XOR` | 29 | `dest = src1 ^ src2` |
| `IR_NOT` | 30 | `dest = ~src1` |

### Control Flow

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_JMP` | 14 | Unconditional jump to `dest.label` |
| `IR_JZ` | 15 | Jump to `dest.label` if `src1 == 0` |
| `IR_JNZ` | 16 | Jump to `dest.label` if `src1 != 0` |
| `IR_LABEL` | 17 | Label definition: `dest.label:` |
| `IR_RET` | 19 | Return from function, value in `src1` |

### Memory

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_LOAD` | 20 | `dest = *src1` (load from memory) |
| `IR_STORE` | 21 | `*dest = src1` (store to memory) |
| `IR_ADDR` | 36 | `dest = &local_var` (address of stack variable, src1.imm = offset) |
| `IR_ADDR_GLOBAL` | 37 | `dest = &global` (address of global, name in global_name_ptr) |
| `IR_GET_ALLOC_ADDR` | 38 | `dest = &stack_alloc` (address of stack allocation for arrays/structs) |

### Function Calls

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_CALL` | 18 | Call function named in `call_name_ptr`, result in `dest` |
| `IR_GET_ARG` | 31 | `dest = argument[src1.imm]` (retrieve function parameter) |
| `IR_SET_ARG` | 32 | Set call argument: `arg[dest.imm] = src1` |
| `IR_SET_SYS_ARG` | 33 | Set syscall argument: `sys_arg[dest.imm] = src1` |
| `IR_SYSCALL` | 34 | Execute syscall, result in `dest` |
| `IR_SET_CTX` | 39 | Set context/state for subsequent operations |
| `IR_FN_ADDR` | 40 | `dest = &function` (load function pointer) |

### Other

| Opcode | Value | Semantics |
|--------|-------|-----------|
| `IR_PHI` | 22 | Phi node (used for SSA-like merge points) |
| `IR_ASM` | 23 | Inline assembly (string in asm_str_ptr) |

## Virtual Register Model

The IR uses an unlimited number of virtual registers, numbered sequentially starting from 0. Each function has its own virtual register namespace tracked by `vreg_count`.

Registers are allocated by `new_vreg()`:

```cst
fn new_vreg() as i32 {
    let is i32 as v = ctx_vreg;
    ctx_vreg = ctx_vreg + 1;
    return v;
}
```

Expression evaluation follows a simple pattern: each sub-expression evaluates to a virtual register, and the parent combines them:

```cst
// For a + b * c:
// v0 = IR_IMM a
// v1 = IR_IMM b
// v2 = IR_IMM c
// v3 = IR_MUL v1, v2
// v4 = IR_ADD v0, v3
```

## IR Generation Context

The generator maintains per-function state in global variables:

```cst
let is *d.IRFunction as ctx_func with mut;   // current function being generated
let is *d.IRInst as ctx_head with mut;        // first instruction in chain
let is *d.IRInst as ctx_tail with mut;        // last instruction in chain
let is i32 as ctx_vreg with mut;              // next virtual register number
let is i32 as ctx_label with mut;             // next label number
let is i32 as ctx_loop_depth with mut;        // current loop nesting depth
let is i32 as ctx_has_sret with mut;          // function returns struct via pointer
let is i32 as ctx_sret_vreg with mut;         // vreg holding sret pointer
let is i32 as ctx_has_returned with mut;      // set after a return statement
```

Additional state for loops and defers:

- **Loop stack** (384 bytes): Stores start/end/continue labels for nested loops (max 32 levels). Used by `break` and `continue` to jump to the correct labels.
- **Defer stack** (512 bytes): Stores pointers to deferred `NK_FNCALL` nodes (max 64). Before each `IR_RET`, all deferred calls are emitted in LIFO order.

## Control Flow Lowering

High-level control flow constructs are lowered to labels and conditional jumps:

### If/Else

```cst
if (cond) { then_body } else { else_body }
```

Becomes:

```
  <evaluate cond into v0>
  IR_JZ v0, label_else
  <then_body>
  IR_JMP label_end
  IR_LABEL label_else
  <else_body>
  IR_LABEL label_end
```

### While Loop

```cst
while (cond) { body }
```

Becomes:

```
  IR_LABEL label_start
  <evaluate cond into v0>
  IR_JZ v0, label_end
  <body>
  IR_JMP label_start
  IR_LABEL label_end
```

### For Loop

```cst
for (init; cond; step) { body }
```

Becomes:

```
  <init>
  IR_LABEL label_start
  <evaluate cond into v0>
  IR_JZ v0, label_end
  <body>
  IR_LABEL label_continue
  <step>
  IR_JMP label_start
  IR_LABEL label_end
```

## Defer Handling

Deferred calls are accumulated on the defer stack during IR generation. Before every `IR_RET` instruction, the generator emits all deferred calls in reverse order (LIFO):

```cst
fn work() as i32 {
    defer cleanup1();
    defer cleanup2();
    return 42;
}
```

IR output (simplified):

```
  v0 = IR_IMM 42           // evaluate return value first
  IR_CALL cleanup2          // last defer runs first
  IR_CALL cleanup1          // first defer runs last
  IR_RET v0                 // then return
```

## Module Deduplication

When processing modules, `gen_ir` tracks which module bodies have already been processed (in the `ir_done_bodies` array, max 256 modules) to avoid generating duplicate IR for the same module imported from multiple files.

## Reachability

Functions are marked `is_reachable = 1` when:

- They are `main` (or all functions when `-c` is used)
- They are called by a reachable function
- They are referenced via `fn_ptr()`

Unreachable functions are skipped during code generation to reduce output size.
