#include "ir.h"
#include "parser.h"
#include "semantic.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *IR_OP_NAMES[] = {
    "IMM", "MOV",
    "ADD", "SUB", "MUL", "DIV", "MOD", "NEG",
    "EQ", "NE", "LT", "LE", "GT", "GE",
    "JMP", "JZ", "JNZ", "LABEL",
    "SYSCALL",
    "COPY", "RET",
    "LOAD", "STORE", "ADDR", "ADDR_GLOBAL",
    "PHI", "ASM", "CAST", "SHL", "SHR", "CALL", "SET_ARG", "GET_ARG", "SET_SYS_ARG", "SYSCALL", "COPY", "ADDR", "ADDR_GLOBAL", "GET_ALLOC_ADDR", "SET_CTX",
};

typedef struct {
    IRFunction *current_func;
    IRInst *inst_head;
    IRInst *inst_tail;
    int vreg_count;
    struct {
        int start_label;
        int end_label;
        int continue_label;
    } loop_stack[32];

    int loop_depth;
    int has_sret;
    int sret_vreg;
    int has_returned;
} IRGenContext;

static IRGenContext ctx;
static int global_label_count = 0;

static void push_loop(int start, int end, int cont) {
    if (ctx.loop_depth >= 32) {
        fprintf(stderr, "Erro: loops aninhados demais\n");
        exit(1);
    }
    ctx.loop_stack[ctx.loop_depth].start_label = start;
    ctx.loop_stack[ctx.loop_depth].end_label = end;
    ctx.loop_stack[ctx.loop_depth].continue_label = cont;
    ctx.loop_depth++;
}

static void pop_loop() {
    if (ctx.loop_depth > 0) {
        ctx.loop_depth--;
    }
}

static IRInst *new_inst(IROp op) {
    IRInst *inst = calloc(1, sizeof(IRInst));
    inst->op = op;
    return inst;
}

static void emit(IRInst *inst) {
    if (!ctx.inst_head) {
        ctx.inst_head = inst;
        ctx.inst_tail = inst;
    } else {
        ctx.inst_tail->next = inst;
        ctx.inst_tail = inst;
    }
}

static int new_vreg() {
    return ctx.vreg_count++;
}

static int new_label() {
    return global_label_count++;
}

static int emit_imm(long value, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(IR_IMM);
    inst->dest = op_vreg(dest);
    inst->src1 = op_imm(value);
    inst->line = line;
    emit(inst);
    return dest;
}

static int emit_binary(IROp op, int lhs_reg, int rhs_reg, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(op);
    inst->dest = op_vreg(dest);
    inst->src1 = op_vreg(lhs_reg);
    inst->src2 = op_vreg(rhs_reg);
    inst->line = line;
    emit(inst);
    return dest;
}



static int emit_cast_from(int src_reg, Type *from_type, Type *to_type, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(IR_CAST);
    inst->dest = op_vreg(dest);
    inst->src1 = op_vreg(src_reg);
    inst->cast_to_type = to_type;
    inst->cast_from_type = from_type;
    inst->line = line;
    emit(inst);
    return dest;
}

static int emit_cast(int src_reg, Type *to_type, int line) {
    return emit_cast_from(src_reg, NULL, to_type, line);
}

static int emit_call(char *func_name, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(IR_CALL);
    inst->dest = op_vreg(dest);
    inst->call_target_name = strdup(func_name);
    inst->line = line;
    emit(inst);
    return dest;
}

static void emit_set_arg(int arg_idx, int src_reg, int line) {
    IRInst *inst = new_inst(IR_SET_ARG);
    inst->dest = op_imm(arg_idx);
    inst->src1 = op_vreg(src_reg);
    inst->line = line;
    emit(inst);
}

static int emit_get_arg(int arg_idx, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(IR_GET_ARG);
    inst->dest = op_vreg(dest);
    inst->src1 = op_imm(arg_idx);
    inst->line = line;
    emit(inst);
    return dest;
}

static void emit_return(int src_reg, int line) {
    IRInst *inst = new_inst(IR_RET);
    if (src_reg != -1) {
        inst->src1 = op_vreg(src_reg);
    } else {
        inst->src1.type = OPERAND_NONE;
    }
    inst->line = line;
    emit(inst);
}

static int gen_expr(Node *node);

