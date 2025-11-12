#include "ir.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    IRFunction *current_func;
    IRInst *inst_head;
    IRInst *inst_tail;
    int vreg_count;
    int label_count;
} IRGenContext;

static IRGenContext ctx;

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
    return ctx.label_count++;
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

static int emit_unary(IROp op, int src_reg, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(op);
    inst->dest = op_vreg(dest);
    inst->src1 = op_vreg(src_reg);
    inst->line = line;
    emit(inst);
    return dest;
}

static void emit_return(int src_reg, int line) {
    IRInst *inst = new_inst(IR_RET);
    inst->src1 = op_vreg(src_reg);
    inst->line = line;
    emit(inst);
}

static int gen_expr(Node *node);

static int gen_expr(Node *node) {
    if (!node) {
        fprintf(stderr, "Erro interno: nó de expressão nulo\n");
        exit(1);
    }

    switch (node->kind) {
        case NODE_KIND_NUM:
            return emit_imm(node->val, node->tok ? node->tok->line : 0);

        case NODE_KIND_ADD: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            return emit_binary(IR_ADD, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_SUBTRACTION: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            return emit_binary(IR_SUB, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_MULTIPLIER: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            return emit_binary(IR_MUL, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_DIVIDER: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            return emit_binary(IR_DIV, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_IDENTIFIER:
            fprintf(stderr, "Erro: variáveis ainda não implementadas\n");
            exit(1);

        case NODE_KIND_ASSIGN:
            fprintf(stderr, "Erro: atribuição ainda não implementada\n");
            exit(1);

        default:
            fprintf(stderr, "Erro interno: tipo de nó não suportado em expressão: %d\n", node->kind);
            exit(1);
    }
}

static void gen_stmt(Node *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case NODE_KIND_RETURN: {
            int result_reg = gen_expr(node->expr);
            emit_return(result_reg, node->tok ? node->tok->line : 0);
            break;
        }

        case NODE_KIND_EXPR_STMT: {
            gen_expr(node->expr);
            break;
        }

        default:
            fprintf(stderr, "Erro interno: tipo de statement não suportado: %d\n", node->kind);
            exit(1);
    }

    gen_stmt(node->next);
}

IRProgram *gen_ir(Node *ast) {
    memset(&ctx, 0, sizeof(ctx));

    IRProgram *prog = calloc(1, sizeof(IRProgram));

    // ast deve ser um NODE_KIND_FN
    if (!ast || ast->kind != NODE_KIND_FN) {
        fprintf(stderr, "Erro: AST deve ser uma função\n");
        exit(1);
    }

    IRFunction *func = calloc(1, sizeof(IRFunction));
    strcpy(func->name, ast->name);
    ctx.current_func = func;

    // Processar o body da função (NODE_KIND_BLOCK)
    if (ast->body && ast->body->kind == NODE_KIND_BLOCK) {
        gen_stmt(ast->body->stmts);
    } else {
        fprintf(stderr, "Erro: função deve ter um bloco\n");
        exit(1);
    }

    func->instructions = ctx.inst_head;
    func->vreg_count = ctx.vreg_count;
    func->label_count = ctx.label_count;

    prog->functions = func;
    prog->main_func = func;

    return prog;
}

void ir_free(IRProgram *prog) {
    if (!prog) return;

    IRFunction *func = prog->functions;
    while (func) {
        IRInst *inst = func->instructions;
        while (inst) {
            IRInst *next = inst->next;
            free(inst);
            inst = next;
        }
        IRFunction *next_func = func->next;
        free(func);
        func = next_func;
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
