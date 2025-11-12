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

static void liveness_analysis(IRFunction *func, unsigned long *live_in, unsigned long *live_out) {
    int n = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next) n++;

    int changed = 1;
    while (changed) {
        changed = 0;
        int i = n - 1;
        for (IRInst *inst = func->instructions; inst; inst = inst->next, i--) {
            unsigned long old_in = live_in[i];
            unsigned long old_out = live_out[i];

            unsigned long use = 0, def = 0;
            if (inst->src1.type == OPERAND_VREG && inst->src1.vreg < 64) use |= (1UL << inst->src1.vreg);
            if (inst->src2.type == OPERAND_VREG && inst->src2.vreg < 64) use |= (1UL << inst->src2.vreg);
            if (inst->dest.type == OPERAND_VREG && inst->dest.vreg < 64) def |= (1UL << inst->dest.vreg);

            live_in[i] = use | (live_out[i] & ~def);
            live_out[i] = inst->next ? live_in[i + 1] : 0;

            if (old_in != live_in[i] || old_out != live_out[i]) changed = 1;
        }
    }
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

    int pos = 0;
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
    }

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
    char dst[64], src1[64], src2[64];
    get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
    get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
    get_operand_loc(inst->src2.vreg, ctx, src2, sizeof(src2));

    int dst_mem = is_mem(inst->dest.vreg, ctx);
    int src1_mem = is_mem(inst->src1.vreg, ctx);
    int src2_mem = is_mem(inst->src2.vreg, ctx);

    if (strcmp(dst, src1) == 0) {
        if (src2_mem) {
            emit("mov r15, %s", src2);
            emit("%s %s, r15", op, dst);
        } else {
            emit("%s %s, %s", op, dst, src2);
        }
    } else {
        if (dst_mem) {
            if (src1_mem) {
                emit("mov r15, %s", src1);
                if (src2_mem) {
                    emit("mov r14, %s", src2);
                    emit("%s r15, r14", op);
                } else {
                    emit("%s r15, %s", op, src2);
                }
                emit("mov %s, r15", dst);
            } else {
                emit("mov r15, %s", src1);
                if (src2_mem) {
                    emit("mov r14, %s", src2);
                    emit("%s r15, r14", op);
                } else {
                    emit("%s r15, %s", op, src2);
                }
                emit("mov %s, r15", dst);
            }
        } else {
            if (src1_mem) {
                emit("mov %s, %s", dst, src1);
            } else {
                if (strcmp(dst, src1) != 0) {
                    emit("mov %s, %s", dst, src1);
                }
            }
            if (src2_mem) {
                emit("mov r15, %s", src2);
                emit("%s %s, r15", op, dst);
            } else {
                emit("%s %s, %s", op, dst, src2);
            }
        }
    }
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
            emit("mov rsp, rbp");
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
