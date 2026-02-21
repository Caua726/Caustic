# IR Data Structures

This document describes the structs that make up Caustic's intermediate representation. All are defined in `src/ir/defs.cst` and allocated on the heap via `mem.galloc`. Every struct is zero-initialized after allocation using `memzero`.

## Operand

```cst
struct Operand {
    op_type as i32;
    vreg as i32;
    imm as i64;
    label as i32;
}
```

A tagged union representing one operand of an IR instruction. See [Operand Types](operand-types.md) for details.

**Size**: 20 bytes (packed).

## IRInst

```cst
struct IRInst {
    op as i32;
    next as *IRInst;
    dest as Operand;
    src1 as Operand;
    src2 as Operand;
    line as i32;
    is_dead as i32;
    asm_str_ptr as *u8;
    asm_str_len as i32;
    call_name_ptr as *u8;
    call_name_len as i32;
    is_variadic_call as i32;
    global_name_ptr as *u8;
    global_name_len as i32;
    is_extern_global as i32;
    cast_to as *u8;
    cast_from as *u8;
}
```

A single IR instruction, linked into a singly-linked list per function.

| Field | Description |
|-------|-------------|
| `op` | The opcode (IR_IMM through IR_FNEG). |
| `next` | Pointer to the next instruction in the list, or null for the last. |
| `dest` | Destination operand. Typically a vreg (result) or label (branch target). |
| `src1` | First source operand. Vreg, immediate, or none. |
| `src2` | Second source operand. Vreg, immediate, or none. |
| `line` | Source line number (token index) for error reporting. |
| `is_dead` | Dead code flag (reserved, not currently used by the IR generator). |
| `asm_str_ptr` / `asm_str_len` | Raw assembly string for IR_ASM instructions. |
| `call_name_ptr` / `call_name_len` | Callee function name for IR_CALL instructions. |
| `is_variadic_call` | Whether the call target is a variadic function. |
| `global_name_ptr` / `global_name_len` | Symbol name for IR_ADDR_GLOBAL and IR_FN_ADDR. |
| `is_extern_global` | If 1, the global is an external symbol (loaded via GOT rather than LEA). |
| `cast_to` | Pointer to the target `Type` struct, reinterpreted as `*u8`. Used by IR_CAST (target type), IR_LOAD/IR_STORE (value type for width selection), IR_DIV/IR_MOD (signedness), IR_CALL (return type), and comparisons (signedness). |
| `cast_from` | Pointer to the source `Type` struct for IR_CAST. Null when the source type is unknown or not needed. |

The `cast_to` and `cast_from` fields are typed as `*u8` rather than `*Type` because the IR module imports the AST module's type definitions and must cast between the two pointer types at the Caustic language level.

## IRFunction

```cst
struct IRFunction {
    name_ptr as *u8;
    name_len as i32;
    asm_name_ptr as *u8;
    asm_name_len as i32;
    instructions as *IRInst;
    vreg_count as i32;
    label_count as i32;
    is_reachable as i32;
    alloc_stack_size as i32;
    stack_size as i32;
    num_args as i32;
    is_variadic as i32;
    next as *IRFunction;
}
```

Represents one function in the IR program.

| Field | Description |
|-------|-------------|
| `name_ptr` / `name_len` | The function's source-level name. |
| `asm_name_ptr` / `asm_name_len` | The assembly-level name (may differ due to name mangling for generics/impl methods). |
| `instructions` | Head of the instruction linked list for this function. |
| `vreg_count` | Total number of virtual registers allocated in this function. |
| `label_count` | Total number of labels allocated in this function. |
| `is_reachable` | Set to 1 by `mark_reachable` if the function is transitively called from `main` (or all functions in `-c` mode). Unreachable functions are excluded from codegen output. |
| `alloc_stack_size` | Extra stack space needed for SRET buffers and enum payload construction, accumulated during IR generation. |
| `stack_size` | Stack space for local variables, computed by the semantic analysis phase and copied from the AST. |
| `num_args` | Number of function parameters (not counting the hidden SRET pointer). |
| `is_variadic` | Whether the function accepts variadic arguments. |
| `next` | Pointer to the next function in the program's function list. |

## IRGlobal

```cst
struct IRGlobal {
    name_ptr as *u8;
    name_len as i32;
    size as i32;
    init_value as i64;
    is_initialized as i32;
    is_reachable as i32;
    next as *IRGlobal;
}
```

Represents a global variable.

| Field | Description |
|-------|-------------|
| `name_ptr` / `name_len` | The global's assembly-level name. |
| `size` | Size in bytes (1, 2, 4, or 8 for primitives; larger for arrays/structs). |
| `init_value` | Compile-time constant initial value, computed by `eval_const_expr`. Only valid if `is_initialized` is 1. |
| `is_initialized` | If 1, the global has an initializer and goes in `.data`. If 0, it goes in `.bss`. |
| `is_reachable` | Set to 1 if the global is referenced by any reachable function. |
| `next` | Pointer to the next global in the list. |

Globals with `is_initialized=1` and a non-zero `init_value` are emitted with `.quad`, `.long`, `.short`, or `.byte` directives. Uninitialized globals use `.zero`.

## StringEntry

```cst
struct StringEntry {
    id as i32;
    ptr as *u8;
    len as i32;
    next as *StringEntry;
}
```

Represents a string literal constant. String literals are collected during IR generation via `register_string` and emitted in the `.rodata` section.

| Field | Description |
|-------|-------------|
| `id` | Unique integer ID, used to generate the `.LCN` label. |
| `ptr` | Pointer to the string's content (heap-allocated copy). |
| `len` | Length in bytes (not including null terminator). |
| `next` | Pointer to the next string entry (linked list, prepend order). |

In the IR, string references become `IR_ADDR_GLOBAL` instructions pointing to the `.LCN` label.

## IRProgram

```cst
struct IRProgram {
    functions as *IRFunction;
    main_func as *IRFunction;
    globals as *IRGlobal;
    strings as *StringEntry;
}
```

The top-level container for the entire IR representation of a program.

| Field | Description |
|-------|-------------|
| `functions` | Head of the linked list of all functions (including those from imported modules). |
| `main_func` | Pointer to the `main` function, or null if compiling in library mode (`-c`). |
| `globals` | Head of the linked list of all global variables. |
| `strings` | Head of the linked list of all string literal constants. |

## Helper Functions

The `defs.cst` module provides factory functions for creating zero-initialized instances:

- `new_inst(op)` -- allocates and returns an `*IRInst` with the given opcode.
- `new_irfunc()` -- allocates and returns an `*IRFunction`.
- `new_irglobal()` -- allocates and returns an `*IRGlobal`.
- `new_irprog()` -- allocates and returns an `*IRProgram`.
- `new_string_entry()` -- allocates and returns a `*StringEntry`.
- `str_dup(src, len)` -- allocates a null-terminated copy of a string.
- `str_eq(a, alen, b, blen)` -- compares two length-prefixed strings for equality.