static int gen_addr(Node *node) {
    switch (node->kind) {
        case NODE_KIND_IDENTIFIER: {
            int dest = new_vreg();
            if (node->var && node->var->is_global) {
                IRInst *inst = new_inst(IR_ADDR_GLOBAL);
                inst->dest = op_vreg(dest);
                if (node->var->asm_name) {
                    inst->global_name = strdup(node->var->asm_name);
                } else {
                    inst->global_name = strdup(node->name);
                }
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
            } else {
                IRInst *inst = new_inst(IR_ADDR);
                inst->dest = op_vreg(dest);
                inst->src1 = op_imm(node->offset);
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
            }
            return dest;
        }
        case NODE_KIND_DEREF: {
            // The address of *p is the value of p
            return gen_expr(node->expr);
        }
        case NODE_KIND_INDEX: {
            // Address of arr[i] = base_addr + i * size
            int base;
            if (node->lhs->ty->kind == TY_PTR) {
                base = gen_expr(node->lhs);
            } else {
                base = gen_addr(node->lhs);
            }
            int index = gen_expr(node->rhs);
            int size = node->ty->size;

            // Calculate offset = index * size
            int offset_reg;
            if (size == 1) {
                offset_reg = index;
            } else {
                int size_reg = emit_imm(size, node->tok ? node->tok->line : 0);
                offset_reg = emit_binary(IR_MUL, index, size_reg, node->tok ? node->tok->line : 0);
            }

            // Address = base + offset
            return emit_binary(IR_ADD, base, offset_reg, node->tok ? node->tok->line : 0);
        }
        case NODE_KIND_MEMBER_ACCESS: {
            // Address of struct.member = base_addr + offset
            int base;
            if (node->lhs->ty->kind == TY_PTR) {
                base = gen_expr(node->lhs); // Load the pointer value
            } else {
                base = gen_addr(node->lhs); // Get the address of the struct
            }

            if (node->offset == 0) {
                return base;
            }
            int offset_reg = emit_imm(node->offset, node->tok ? node->tok->line : 0);
            return emit_binary(IR_ADD, base, offset_reg, node->tok ? node->tok->line : 0);
        }
        default:
            fprintf(stderr, "Erro interno: gen_addr chamado para nó inválido: %d\n", node->kind);
            exit(1);
    }
}

