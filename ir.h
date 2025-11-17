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
    IR_ADDR,
    IR_PHI,
    IR_ASM,
    IR_CAST,
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
    Type *cast_to_type;
} IRInst;

typedef struct IRFunction {
    char name[64];
    IRInst *instructions;
    int vreg_count;
    int label_count;
    struct IRFunction *next;
} IRFunction;

typedef struct {
    IRFunction *functions;
    IRFunction *main_func;
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

static const char *IR_OP_NAMES[] = {
    "IMM", "MOV",
    "ADD", "SUB", "MUL", "DIV", "MOD", "NEG",
    "EQ", "NE", "LT", "LE", "GT", "GE",
    "JMP", "JZ", "JNZ", "LABEL",
    "CALL", "RET",
    "LOAD", "STORE", "ADDR",
    "PHI", "ASM", "CAST", "CALL",
};

IRProgram *gen_ir(Node *ast);
void ir_free(IRProgram *prog);
void ir_print(IRProgram *prog);
