#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

typedef struct {
    int vreg;
    int start;
    int end;
    int reg;
    int spill_loc;
    int use_count;
    int spill_cost;
} LiveInterval;

typedef struct RegState {
    int vreg;
    int dirty;
    int last_use;
} RegState;

typedef struct {
    int *successors;
    int num_successors;
} CFGNode;

typedef struct {
    LiveInterval *intervals;
    int count;
    RegState *regs;
    int *vreg_to_loc;
    int stack_slots;
    int next_spill;
} AllocCtx;

static FILE *out;
static const char *regs[] = {"rax", "rcx", "rdx", "rbx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
static const int num_regs = 14;
static const int rax_idx = 0;
static const int rcx_idx = 1;
static const int rdx_idx = 2;

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(out, "  ");
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}

static CFGNode *build_cfg(IRFunction *func, int n) {
    CFGNode *cfg = calloc(n, sizeof(CFGNode));
    int *label_to_pos = calloc(func->label_count + 1, sizeof(int));

    int pos = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next, pos++) {
        if (inst->op == IR_LABEL) {
            label_to_pos[inst->dest.label] = pos;
        }
    }

    pos = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next, pos++) {
        cfg[pos].successors = calloc(2, sizeof(int));
        cfg[pos].num_successors = 0;

        if (inst->op == IR_JMP) {
            cfg[pos].successors[0] = label_to_pos[inst->dest.label];
            cfg[pos].num_successors = 1;
        } else if (inst->op == IR_JZ || inst->op == IR_JNZ) {
            cfg[pos].successors[0] = label_to_pos[inst->dest.label];
            if (inst->next) {
                cfg[pos].successors[1] = pos + 1;
                cfg[pos].num_successors = 2;
            } else {
                cfg[pos].num_successors = 1;
            }
        } else if (inst->op == IR_RET) {
            cfg[pos].num_successors = 0;
        } else {
            if (inst->next) {
                cfg[pos].successors[0] = pos + 1;
                cfg[pos].num_successors = 1;
            }
        }
    }

    free(label_to_pos);
    return cfg;
}

static void liveness_analysis(IRFunction *func, unsigned long *live_in, unsigned long *live_out) {
    int n = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next) n++;

    CFGNode *cfg = build_cfg(func, n);

    int changed = 1;
    while (changed) {
        changed = 0;
        int pos = 0;
        for (IRInst *inst = func->instructions; inst; inst = inst->next, pos++) {
            unsigned long old_in = live_in[pos];
            unsigned long old_out = live_out[pos];

            unsigned long use = 0, def = 0;
            if (inst->src1.type == OPERAND_VREG && inst->src1.vreg < 64) use |= (1UL << inst->src1.vreg);
            if (inst->src2.type == OPERAND_VREG && inst->src2.vreg < 64) use |= (1UL << inst->src2.vreg);
            if (inst->dest.type == OPERAND_VREG && inst->dest.vreg < 64) def |= (1UL << inst->dest.vreg);

            live_out[pos] = 0;
            for (int i = 0; i < cfg[pos].num_successors; i++) {
                live_out[pos] |= live_in[cfg[pos].successors[i]];
            }

            live_in[pos] = use | (live_out[pos] & ~def);

            if (old_in != live_in[pos] || old_out != live_out[pos]) changed = 1;
        }
    }

    for (int i = 0; i < n; i++) {
        free(cfg[i].successors);
    }
    free(cfg);
}

static int interval_cmp(const void *a, const void *b) {
    LiveInterval *ia = (LiveInterval *)a;
    LiveInterval *ib = (LiveInterval *)b;
    if (ia->start != ib->start) return ia->start - ib->start;
    return ia->end - ib->end;
}

