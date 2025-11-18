#include "ir.h"
#include "parser.h"
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

static int emit_cast(int src_reg, Type *to_type, int line) {
    int dest = new_vreg();
    IRInst *inst = new_inst(IR_CAST);
    inst->dest = op_vreg(dest);
    inst->src1 = op_vreg(src_reg);
    inst->cast_to_type = to_type;
    inst->line = line;
    emit(inst);
    return dest;
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
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_ADD, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_SUBTRACTION: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_SUB, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_MULTIPLIER: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_MUL, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_DIVIDER: {
            int lhs = gen_expr(node->lhs);
            int rhs = gen_expr(node->rhs);
            if (node->lhs->ty != node->ty) {
                lhs = emit_cast(lhs, node->ty, node->tok ? node->tok->line : 0);
            }
            if (node->rhs->ty != node->ty) {
                rhs = emit_cast(rhs, node->ty, node->tok ? node->tok->line : 0);
            }
            return emit_binary(IR_DIV, lhs, rhs, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_MOD: {
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

        case NODE_KIND_IDENTIFIER: {
            int dest = new_vreg();
            IRInst *inst = new_inst(IR_LOAD);
            inst->dest = op_vreg(dest);
            inst->src1 = op_imm(node->offset);
            inst->cast_to_type = node->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            return dest;
        }

        case NODE_KIND_CAST: {
            int src = gen_expr(node->expr);
            return emit_cast(src, node->ty, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_FNCALL: {
           // int src = gen_expr(node->expr);
            return emit_call(node->name, node->tok ? node->tok->line : 0);
        }

        case NODE_KIND_ASSIGN:
            fprintf(stderr, "Erro: atribuição ainda não implementada\n");
            exit(1);

        default:
            fprintf(stderr, "Erro interno: tipo de nó não suportado em expressão: %d\n", node->kind);
            exit(1);
    }
}

static void gen_stmt_single(Node *node);

static void gen_stmt(Node *node) {
    while (node) {
        gen_stmt_single(node);
        node = node->next;
    }
}

static void gen_stmt_single(Node *node) {
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

        case NODE_KIND_LET: {
            if (node->init_expr) {
                int val_reg = gen_expr(node->init_expr);
                IRInst *inst = new_inst(IR_STORE);
                inst->dest = op_imm(node->offset);
                inst->src1 = op_vreg(val_reg);
                inst->cast_to_type = node->ty;
                inst->line = node->tok ? node->tok->line : 0;
                emit(inst);
            }
            break;
        }

        case NODE_KIND_BLOCK: {
            gen_stmt(node->stmts);
            break;
        }

        case NODE_KIND_IF: {
            int else_label = new_label();
            int end_label = new_label();
            int cond_reg = gen_expr(node->if_stmt.cond);
            IRInst *jz = new_inst(IR_JZ);
            jz->src1 = op_vreg(cond_reg);
            jz->dest = op_label(else_label);
            jz->line = node->tok ? node->tok->line : 0;
            emit(jz);
            gen_stmt(node->if_stmt.then_b);
            IRInst *jmp = new_inst(IR_JMP);
            jmp->dest = op_label(end_label);
            jmp->line = node->tok ? node->tok->line : 0;
            emit(jmp);
            IRInst *else_lbl = new_inst(IR_LABEL);
            else_lbl->dest = op_label(else_label);
            emit(else_lbl);
            if (node->if_stmt.else_b) {
                gen_stmt(node->if_stmt.else_b);
            }
            IRInst *end_lbl = new_inst(IR_LABEL);
            end_lbl->dest = op_label(end_label);
            emit(end_lbl);
            break;
        }
        case NODE_KIND_ASM:
            IRInst *inst = new_inst(IR_ASM);
            inst->asm_str = strdup(node->name);
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            break;
        case NODE_KIND_WHILE: {
            int start_label = new_label();
            int end_label = new_label();

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
            break;
        }
        case NODE_KIND_ASSIGN: {
            int val_reg = gen_expr(node->rhs);
            IRInst *inst = new_inst(IR_STORE);
            inst->dest = op_imm(node->lhs->offset);
            inst->src1 = op_vreg(val_reg);
            inst->cast_to_type = node->lhs->ty;
            inst->line = node->tok ? node->tok->line : 0;
            emit(inst);
            break;
        }

        default:
            fprintf(stderr, "Erro interno: tipo de statement não suportado: %d\n", node->kind);
            exit(1);
    }
}

IRProgram *gen_ir(Node *ast) {
    IRProgram *prog = calloc(1, sizeof(IRProgram));
    IRFunction **func_ptr = &prog->functions;

    for (Node *fn_node = ast; fn_node; fn_node = fn_node->next) {
        if (fn_node->kind != NODE_KIND_FN) {
            fprintf(stderr, "Erro: AST deve ser uma lista de funções\n");
            exit(1);
        }

        // Processar cada função com contexto limpo
        ctx.inst_head = NULL;
        ctx.inst_tail = NULL;
        ctx.vreg_count = 0;
        ctx.label_count = 0;

        IRFunction *func = calloc(1, sizeof(IRFunction));
        strcpy(func->name, fn_node->name);
        ctx.current_func = func;

        if (fn_node->body && fn_node->body->kind == NODE_KIND_BLOCK) {
            gen_stmt(fn_node->body->stmts);
        } else {
            fprintf(stderr, "Erro: função '%s' deve ter um bloco\n", fn_node->name);
            exit(1);
        }

        func->instructions = ctx.inst_head;
        func->vreg_count = ctx.vreg_count;
        func->label_count = ctx.label_count;

        // Adicionar à lista de funções
        *func_ptr = func;
        func_ptr = &func->next;

        // Identificar main
        if (strcmp(fn_node->name, "main") == 0) {
            prog->main_func = func;
        }
    }

    if (!prog->main_func) {
        fprintf(stderr, "Erro: função main não encontrada\n");
        exit(1);
    }

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
