#pragma once
#include "parser.h"

typedef enum {
    IR_IMM,
    IR_MOV,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_NEG,
    IR_EQ,
    IR_NE,
    IR_LT,
    IR_LE,
    IR_GT,
    IR_GE,
    IR_JMP,
    IR_JZ,
    IR_JNZ,
    IR_LABEL,
    IR_CALL,
    IR_RET,
    IR_LOAD,
    IR_STORE,
    IR_PHI,
    IR_ASM,
    IR_CAST,
    IR_SHL,
    IR_SHR,
    IR_GET_ARG,
    IR_SET_ARG,
    IR_SET_SYS_ARG,
    IR_SYSCALL,
    IR_COPY,
    IR_ADDR, // Get address of variable (stack)
    IR_ADDR_GLOBAL, // Get address of global variable
    IR_GET_ALLOC_ADDR, // Get address of allocated stack space
} IROp;

typedef enum {
    OPERAND_NONE,
    OPERAND_VREG,
    OPERAND_IMM,
    OPERAND_LABEL,
} OperandType;

typedef struct {
    OperandType type;
    union {
        int vreg;
        long imm;
        int label;
    };
} Operand;

typedef struct IRInst {
    IROp op;
    struct IRInst *next;
    Operand dest;
    Operand src1;
    Operand src2;
    int line;
    int is_dead;
    unsigned long live_in;
    unsigned long live_out;
    char *asm_str;
    char *call_target_name;
    char *global_name; // For IR_ADDR_GLOBAL
    Type *cast_to_type;
} IRInst;

typedef struct IRFunction {
    char *name;
    char *asm_name;
    IRInst *instructions;
    int vreg_count;
    int label_count;
    int is_reachable;
    int alloc_stack_size; // Extra stack space allocated by IR (e.g. for struct returns)
    int num_args;
    struct IRFunction *next;
} IRFunction;

typedef struct IRGlobal {
    char *name;
    int size;
    long init_value; // For simple integer initialization
    int is_initialized;
    int is_reachable;
    struct IRGlobal *next;
} IRGlobal;

typedef struct {
    IRFunction *functions;
    IRFunction *main_func;
    IRGlobal *globals;
    struct StringLiteral *strings;
} IRProgram;

static inline Operand op_none() {
    Operand op;
    op.type = OPERAND_NONE;
    op.vreg = 0;
    return op;
}

static inline Operand op_vreg(int vreg) {
    Operand op;
    op.type = OPERAND_VREG;
    op.vreg = vreg;
    return op;
}

static inline Operand op_imm(long imm) {
    Operand op;
    op.type = OPERAND_IMM;
    op.imm = imm;
    return op;
}

static inline Operand op_label(int label) {
    Operand op;
    op.type = OPERAND_LABEL;
    op.label = label;
    return op;
}

extern const char *IR_OP_NAMES[];

IRProgram *gen_ir(Node *ast);
void ir_free(IRProgram *prog);
void ir_print(IRProgram *prog);