static void build_intervals(IRFunction *func, AllocCtx *ctx) {
    ctx->intervals = calloc(func->vreg_count, sizeof(LiveInterval));
    ctx->count = func->vreg_count;

    for (int i = 0; i < func->vreg_count; i++) {
        ctx->intervals[i].vreg = i;
        ctx->intervals[i].start = INT_MAX;
        ctx->intervals[i].end = -1;
        ctx->intervals[i].reg = -1;
        ctx->intervals[i].spill_loc = -1;
        ctx->intervals[i].use_count = 0;
        ctx->intervals[i].spill_cost = 0;
    }

    int *label_to_pos = calloc(func->label_count + 1, sizeof(int));
    int pos = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next, pos++) {
        if (inst->op == IR_LABEL) {
            label_to_pos[inst->dest.label] = pos;
        }
    }

    pos = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next, pos++) {
        if (inst->src1.type == OPERAND_VREG) {
            int v = inst->src1.vreg;
            if (ctx->intervals[v].start > pos) ctx->intervals[v].start = pos;
            ctx->intervals[v].end = pos;
            ctx->intervals[v].use_count++;
            ctx->intervals[v].spill_cost += 10;
        }
        if (inst->src2.type == OPERAND_VREG) {
            int v = inst->src2.vreg;
            if (ctx->intervals[v].start > pos) ctx->intervals[v].start = pos;
            ctx->intervals[v].end = pos;
            ctx->intervals[v].use_count++;
            ctx->intervals[v].spill_cost += 10;
        }
        if (inst->dest.type == OPERAND_VREG) {
            int v = inst->dest.vreg;
            if (ctx->intervals[v].start > pos) ctx->intervals[v].start = pos;
            ctx->intervals[v].end = pos;
            ctx->intervals[v].spill_cost += 1;
        }

        if (inst->op == IR_JMP || inst->op == IR_JZ || inst->op == IR_JNZ) {
            int target_pos = label_to_pos[inst->dest.label];
            if (target_pos <= pos) {
                for (int v = 0; v < func->vreg_count; v++) {
                    if (ctx->intervals[v].start != INT_MAX &&
                        ctx->intervals[v].start < pos &&
                        ctx->intervals[v].end >= target_pos) {
                        if (ctx->intervals[v].start > target_pos) ctx->intervals[v].start = target_pos;
                        if (ctx->intervals[v].end < pos) ctx->intervals[v].end = pos;
                    }
                }
            }
        }
    }

    free(label_to_pos);

    for (int i = 0; i < func->vreg_count; i++) {
        if (ctx->intervals[i].use_count > 0) {
            ctx->intervals[i].spill_cost = ctx->intervals[i].spill_cost * 100 /
                (ctx->intervals[i].end - ctx->intervals[i].start + 1);
        }
    }

    qsort(ctx->intervals, ctx->count, sizeof(LiveInterval), interval_cmp);
}

static int find_free_reg(AllocCtx *ctx, int pos) {
    for (int i = 3; i < num_regs; i++) {
        if (i == rax_idx || i == rcx_idx || i == rdx_idx) continue;
        if (ctx->regs[i].vreg == -1) return i;
        if (ctx->regs[i].last_use < pos) return i;
    }
    return -1;
}

static int spill_register(AllocCtx *ctx, int pos) {
    int max_cost = -1;
    int victim = -1;

    for (int i = 3; i < num_regs; i++) {
        if (i == rax_idx || i == rcx_idx || i == rdx_idx) continue;
        if (ctx->regs[i].vreg != -1) {
            LiveInterval *iv = NULL;
            for (int j = 0; j < ctx->count; j++) {
                if (ctx->intervals[j].vreg == ctx->regs[i].vreg) {
                    iv = &ctx->intervals[j];
                    break;
                }
            }
            if (iv && iv->spill_cost > max_cost) {
                max_cost = iv->spill_cost;
                victim = i;
            }
        }
    }

    if (victim != -1) {
        int v = ctx->regs[victim].vreg;
        ctx->vreg_to_loc[v] = -(ctx->next_spill++);
        ctx->regs[victim].vreg = -1;
        return victim;
    }

    return -1;
}