static int gen_expr(Node *node) {
    if (!node) {
        fprintf(stderr, "Erro interno: nó de expressão nulo\n");
        exit(1);
    }

    switch (node->kind) {
        case NODE_KIND_ADDR:
            return gen_addr(node->expr);

        case NODE_KIND_DEREF: {
            int addr = gen_expr(node->expr);
            if (node->ty->size > 8) return addr;
            int dest = new_vreg();
            IRInst *inst = new_inst(IR_LOAD);
            inst->dest = op_vreg(dest);
            inst->src1 = op_vreg(addr); // Load from address in vreg
            inst->cast_to_type = node->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_INDEX: {
            int addr = gen_addr(node);
            if (node->ty->size > 8) return addr;
            int dest = new_vreg();
            IRInst *inst = new_inst(IR_LOAD);
            inst->dest = op_vreg(dest);
            inst->src1 = op_vreg(addr); // Load from address in vreg
            inst->cast_to_type = node->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_ASSIGN: {
            int lhs_addr = gen_addr(node->lhs);

            if (node->rhs->kind == NODE_KIND_FNCALL || node->rhs->kind == NODE_KIND_SYSCALL) {
                IRInst *ctx = new_inst(IR_SET_CTX);
                ctx->src1 = op_vreg(lhs_addr);
                ctx->line = node->tok ? node->tok->line : 0;
                emit(ctx);
            }

            int rhs_val = gen_expr(node->rhs);

            if (node->ty->size > 8) {
                IRInst *inst = new_inst(IR_COPY);
                inst->dest = op_vreg(lhs_addr);
                inst->src1 = op_vreg(rhs_val);
                inst->src2 = op_imm(node->ty->size);
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
                return lhs_addr; // Return address as result of assignment
            }

            IRInst *inst = new_inst(IR_STORE);
            inst->dest = op_vreg(lhs_addr);
            inst->src1 = op_vreg(rhs_val);
            inst->cast_to_type = node->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return rhs_val;
        }

        case NODE_KIND_MEMBER_ACCESS: {
            int addr = gen_addr(node);
            if (node->ty->size > 8) return addr;
            int dest = new_vreg();
            IRInst *inst = new_inst(IR_LOAD);
            inst->dest = op_vreg(dest);
            inst->src1 = op_vreg(addr); // Load from address in vreg
            inst->cast_to_type = node->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_NUM:
            if (node->ty == type_f64) {
                 // Bitcast double to int64 for storage in immediate
                 long val;
                 memcpy(&val, &node->fval, sizeof(double));
                 return emit_imm(val, node->tok ? node->tok->line : 0);
            }
            return emit_imm(node->val, node->tok ? node->tok->line : 0);

        case NODE_KIND_STRING_LITERAL: {
            int dest = new_vreg();
            char name[32];
            sprintf(name, ".LC%ld", node->val);
            IRInst *inst = new_inst(IR_ADDR_GLOBAL);
            inst->dest = op_vreg(dest);
            inst->global_name = strdup(name);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_ADD: {
            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind == NODE_KIND_NUM) {
                return emit_imm(node->lhs->val + node->rhs->val, node->tok ? node->tok->line : 0);
            }
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            int line = node->tok ? node->tok->line : 0;

            // Pointer Arithmetic: ptr + int
            if (node->lhs->ty->kind == TY_PTR && (node->rhs->ty->kind == TY_I32 || node->rhs->ty->kind == TY_I64)) {
                int size = node->lhs->ty->base->size;
                if (size > 1) {
                    int size_reg = emit_imm(size, line);
                    rhs = emit_binary(IR_MUL, rhs, size_reg, line);
                }
                return emit_binary(IR_ADD, lhs, rhs, line);
            }
            // Pointer Arithmetic: int + ptr
            if (node->rhs->ty->kind == TY_PTR && (node->lhs->ty->kind == TY_I32 || node->lhs->ty->kind == TY_I64)) {
                int size = node->rhs->ty->base->size;
                if (size > 1) {
                    int size_reg = emit_imm(size, line);
                    lhs = emit_binary(IR_MUL, lhs, size_reg, line);
                }
                return emit_binary(IR_ADD, lhs, rhs, line);
            }

            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, line);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, line);
            }
            // Float addition
            if (node->ty->kind == TY_F32 || node->ty->kind == TY_F64) {
                return emit_binary(IR_FADD, lhs, rhs, line);
            }
            return emit_binary(IR_ADD, lhs, rhs, line);
        }

        case NODE_KIND_SUBTRACTION: {
            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind == NODE_KIND_NUM) {
                return emit_imm(node->lhs->val - node->rhs->val, node->tok ? node->tok->line : 0);
            }
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            int line = node->tok ? node->tok->line : 0;

            // Pointer Arithmetic: ptr - int
            if (node->lhs->ty->kind == TY_PTR && (node->rhs->ty->kind == TY_I32 || node->rhs->ty->kind == TY_I64)) {
                int size = node->lhs->ty->base->size;
                if (size > 1) {
                    int size_reg = emit_imm(size, line);
                    rhs = emit_binary(IR_MUL, rhs, size_reg, line);
                }
                return emit_binary(IR_SUB, lhs, rhs, line);
            }
            
            // Pointer Arithmetic: ptr - ptr
            if (node->lhs->ty->kind == TY_PTR && node->rhs->ty->kind == TY_PTR) {
                int diff = emit_binary(IR_SUB, lhs, rhs, line);
                int size = node->lhs->ty->base->size;
                if (size > 1) {
                    int size_reg = emit_imm(size, line);
                    return emit_binary(IR_DIV, diff, size_reg, line);
                }
                return diff;
            }

            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, line);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, line);
            }
            // Float subtraction
            if (node->ty->kind == TY_F32 || node->ty->kind == TY_F64) {
                return emit_binary(IR_FSUB, lhs, rhs, line);
            }
            return emit_binary(IR_SUB, lhs, rhs, line);
        }

        case NODE_KIND_MULTIPLIER: {
            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind == NODE_KIND_NUM) {
                return emit_imm(node->lhs->val * node->rhs->val, node->tok ? node->tok->line : 0);
            }
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            // Float multiplication
            if (node->ty->kind == TY_F32 || node->ty->kind == TY_F64) {
                return emit_binary(IR_FMUL, lhs, rhs, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_MUL, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_DIVIDER: {
            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind == NODE_KIND_NUM) {
                if (node->rhs->val == 0) {
                    fprintf(stderr, "Erro: divisão por zero na linha %d\n", node->tok ? node->tok->line : 0);
                    exit(1);
                }
                return emit_imm(node->lhs->val / node->rhs->val, node->tok ? node->tok->line : 0);
            }
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            // Float division
            if (node->ty->kind == TY_F32 || node->ty->kind == TY_F64) {
                return emit_binary(IR_FDIV, lhs, rhs, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_DIV, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_MOD: {
            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind == NODE_KIND_NUM) {
                if (node->rhs->val == 0) {
                    fprintf(stderr, "Erro: módulo por zero na linha %d\n", node->tok ? node->tok->line : 0);
                    exit(1);
                }
                return emit_imm(node->lhs->val % node->rhs->val, node->tok ? node->tok->line : 0);
            }
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_MOD, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_EQ: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_EQ, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_NE: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_NE, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_LT: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_LT, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_LE: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_LE, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_GT: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_GT, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_GE: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_GE, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_SHL: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_SHL, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_SHR: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_SHR, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_BITWISE_AND: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_AND, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_BITWISE_OR: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_OR, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_BITWISE_XOR: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_XOR, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_BITWISE_NOT: {
            int src = gen_expr(node->expr);
            int dest = new_vreg();
            IRInst *inst = new_inst(IR_NOT);
            inst->dest = op_vreg(dest);
            inst->src1 = op_vreg(src);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_LOGICAL_NOT: {
            int src = gen_expr(node->expr);
            int zero = emit_imm(0, node->tok ? node->tok->line : 0);
            return emit_binary(IR_EQ, src, zero, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_NEG: {
            int src = gen_expr(node->expr);
            int dest = new_vreg();
            // Float negation
            IROp neg_op = IR_NEG;
            if (node->expr->ty->kind == TY_F32 || node->expr->ty->kind == TY_F64) {
                neg_op = IR_FNEG;
            }
            IRInst *inst = new_inst(neg_op);
            inst->dest = op_vreg(dest);
            inst->src1 = op_vreg(src);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_LOGICAL_AND: {
            int false_label = new_label();
            int end_label = new_label();
            int dest = new_vreg();

            int lhs = gen_expr(node->lhs);
            IRInst *jz1 = new_inst(IR_JZ);
            jz1->src1 = op_vreg(lhs);
            jz1->dest = op_label(false_label);
            jz1->line = node->tok ? node->tok->line : 0;
            emit(jz1);

            int rhs = gen_expr(node->rhs);
            IRInst *jz2 = new_inst(IR_JZ);
            jz2->src1 = op_vreg(rhs);
            jz2->dest = op_label(false_label);
            jz2->line = node->tok ? node->tok->line : 0;
            emit(jz2);

            // True case
            IRInst *mov_true = new_inst(IR_IMM);
            mov_true->dest = op_vreg(dest);
            mov_true->src1 = op_imm(1);
            emit(mov_true);
            
            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(end_label);
            emit(jmp);

            // False label
            IRInst *lbl_false = new_inst(IR_LABEL);
            lbl_false->dest = op_label(false_label);
            emit(lbl_false);

            // False case
            IRInst *mov_false = new_inst(IR_IMM);
            mov_false->dest = op_vreg(dest);
            mov_false->src1 = op_imm(0);
            emit(mov_false);

            // End label
            IRInst *lbl_end = new_inst(IR_LABEL);
            lbl_end->dest = op_label(end_label);
            emit(lbl_end);

            return dest;
        }

        case NODE_KIND_LOGICAL_OR: {
            int true_label = new_label();
            int end_label = new_label();
            int dest = new_vreg();

            int lhs = gen_expr(node->lhs);
            IRInst *jnz1 = new_inst(IR_JNZ);
            jnz1->src1 = op_vreg(lhs);
            jnz1->dest = op_label(true_label);
            jnz1->line = node->tok ? node->tok->line : 0;
            emit(jnz1);

            int rhs = gen_expr(node->rhs);
            IRInst *jnz2 = new_inst(IR_JNZ);
            jnz2->src1 = op_vreg(rhs);
            jnz2->dest = op_label(true_label);
            jnz2->line = node->tok ? node->tok->line : 0;
            emit(jnz2);

            // False case
            IRInst *mov_false = new_inst(IR_IMM);
            mov_false->dest = op_vreg(dest);
            mov_false->src1 = op_imm(0);
            emit(mov_false);

            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(end_label);
            emit(jmp);

            // True label
            IRInst *lbl_true = new_inst(IR_LABEL);
            lbl_true->dest = op_label(true_label);
            emit(lbl_true);

            // True case
            IRInst *mov_true = new_inst(IR_IMM);
            mov_true->dest = op_vreg(dest);
            mov_true->src1 = op_imm(1);
            emit(mov_true);

            // End label
            IRInst *lbl_end = new_inst(IR_LABEL);
            lbl_end->dest = op_label(end_label);
            emit(lbl_end);

            return dest;
        }

        case NODE_KIND_IDENTIFIER: {
            int reg = new_vreg();
            if (node->var && node->var->is_global) {
                // Load from global: 1. Get address, 2. Load from address
                int addr = new_vreg();
                IRInst *addr_inst = new_inst(IR_ADDR_GLOBAL);
                addr_inst->dest = op_vreg(addr);
                if (node->var->asm_name) {
                    addr_inst->global_name = strdup(node->var->asm_name);
                } else {
                    addr_inst->global_name = strdup(node->name);
                }
                addr_inst->line = node->tok ? node->tok->line : 0;
                emit(addr_inst);

                if (node->ty->size > 8) return addr;

                IRInst *inst = new_inst(IR_LOAD);
                inst->dest = op_vreg(reg);
                inst->src1 = op_vreg(addr);
                inst->cast_to_type = node->ty;
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
            } else {
                if (node->ty->size > 8) {
                    IRInst *inst = new_inst(IR_ADDR);
                    inst->dest = op_vreg(reg);
                    inst->src1 = op_imm(node->offset);
                    inst->line = node->tok ? node->tok->line : 0;
                    emit(inst);
                    return reg;
                }
                IRInst *inst = new_inst(IR_LOAD);
                inst->dest = op_vreg(reg);
                inst->src1 = op_imm(node->offset);
                inst->cast_to_type = node->ty;
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
            }
            return reg;
        }

        case NODE_KIND_CAST: {
            int src = gen_expr(node->expr);
            return emit_cast_from(src, node->expr->ty, node->ty, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_FNCALL: {
            int arg_count = 0;
            for (Node *arg = node->args; arg; arg = arg->next) arg_count++;

            int sret_addr_reg = -1;
            if (node->ty->size > 8) {
                // Allocate stack space for return value
                int offset = ctx.current_func->alloc_stack_size;
                ctx.current_func->alloc_stack_size += node->ty->size;
                // Align to 16 bytes
                if (ctx.current_func->alloc_stack_size % 16 != 0) {
                    ctx.current_func->alloc_stack_size += 16 - (ctx.current_func->alloc_stack_size % 16);
                }

                sret_addr_reg = new_vreg();
                IRInst *inst = new_inst(IR_GET_ALLOC_ADDR);
                inst->dest = op_vreg(sret_addr_reg);
                inst->src1 = op_imm(offset);
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);

                arg_count++; // Add hidden argument
            }

            int *arg_vregs = calloc(arg_count, sizeof(int));
            int i = 0;
            if (sret_addr_reg != -1) {
                arg_vregs[i++] = sret_addr_reg;
            }
            for (Node *arg = node->args; arg; arg = arg->next) {
                arg_vregs[i++] = gen_expr(arg);
            }

            int stack_args = (arg_count > 6) ? (arg_count - 6) : 0;
            int padding = (stack_args % 2 != 0) ? 1 : 0;

            if (padding) {
                IRInst *align = new_inst(IR_ASM);
                align->asm_str = strdup("sub rsp, 8");
                align->line = node->tok ? node->tok->line : 0;
                emit(align);
            }

            // Emit stack args (Arg N ... Arg 6) in reverse order
            for (int j = arg_count - 1; j >= 6; j--) {
                emit_set_arg(j, arg_vregs[j], node->tok ? node->tok->line : 0);
            }

            // Emit register args (Arg 0 ... Arg 5)
            for (int j = 0; j < arg_count && j < 6; j++) {
                emit_set_arg(j, arg_vregs[j], node->tok ? node->tok->line : 0);
            }

            free(arg_vregs);

            int ret_reg = emit_call(node->name, node->tok ? node->tok->line : 0);

            if (stack_args > 0) {
                IRInst *cleanup = new_inst(IR_ASM);
                char buf[32];
                snprintf(buf, sizeof(buf), "add rsp, %d", (stack_args + padding) * 8);
                cleanup->asm_str = strdup(buf);
                cleanup->line = node->tok ? node->tok->line : 0;
                emit(cleanup);
            }

            if (sret_addr_reg != -1) {
                return sret_addr_reg;
            }
            return ret_reg;
        }

        case NODE_KIND_SYSCALL: {
            int arg_count = 0;
            for (Node *arg = node->args; arg; arg = arg->next) arg_count++;

            if (arg_count > 7) { // ID + 6 args
                fprintf(stderr, "Erro: syscall suporta maximo 7 argumentos (ID + 6 args)\n");
                exit(1);
            }

            int args_vregs[7];
            int count = 0;

            for (Node *arg = node->args; arg; arg = arg->next) {
                args_vregs[count++] = gen_expr(arg);
            }

            // Emit IR_SET_SYS_ARG for each argument
            for (int i = 0; i < count; i++) {
                IRInst *arg = new_inst(IR_SET_SYS_ARG);
                arg->dest = op_imm(i); // 0=RAX(ID), 1=RDI, etc.
                arg->src1 = op_vreg(args_vregs[i]);
                arg->line = node->tok ? node->tok->line : 0;
                emit(arg);
            }

            int dest = new_vreg();
            IRInst *inst = new_inst(IR_SYSCALL);
            inst->dest = op_vreg(dest);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);

            return dest;
        }



        default:
            fprintf(stderr, "Erro interno: tipo de nó não suportado em expressão: %d\n", node->kind);
            exit(1);
    }
}

static void gen_stmt_single(Node *node);

static void gen_stmt(Node *node) {
    while (node) {
        if (!ctx.has_returned) {
            gen_stmt_single(node);
        }
        node = node->next;
    }
}

static void gen_stmt_single(Node *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case NODE_KIND_RETURN: {
            if (node->expr) {
                int result_reg = gen_expr(node->expr);
                if (ctx.has_sret) {
                    IRInst *inst = new_inst(IR_COPY);
                    inst->dest = op_vreg(ctx.sret_vreg);
                    inst->src1 = op_vreg(result_reg);
                    inst->src2 = op_imm(node->expr->ty->size);
                    inst->line = node->tok ? node->tok->line : 0;
                    emit(inst);
                    emit_return(ctx.sret_vreg, node->tok ? node->tok->line : 0);
                } else {
                    emit_return(result_reg, node->tok ? node->tok->line : 0);
                }
            } else {
                emit_return(-1, node->tok ? node->tok->line : 0);
            }
            ctx.has_returned = 1;
            break;
        }

        case NODE_KIND_EXPR_STMT: {
            gen_expr(node->expr);
            break;
        }

        case NODE_KIND_LET: {
            if (node->init_expr && (node->init_expr->kind == NODE_KIND_FNCALL || node->init_expr->kind == NODE_KIND_SYSCALL)) {
                IRInst *ctx = new_inst(IR_SET_CTX);
                ctx->src1 = op_imm(0);
                ctx->line = node->tok ? node->tok->line : 0;
                emit(ctx);
            }

            if (node->init_expr) {
                int val_reg = gen_expr(node->init_expr);
                
                if (node->ty->size > 8) {
                    int dest_addr = new_vreg();
                    IRInst *addr_inst = new_inst(IR_ADDR);
                    addr_inst->dest = op_vreg(dest_addr);
                    addr_inst->src1 = op_imm(node->offset);
                    addr_inst->line = node->tok ? node->tok->line : 0;
                    emit(addr_inst);

                    IRInst *inst = new_inst(IR_COPY);
                    inst->dest = op_vreg(dest_addr);
                    inst->src1 = op_vreg(val_reg);
                    inst->src2 = op_imm(node->ty->size);
                    inst->line = node->tok ? node->tok->line : 0;
                    emit(inst);
                } else {
                    IRInst *inst = new_inst(IR_STORE);
                    inst->dest = op_imm(node->offset);
                    inst->src1 = op_vreg(val_reg);
                    inst->cast_to_type = node->ty;
                    inst->line = node->tok ? node->tok->line : 0;
                    emit(inst);
                }
            }
            break;
        }

        case NODE_KIND_BLOCK: {
            gen_stmt(node->stmts);
            break;
        }

        case NODE_KIND_IF: {
            int saved_ret = ctx.has_returned;
            
            int else_label = new_label();
            int end_label = new_label();
            int cond_reg = gen_expr(node->if_stmt.cond);
            IRInst *jz = new_inst(IR_JZ);
            jz->src1 = op_vreg(cond_reg);
            jz->dest = op_label(else_label);
            jz->line = node->tok ? node->tok->line : 0;
            emit(jz);
            
            gen_stmt(node->if_stmt.then_b);
            int then_ret = ctx.has_returned;
            ctx.has_returned = saved_ret; // Reset for else block
            
            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(end_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);
            
            IRInst *else_lbl = new_inst(IR_LABEL);
            else_lbl->dest = op_label(else_label);
            emit(else_lbl);
            
            int else_ret = 0;
            if (node->if_stmt.else_b) {
                gen_stmt(node->if_stmt.else_b);
                else_ret = ctx.has_returned;
            }
            
            IRInst *end_lbl = new_inst(IR_LABEL);
            end_lbl->dest = op_label(end_label);
            emit(end_lbl);
            
            if (then_ret && else_ret) {
                ctx.has_returned = 1;
            } else {
                ctx.has_returned = saved_ret;
            }
            break;
        }
        case NODE_KIND_ASM:
            IRInst *inst = new_inst(IR_ASM);
            inst->asm_str = strdup(node->name);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            ctx.has_returned = 1;
            break;
        case NODE_KIND_WHILE: {
            int saved_ret = ctx.has_returned;
            int start_label = new_label();
            int end_label = new_label();

            push_loop(start_label, end_label, start_label);

            IRInst *start_lbl = new_inst(IR_LABEL);
            start_lbl->dest = op_label(start_label);
            emit(start_lbl);

            int cond_reg = gen_expr(node->while_stmt.cond);

            IRInst *jz = new_inst(IR_JZ);
            jz->src1 = op_vreg(cond_reg);
            jz->dest = op_label(end_label);
            jz->line = node->tok ? node->tok->line : 0;
            emit(jz);

            gen_stmt_single(node->while_stmt.body);

            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(start_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);

            IRInst *end_lbl = new_inst(IR_LABEL);
            end_lbl->dest = op_label(end_label);
            emit(end_lbl);
            
            pop_loop();
            ctx.has_returned = saved_ret; // Loops might not execute
            break;
        }

        case NODE_KIND_DO_WHILE: {
            int saved_ret = ctx.has_returned;
            int start_label = new_label();
            int end_label = new_label();
            int cond_label = new_label();

            push_loop(start_label, end_label, cond_label);

            IRInst *start_lbl = new_inst(IR_LABEL);
            start_lbl->dest = op_label(start_label);
            emit(start_lbl);

            gen_stmt_single(node->do_while_stmt.body);

            IRInst *cond_lbl = new_inst(IR_LABEL);
            cond_lbl->dest = op_label(cond_label);
            emit(cond_lbl);

            int cond_reg = gen_expr(node->do_while_stmt.cond);

            IRInst *jnz = new_inst(IR_JNZ);
            jnz->src1 = op_vreg(cond_reg);
            jnz->dest = op_label(start_label);
            jnz->line = node->tok ? node->tok->line : 0;
            emit(jnz);

            IRInst *end_lbl = new_inst(IR_LABEL);
            end_lbl->dest = op_label(end_label);
            emit(end_lbl);

            pop_loop();
            ctx.has_returned = saved_ret;
            break;
        }

        case NODE_KIND_FOR: {
            int start_label = new_label();
            int end_label = new_label();
            int step_label = new_label();

            // Init
            if (node->for_stmt.init) {
                if (node->for_stmt.init->kind == NODE_KIND_LET) {
                    gen_stmt_single(node->for_stmt.init);
                } else {
                    gen_expr(node->for_stmt.init);
                }
            }

            push_loop(start_label, end_label, step_label);

            IRInst *start_lbl = new_inst(IR_LABEL);
            start_lbl->dest = op_label(start_label);
            emit(start_lbl);

            // Cond
            if (node->for_stmt.cond) {
                int cond_reg = gen_expr(node->for_stmt.cond);
                IRInst *jz = new_inst(IR_JZ);
                jz->src1 = op_vreg(cond_reg);
                jz->dest = op_label(end_label);
                jz->line = node->tok ? node->tok->line : 0;
                emit(jz);
            }

            gen_stmt_single(node->for_stmt.body);

            IRInst *step_lbl = new_inst(IR_LABEL);
            step_lbl->dest = op_label(step_label);
            emit(step_lbl);

            // Step
            if (node->for_stmt.step) {
                gen_expr(node->for_stmt.step);
            }

            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(start_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);

            IRInst *end_lbl = new_inst(IR_LABEL);
            end_lbl->dest = op_label(end_label);
            emit(end_lbl);

            pop_loop();
            break;
        }

        case NODE_KIND_BREAK: {
            if (ctx.loop_depth == 0) {
                fprintf(stderr, "Erro: 'break' fora de loop\n");
                exit(1);
            }
            int end_label = ctx.loop_stack[ctx.loop_depth - 1].end_label;
            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(end_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);
            break;
        }

        case NODE_KIND_CONTINUE: {
            if (ctx.loop_depth == 0) {
                fprintf(stderr, "Erro: 'continue' fora de loop\n");
                exit(1);
            }
            int continue_label = ctx.loop_stack[ctx.loop_depth - 1].continue_label;
            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(continue_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);
            break;
        }
        case NODE_KIND_ASSIGN:
            gen_expr(node);
            break;

        default:
            fprintf(stderr, "Erro interno: tipo de statement não suportado: %d\n", node->kind);
            exit(1);
    }
}

static void mark_reachable(IRProgram *prog, IRFunction *func) {
    if (!func || func->is_reachable) return;
    func->is_reachable = 1;
    
    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        if (inst->op == IR_CALL && inst->call_target_name) {
            for (IRFunction *f = prog->functions; f; f = f->next) {
                char *fname = f->asm_name ? f->asm_name : f->name;
                if (strcmp(fname, inst->call_target_name) == 0) {
                    mark_reachable(prog, f);
                    break;
                }
            }
        } else if (inst->op == IR_ADDR_GLOBAL && inst->global_name) {
             for (IRGlobal *g = prog->globals; g; g = g->next) {
                if (strcmp(g->name, inst->global_name) == 0) {
                    g->is_reachable = 1;
                    break;
                }
            }
        }
    }
}

static long eval_const_expr(Node *node) {
    if (node->kind == NODE_KIND_NUM) {
        return node->val;
    }
    if (node->kind == NODE_KIND_ADD) {
        return eval_const_expr(node->lhs) + eval_const_expr(node->rhs);
    }
    if (node->kind == NODE_KIND_SUBTRACTION) {
        return eval_const_expr(node->lhs) - eval_const_expr(node->rhs);
    }
    if (node->kind == NODE_KIND_MULTIPLIER) {
        return eval_const_expr(node->lhs) * eval_const_expr(node->rhs);
    }
    if (node->kind == NODE_KIND_DIVIDER) {
        long rhs = eval_const_expr(node->rhs);
        if (rhs == 0) {
            fprintf(stderr, "Erro: divisão por zero em expressão constante.\n");
            exit(1);
        }
        return eval_const_expr(node->lhs) / rhs;
    }
    fprintf(stderr, "Erro: expressão não constante na inicialização global.\n");
    exit(1);
}

IRProgram *gen_ir(Node *ast) {
    static int depth = 0;
    depth++;
    
    IRProgram *prog = calloc(1, sizeof(IRProgram));
    IRFunction **func_ptr = &prog->functions;
    IRGlobal **global_ptr = &prog->globals;

    for (Node *node = ast; node; node = node->next) {
        if (node->kind == NODE_KIND_LET) {
            // Global variable
            // Global variable
            IRGlobal *glob = calloc(1, sizeof(IRGlobal));
            if (node->var && node->var->asm_name) {
                glob->name = strdup(node->var->asm_name);
            } else {
                glob->name = strdup(node->name);
            }
            glob->size = node->ty->size;
            
            // Handle initialization
            if (node->init_expr) {
                glob->init_value = eval_const_expr(node->init_expr);
                glob->is_initialized = 1;
            } else {
                glob->is_initialized = 0;
            }

            *global_ptr = glob;
            global_ptr = &glob->next;
            continue;
        }

        if (node->kind != NODE_KIND_FN) {
            continue;
        }

        // Skip generic templates (uninstantiated)
        if (node->generic_param_count > 0) {
            continue;
        }

        Node *fn_node = node;

        // Processar cada função com contexto limpo
        ctx.inst_head = NULL;
        ctx.inst_tail = NULL;
        ctx.vreg_count = 0;
        ctx.loop_depth = 0;
        ctx.has_sret = 0;
        ctx.has_returned = 0;
        // Labels are global to ensure uniqueness across functions

        IRFunction *func = calloc(1, sizeof(IRFunction));
        func->name = strdup(fn_node->name);
        func->asm_name = strdup(fn_node->name);
        func->instructions = NULL;
        
        int param_count = 0;
        for (Node *param = fn_node->params; param; param = param->next) {
            param_count++;
        }
        func->num_args = param_count;
        
        ctx.current_func = func;

        if (fn_node->body && fn_node->body->kind == NODE_KIND_BLOCK) {
            // Check for SRET (Struct Return)
            int arg_idx = 0;
            if (fn_node->return_type && fn_node->return_type->size > 8) {
                ctx.has_sret = 1;
                ctx.sret_vreg = emit_get_arg(arg_idx++, fn_node->tok ? fn_node->tok->line : 0);
            } else {
                ctx.has_sret = 0;
            }

            // Gerar código para recuperar argumentos e colocar nas variáveis locais
            for (Node *param = fn_node->params; param; param = param->next) {
                int arg_val = emit_get_arg(arg_idx++, param->tok ? param->tok->line : 0);
                IRInst *store = new_inst(IR_STORE);
                store->dest = op_imm(param->offset);
                store->src1 = op_vreg(arg_val);
                store->cast_to_type = param->ty;
                store->line = param->tok ? param->tok->line : 0;
                emit(store);
            }
            gen_stmt(fn_node->body->stmts);
            
            // Sempre emitir um retorno implícito (0) no final da função.
            // Isso garante que funções void tenham epílogo gerado no codegen.
            int zero = emit_imm(0, 0);
            emit_return(zero, 0);
        } else {
            fprintf(stderr, "Erro: função '%s' deve ter um bloco\n", fn_node->name);
            exit(1);
        }

        func->instructions = ctx.inst_head;
        func->vreg_count = ctx.vreg_count;
        func->label_count = global_label_count;

        // Adicionar à lista de funções
        *func_ptr = func;
        func_ptr = &func->next;

        // Identificar main
        if (strcmp(fn_node->name, "main") == 0) {
            prog->main_func = func;
        }
    }

    // Handle NODE_KIND_USE at top level
    for (Node *node = ast; node; node = node->next) {
        if (node->kind == NODE_KIND_USE) {
            IRProgram *mod_prog = gen_ir(node->body);
            
            // Merge functions
            if (mod_prog->functions) {
                
                if (prog->functions) {
                    IRFunction *last = prog->functions;
                    while (last->next) last = last->next;
                    last->next = mod_prog->functions;
                } else {
                    prog->functions = mod_prog->functions;
                }
            }

            // Merge globals
            if (mod_prog->globals) {
                if (prog->globals) {
                    IRGlobal *last = prog->globals;
                    while (last->next) last = last->next;
                    last->next = mod_prog->globals;
                } else {
                    prog->globals = mod_prog->globals;
                }
            }
            
            // Strings are global (handled by get_strings())
            
            free(mod_prog); // Free container
        }
    }

    // Scan for main function after all merges (only at top level)
    if (depth == 1 && !prog->main_func) {
        for (IRFunction *f = prog->functions; f; f = f->next) {
            if (strcmp(f->name, "main") == 0) {
                prog->main_func = f;
                break;
            }
        }
    }

    if (depth == 1 && !prog->main_func) {
        fprintf(stderr, "Erro: função main não encontrada\n");
        exit(1);
    }

    prog->strings = get_strings();

    if (prog->main_func) {
        mark_reachable(prog, prog->main_func);
    }

    depth--;
    return prog;
}

void ir_free(IRProgram *prog) {
    if (!prog) return;

    IRFunction *func = prog->functions;
    while (func) {
        IRInst *inst = func->instructions;
        while (inst) {
            IRInst *next = inst->next;
            if (inst->asm_str) free(inst->asm_str);
            if (inst->call_target_name) free(inst->call_target_name);
            if (inst->global_name) free(inst->global_name);
            free(inst);
            inst = next;
        }
        IRFunction *next_func = func->next;
        if (func->name) free(func->name);
        if (func->asm_name) free(func->asm_name);
        free(func);
        func = next_func;
    }
    
    IRGlobal *glob = prog->globals;
    while (glob) {
        IRGlobal *next = glob->next;
        free(glob->name);
        free(glob);
        glob = next;
    }
    free(prog);
}

static void print_operand(Operand op) {
    switch (op.type) {
        case OPERAND_NONE:
            break;
        case OPERAND_VREG:
            printf("v%d", op.vreg);
            break;
        case OPERAND_IMM:
            printf("%ld", op.imm);
            break;
        case OPERAND_LABEL:
            printf("L%d", op.label);
            break;
    }
}

void ir_print(IRProgram *prog) {
    if (!prog) {
        printf("(programa IR vazio)\n");
        return;
    }

    printf("=== IR GERADO ===\n\n");

    for (IRGlobal *g = prog->globals; g; g = g->next) {
        printf("global %s: size=%d, init=%d, val=%ld\n", g->name, g->size, g->is_initialized, g->init_value);
    }
    printf("\n");

    for (IRFunction *func = prog->functions; func; func = func->next) {
        printf("function %s():\n", func->name);
        printf("  vregs: %d, labels: %d\n\n", func->vreg_count, func->label_count);

        int inst_num = 0;
        for (IRInst *inst = func->instructions; inst; inst = inst->next) {
            printf("  %3d: ", inst_num++);

            printf("%-6s ", IR_OP_NAMES[inst->op]);

            switch (inst->op) {
                case IR_IMM:
                    print_operand(inst->dest);
                    printf(" = ");
                    print_operand(inst->src1);
                    break;

                case IR_MOV:
                    print_operand(inst->dest);
                    printf(" = ");
                    print_operand(inst->src1);
                    break;

                case IR_ADD:
                case IR_SUB:
                case IR_MUL:
                case IR_DIV:
                case IR_EQ:
                case IR_NE:
                case IR_LT:
                case IR_LE:
                case IR_GT:
                case IR_GE:
                    print_operand(inst->dest);
                    printf(" = ");
                    print_operand(inst->src1);
                    printf(", ");
                    print_operand(inst->src2);
                    break;

                case IR_NEG:
                    print_operand(inst->dest);
                    printf(" = -");
                    print_operand(inst->src1);
                    break;

                case IR_RET:
                    print_operand(inst->src1);
                    break;

                case IR_LABEL:
                    printf(":");
                    print_operand(inst->dest);
                    break;

                case IR_JMP:
                    print_operand(inst->dest);
                    break;

                case IR_JZ:
                case IR_JNZ:
                    print_operand(inst->dest);
                    printf(" if ");
                    print_operand(inst->src1);
                    break;

                case IR_LOAD:
                    print_operand(inst->dest);
                    printf(" = [rbp-");
                    print_operand(inst->src1);
                    printf("-8]");
                    break;

                case IR_STORE:
                    printf("[rbp-");
                    print_operand(inst->dest);
                    printf("-8] = ");
                    print_operand(inst->src1);
                    break;
                case IR_ASM:
                    printf("\"%s\"", inst->asm_str ? inst->asm_str : "");
                    break;

                case IR_CAST:
                    print_operand(inst->dest);
                    printf(" = CAST ");
                    print_operand(inst->src1);
                    if (inst->cast_to_type) {
                        printf(" to [size=%d]", inst->cast_to_type->size);
                    }
                    break;

                case IR_CALL:
                    print_operand(inst->dest);
                    printf(" = CALL %s", inst->call_target_name ? inst->call_target_name : "NULL");
                    break;

                case IR_ADDR_GLOBAL:
                    print_operand(inst->dest);
                    printf(" = ADDR_GLOBAL %s", inst->global_name ? inst->global_name : "NULL");
                    break;

                default:
                    printf("(?)");
                    break;
            }

            if (inst->line > 0) {
                printf("  ; linha %d", inst->line);
            }

            printf("\n");
        }

        printf("\n");
    }

    printf("=================\n");
}