static void linear_scan_alloc(IRFunction *func, AllocCtx *ctx) {
    ctx->regs = calloc(num_regs, sizeof(RegState));
    ctx->vreg_to_loc = calloc(func->vreg_count, sizeof(int));
    ctx->next_spill = 1;

    for (int i = 0; i < num_regs; i++) {
        ctx->regs[i].vreg = -1;
        ctx->regs[i].last_use = -1;
    }

    for (int i = 0; i < func->vreg_count; i++) {
        ctx->vreg_to_loc[i] = -1000;
    }

    for (int i = 0; i < ctx->count; i++) {
        LiveInterval *iv = &ctx->intervals[i];
        if (iv->start == INT_MAX) continue;

        for (int j = 0; j < num_regs; j++) {
            if (ctx->regs[j].vreg != -1) {
                LiveInterval *active = NULL;
                for (int k = 0; k < ctx->count; k++) {
                    if (ctx->intervals[k].vreg == ctx->regs[j].vreg) {
                        active = &ctx->intervals[k];
                        break;
                    }
                }
                if (active && active->end < iv->start) {
                    ctx->regs[j].vreg = -1;
                }
            }
        }

        int reg = find_free_reg(ctx, iv->start);
        if (reg == -1) {
            reg = spill_register(ctx, iv->start);
        }

        if (reg != -1) {
            ctx->regs[reg].vreg = iv->vreg;
            ctx->regs[reg].last_use = iv->end;
            ctx->vreg_to_loc[iv->vreg] = reg;
            iv->reg = reg;
        } else {
            ctx->vreg_to_loc[iv->vreg] = -(ctx->next_spill++);
            iv->spill_loc = ctx->next_spill - 1;
        }
    }

    ctx->stack_slots = ctx->next_spill - 1;
}

static void get_operand_loc(int vreg, AllocCtx *ctx, char *buf, int size) {
    int loc = ctx->vreg_to_loc[vreg];
    if (loc >= 0) {
        snprintf(buf, size, "%s", regs[loc]);
    } else if (loc < -1) {
        int offset = (-loc) * 8;
        snprintf(buf, size, "QWORD PTR [rbp-%d]", offset);
    } else {
        snprintf(buf, size, "rax");
    }
}

static int is_mem(int vreg, AllocCtx *ctx) {
    return ctx->vreg_to_loc[vreg] < 0;
}

static void load_operand(int vreg, AllocCtx *ctx, const char *target) {
    char src[64];
    get_operand_loc(vreg, ctx, src, sizeof(src));
    if (strcmp(src, target) != 0) {
        emit("mov %s, %s", target, src);
    }
}

static void store_operand(int vreg, AllocCtx *ctx, const char *source) {
    char dst[64];
    get_operand_loc(vreg, ctx, dst, sizeof(dst));
    if (strcmp(dst, source) != 0) {
        emit("mov %s, %s", dst, source);
    }
}

static void gen_binary_op(IRInst *inst, AllocCtx *ctx, const char *op) {
    load_operand(inst->src1.vreg, ctx, "r15");
    load_operand(inst->src2.vreg, ctx, "r14");
    emit("%s r15, r14", op);
    store_operand(inst->dest.vreg, ctx, "r15");
}

static void gen_inst(IRInst *inst, AllocCtx *ctx) {
    char dst[64], src1[64], src2[64];

    switch (inst->op) {
        case IR_IMM:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            emit("mov %s, %ld", dst, inst->src1.imm);
            break;

        case IR_MOV:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
            if (strcmp(dst, src1) != 0) {
                if (is_mem(inst->dest.vreg, ctx) && is_mem(inst->src1.vreg, ctx)) {
                    emit("mov r15, %s", src1);
                    emit("mov %s, r15", dst);
                } else {
                    emit("mov %s, %s", dst, src1);
                }
            }
            break;

        case IR_ADD:
            gen_binary_op(inst, ctx, "add");
            break;

        case IR_SUB:
            gen_binary_op(inst, ctx, "sub");
            break;

        case IR_MUL:
            gen_binary_op(inst, ctx, "imul");
            break;

        case IR_DIV:
            load_operand(inst->src1.vreg, ctx, "rax");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("cqo");
            emit("idiv r15");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_MOD:
            load_operand(inst->src1.vreg, ctx, "rax");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("cqo");
            emit("idiv r15");
            store_operand(inst->dest.vreg, ctx, "rdx");
            break;

        case IR_NEG:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
            if (strcmp(dst, src1) != 0) {
                if (is_mem(inst->dest.vreg, ctx) && is_mem(inst->src1.vreg, ctx)) {
                    emit("mov r15, %s", src1);
                    emit("neg r15");
                    emit("mov %s, r15", dst);
                } else {
                    emit("mov %s, %s", dst, src1);
                    emit("neg %s", dst);
                }
            } else {
                emit("neg %s", dst);
            }
            break;

        case IR_RET:
            load_operand(inst->src1.vreg, ctx, "rax");

            int stack_size = ctx->stack_slots * 8;
            if (stack_size > 0) {
                stack_size = (stack_size + 15) & ~15;
                emit("add rsp, %d", stack_size);
            }

            emit("pop r15");
            emit("pop r14");
            emit("pop r13");
            emit("pop r12");
            emit("pop rbx");

            emit("pop rbp");
            emit("ret");
            break;

        case IR_LABEL:
            fprintf(out, ".L%d:\n", inst->dest.label);
            break;

        case IR_JMP:
            emit("jmp .L%d", inst->dest.label);
            break;

        case IR_JZ:
            load_operand(inst->src1.vreg, ctx, "r15");
            emit("test r15, r15");
            emit("jz .L%d", inst->dest.label);
            break;

        case IR_JNZ:
            load_operand(inst->src1.vreg, ctx, "r15");
            emit("test r15, r15");
            emit("jnz .L%d", inst->dest.label);
            break;

        case IR_EQ:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("sete al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_NE:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("setne al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_LT:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("setl al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_LE:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("setle al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_GT:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("setg al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_GE:
            load_operand(inst->src1.vreg, ctx, "r15");
            load_operand(inst->src2.vreg, ctx, "r14");
            emit("xor rax, rax");
            emit("cmp r15, r14");
            emit("setge al");
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_LOAD:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            if (inst->cast_to_type) {
                int size = inst->cast_to_type->size;

                // Se o destino é memória, precisamos usar registrador temporário
                int dst_is_mem = (strstr(dst, "PTR") != NULL);
                const char *target_reg = dst_is_mem ? "r15" : dst;

                if (size == 8) {
                    emit("mov %s, QWORD PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                } else if (size == 4) {
                    if (inst->cast_to_type->is_signed) {
                        emit("movsxd %s, DWORD PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    } else {
                        emit("mov %sd, DWORD PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    }
                } else if (size == 2) {
                    if (inst->cast_to_type->is_signed) {
                        emit("movsx %s, WORD PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    } else {
                        emit("movzx %s, WORD PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    }
                } else if (size == 1) {
                    if (inst->cast_to_type->is_signed) {
                        emit("movsx %s, BYTE PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    } else {
                        emit("movzx %s, BYTE PTR [rbp-%ld]", target_reg, inst->src1.imm + size);
                    }
                }

                // Se destino era memória, mover do temporário para lá
                if (dst_is_mem) {
                    emit("mov %s, r15", dst);
                }
            } else {
                emit("mov %s, QWORD PTR [rbp-%ld]", dst, inst->src1.imm + 8);
            }
            break;

        case IR_STORE:
            get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
            if (inst->cast_to_type) {
                int size = inst->cast_to_type->size;

                // Sempre usar registrador temporário para source se for memória
                int src_is_mem = (strstr(src1, "PTR") != NULL);
                const char *source_reg = src1;

                if (src_is_mem) {
                    emit("mov r15, %s", src1);
                    source_reg = "r15";
                }

                if (size == 8) {
                    emit("mov QWORD PTR [rbp-%ld], %s", inst->dest.imm + size, source_reg);
                } else if (size == 4) {
                    char reg32[64];
                    if (strcmp(source_reg, "rax") == 0) strcpy(reg32, "eax");
                    else if (strcmp(source_reg, "rbx") == 0) strcpy(reg32, "ebx");
                    else if (strcmp(source_reg, "rcx") == 0) strcpy(reg32, "ecx");
                    else if (strcmp(source_reg, "rdx") == 0) strcpy(reg32, "edx");
                    else if (strcmp(source_reg, "rsi") == 0) strcpy(reg32, "esi");
                    else if (strcmp(source_reg, "rdi") == 0) strcpy(reg32, "edi");
                    else if (strcmp(source_reg, "r8") == 0) strcpy(reg32, "r8d");
                    else if (strcmp(source_reg, "r9") == 0) strcpy(reg32, "r9d");
                    else if (strcmp(source_reg, "r10") == 0) strcpy(reg32, "r10d");
                    else if (strcmp(source_reg, "r11") == 0) strcpy(reg32, "r11d");
                    else if (strcmp(source_reg, "r12") == 0) strcpy(reg32, "r12d");
                    else if (strcmp(source_reg, "r13") == 0) strcpy(reg32, "r13d");
                    else if (strcmp(source_reg, "r14") == 0) strcpy(reg32, "r14d");
                    else if (strcmp(source_reg, "r15") == 0) strcpy(reg32, "r15d");
                    else strncpy(reg32, source_reg, sizeof(reg32) - 1);
                    emit("mov DWORD PTR [rbp-%ld], %s", inst->dest.imm + size, reg32);
                } else if (size == 2) {
                    char reg16[64];
                    if (strcmp(source_reg, "rax") == 0) strcpy(reg16, "ax");
                    else if (strcmp(source_reg, "rbx") == 0) strcpy(reg16, "bx");
                    else if (strcmp(source_reg, "rcx") == 0) strcpy(reg16, "cx");
                    else if (strcmp(source_reg, "rdx") == 0) strcpy(reg16, "dx");
                    else if (strcmp(source_reg, "rsi") == 0) strcpy(reg16, "si");
                    else if (strcmp(source_reg, "rdi") == 0) strcpy(reg16, "di");
                    else if (strcmp(source_reg, "r8") == 0) strcpy(reg16, "r8w");
                    else if (strcmp(source_reg, "r9") == 0) strcpy(reg16, "r9w");
                    else if (strcmp(source_reg, "r10") == 0) strcpy(reg16, "r10w");
                    else if (strcmp(source_reg, "r11") == 0) strcpy(reg16, "r11w");
                    else if (strcmp(source_reg, "r12") == 0) strcpy(reg16, "r12w");
                    else if (strcmp(source_reg, "r13") == 0) strcpy(reg16, "r13w");
                    else if (strcmp(source_reg, "r14") == 0) strcpy(reg16, "r14w");
                    else if (strcmp(source_reg, "r15") == 0) strcpy(reg16, "r15w");
                    else strncpy(reg16, source_reg, sizeof(reg16) - 1);
                    emit("mov WORD PTR [rbp-%ld], %s", inst->dest.imm + size, reg16);
                } else if (size == 1) {
                    char reg8[64];
                    if (strcmp(source_reg, "rax") == 0) strcpy(reg8, "al");
                    else if (strcmp(source_reg, "rbx") == 0) strcpy(reg8, "bl");
                    else if (strcmp(source_reg, "rcx") == 0) strcpy(reg8, "cl");
                    else if (strcmp(source_reg, "rdx") == 0) strcpy(reg8, "dl");
                    else if (strcmp(source_reg, "rsi") == 0) strcpy(reg8, "sil");
                    else if (strcmp(source_reg, "rdi") == 0) strcpy(reg8, "dil");
                    else if (strcmp(source_reg, "r8") == 0) strcpy(reg8, "r8b");
                    else if (strcmp(source_reg, "r9") == 0) strcpy(reg8, "r9b");
                    else if (strcmp(source_reg, "r10") == 0) strcpy(reg8, "r10b");
                    else if (strcmp(source_reg, "r11") == 0) strcpy(reg8, "r11b");
                    else if (strcmp(source_reg, "r12") == 0) strcpy(reg8, "r12b");
                    else if (strcmp(source_reg, "r13") == 0) strcpy(reg8, "r13b");
                    else if (strcmp(source_reg, "r14") == 0) strcpy(reg8, "r14b");
                    else if (strcmp(source_reg, "r15") == 0) strcpy(reg8, "r15b");
                    else strncpy(reg8, source_reg, sizeof(reg8) - 1);
                    emit("mov BYTE PTR [rbp-%ld], %s", inst->dest.imm + size, reg8);
                }
            } else {
                emit("mov QWORD PTR [rbp-%ld], %s", inst->dest.imm + 8, src1);
            }
            break;

        case IR_ASM:
            if (inst->asm_str) {
                fprintf(out, "  %s\n", inst->asm_str);
            }
            break;

        case IR_CAST:
            get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));

            if (!inst->cast_to_type) {
                if (strcmp(dst, src1) != 0) {
                    emit("mov %s, %s", dst, src1);
                }
                break;
            }

            int to_size = inst->cast_to_type->size;

            // Verificar se source ou dest são memória
            int src_is_mem = (strstr(src1, "PTR") != NULL);
            int dst_is_mem = (strstr(dst, "PTR") != NULL);

            const char *work_src = src1;
            const char *work_dst = dst;

            // Se source é memória, carregar em r14 primeiro
            if (src_is_mem) {
                emit("mov r14, %s", src1);
                work_src = "r14";
            }

            // Se dest é memória, usar r15 como temporário
            if (dst_is_mem) {
                work_dst = "r15";
            }

            if (to_size == 8) {
                if (strcmp(work_dst, work_src) != 0) {
                    emit("mov %s, %s", work_dst, work_src);
                }
            } else if (to_size == 4) {
                char src_32[64];
                if (strcmp(work_src, "rax") == 0) strcpy(src_32, "eax");
                else if (strcmp(work_src, "rbx") == 0) strcpy(src_32, "ebx");
                else if (strcmp(work_src, "rcx") == 0) strcpy(src_32, "ecx");
                else if (strcmp(work_src, "rdx") == 0) strcpy(src_32, "edx");
                else if (strcmp(work_src, "rsi") == 0) strcpy(src_32, "esi");
                else if (strcmp(work_src, "rdi") == 0) strcpy(src_32, "edi");
                else if (strcmp(work_src, "r8") == 0) strcpy(src_32, "r8d");
                else if (strcmp(work_src, "r9") == 0) strcpy(src_32, "r9d");
                else if (strcmp(work_src, "r10") == 0) strcpy(src_32, "r10d");
                else if (strcmp(work_src, "r11") == 0) strcpy(src_32, "r11d");
                else if (strcmp(work_src, "r12") == 0) strcpy(src_32, "r12d");
                else if (strcmp(work_src, "r13") == 0) strcpy(src_32, "r13d");
                else if (strcmp(work_src, "r14") == 0) strcpy(src_32, "r14d");
                else if (strcmp(work_src, "r15") == 0) strcpy(src_32, "r15d");
                else strncpy(src_32, work_src, sizeof(src_32) - 1);

                if (inst->cast_to_type->is_signed) {
                    emit("movsxd %s, %s", work_dst, src_32);
                } else {
                    emit("mov %s, %s", work_dst, src_32);
                }
            } else if (to_size == 2) {
                char src_16[64];
                if (strcmp(work_src, "rax") == 0) strcpy(src_16, "ax");
                else if (strcmp(work_src, "rbx") == 0) strcpy(src_16, "bx");
                else if (strcmp(work_src, "rcx") == 0) strcpy(src_16, "cx");
                else if (strcmp(work_src, "rdx") == 0) strcpy(src_16, "dx");
                else if (strcmp(work_src, "rsi") == 0) strcpy(src_16, "si");
                else if (strcmp(work_src, "rdi") == 0) strcpy(src_16, "di");
                else if (strcmp(work_src, "r8") == 0) strcpy(src_16, "r8w");
                else if (strcmp(work_src, "r9") == 0) strcpy(src_16, "r9w");
                else if (strcmp(work_src, "r10") == 0) strcpy(src_16, "r10w");
                else if (strcmp(work_src, "r11") == 0) strcpy(src_16, "r11w");
                else if (strcmp(work_src, "r12") == 0) strcpy(src_16, "r12w");
                else if (strcmp(work_src, "r13") == 0) strcpy(src_16, "r13w");
                else if (strcmp(work_src, "r14") == 0) strcpy(src_16, "r14w");
                else if (strcmp(work_src, "r15") == 0) strcpy(src_16, "r15w");
                else strncpy(src_16, work_src, sizeof(src_16) - 1);

                if (inst->cast_to_type->is_signed) {
                    emit("movsx %s, %s", work_dst, src_16);
                } else {
                    emit("movzx %s, %s", work_dst, src_16);
                }
            } else if (to_size == 1) {
                char src_8[64];
                if (strcmp(work_src, "rax") == 0) strcpy(src_8, "al");
                else if (strcmp(work_src, "rbx") == 0) strcpy(src_8, "bl");
                else if (strcmp(work_src, "rcx") == 0) strcpy(src_8, "cl");
                else if (strcmp(work_src, "rdx") == 0) strcpy(src_8, "dl");
                else if (strcmp(work_src, "rsi") == 0) strcpy(src_8, "sil");
                else if (strcmp(work_src, "rdi") == 0) strcpy(src_8, "dil");
                else if (strcmp(work_src, "r8") == 0) strcpy(src_8, "r8b");
                else if (strcmp(work_src, "r9") == 0) strcpy(src_8, "r9b");
                else if (strcmp(work_src, "r10") == 0) strcpy(src_8, "r10b");
                else if (strcmp(work_src, "r11") == 0) strcpy(src_8, "r11b");
                else if (strcmp(work_src, "r12") == 0) strcpy(src_8, "r12b");
                else if (strcmp(work_src, "r13") == 0) strcpy(src_8, "r13b");
                else if (strcmp(work_src, "r14") == 0) strcpy(src_8, "r14b");
                else if (strcmp(work_src, "r15") == 0) strcpy(src_8, "r15b");
                else strncpy(src_8, work_src, sizeof(src_8) - 1);

                if (inst->cast_to_type->is_signed) {
                    emit("movsx %s, %s", work_dst, src_8);
                } else {
                    emit("movzx %s, %s", work_dst, src_8);
                }
            }

            // Se destino era memória, copiar do temporário
            if (dst_is_mem) {
                emit("mov %s, r15", dst);
            }
            break;

            case IR_CALL: // <-- ADICIONE ESTE CASE
                // TODO: No futuro, aqui teremos que salvar os registradores "caller-saved".
                emit("call %s", inst->call_target_name);

                // O valor de retorno da função está em 'rax', pela convenção da ABI.
                // Armazenamos o valor de 'rax' no local designado para o vreg de destino.
                store_operand(inst->dest.vreg, ctx, "rax");
                break;

        default:
            break;
    }
}

static void gen_func(IRFunction *func) {
    AllocCtx ctx = {0};

    build_intervals(func, &ctx);
    linear_scan_alloc(func, &ctx);

    fprintf(out, "%s:\n", func->name);
    emit("push rbp");
    emit("mov rbp, rsp");
    emit("push rbx");
    emit("push r12");
    emit("push r13");
    emit("push r14");
    emit("push r15");

    int stack_size = ctx.stack_slots * 8;
    if (stack_size > 0) {
        stack_size = (stack_size + 15) & ~15;
        emit("sub rsp, %d", stack_size);
    }

    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        gen_inst(inst, &ctx);
    }

    free(ctx.intervals);
    free(ctx.regs);
    free(ctx.vreg_to_loc);
}

void codegen(IRProgram *prog, const char *filename) {
    out = fopen(filename, "w");
    if (!out) {
        fprintf(stderr, "Erro ao abrir arquivo de saida: %s\n", filename);
        return;
    }

    fprintf(out, ".intel_syntax noprefix\n");
    fprintf(out, ".global main\n\n");

    for (IRFunction *func = prog->functions; func; func = func->next) {
        gen_func(func);
        fprintf(out, "\n");
    }

    fclose(out);
}
