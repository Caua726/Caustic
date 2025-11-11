#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static FILE *out;
static int *vreg_to_reg;
static int vreg_count;

static const char *regs[] = {"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"};

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(out, "  ");
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}

static const char *get_reg(int vreg) {
    if (vreg >= vreg_count || vreg_to_reg[vreg] == -1) {
        return "rax";
    }
    return regs[vreg_to_reg[vreg]];
}

static void alloc_regs(IRFunction *func) {
    vreg_count = func->vreg_count;
    vreg_to_reg = calloc(vreg_count, sizeof(int));

    for (int i = 0; i < vreg_count; i++) {
        vreg_to_reg[i] = i < 9 ? i : -1;
    }
}

static void gen_inst(IRInst *inst) {
    const char *dst, *src1, *src2;

    switch (inst->op) {
        case IR_IMM:
            dst = get_reg(inst->dest.vreg);
            emit("mov %s, %ld", dst, inst->src1.imm);
            break;

        case IR_MOV:
            dst = get_reg(inst->dest.vreg);
            src1 = get_reg(inst->src1.vreg);
            if (strcmp(dst, src1) != 0) {
                emit("mov %s, %s", dst, src1);
            }
            break;

        case IR_ADD:
            dst = get_reg(inst->dest.vreg);
            src1 = get_reg(inst->src1.vreg);
            src2 = get_reg(inst->src2.vreg);
            if (strcmp(dst, src1) == 0) {
                emit("add %s, %s", dst, src2);
            } else {
                emit("mov %s, %s", dst, src1);
                emit("add %s, %s", dst, src2);
            }
            break;

        case IR_SUB:
            dst = get_reg(inst->dest.vreg);
            src1 = get_reg(inst->src1.vreg);
            src2 = get_reg(inst->src2.vreg);
            if (strcmp(dst, src1) == 0) {
                emit("sub %s, %s", dst, src2);
            } else {
                emit("mov %s, %s", dst, src1);
                emit("sub %s, %s", dst, src2);
            }
            break;

        case IR_MUL:
            dst = get_reg(inst->dest.vreg);
            src1 = get_reg(inst->src1.vreg);
            src2 = get_reg(inst->src2.vreg);
            if (strcmp(dst, src1) == 0) {
                emit("imul %s, %s", dst, src2);
            } else {
                emit("mov %s, %s", dst, src1);
                emit("imul %s, %s", dst, src2);
            }
            break;

        case IR_DIV:
            src1 = get_reg(inst->src1.vreg);
            src2 = get_reg(inst->src2.vreg);
            if (strcmp(src1, "rax") != 0) {
                emit("mov rax, %s", src1);
            }
            emit("cqo");
            emit("idiv %s", src2);
            dst = get_reg(inst->dest.vreg);
            if (strcmp(dst, "rax") != 0) {
                emit("mov %s, rax", dst);
            }
            break;

        case IR_NEG:
            dst = get_reg(inst->dest.vreg);
            src1 = get_reg(inst->src1.vreg);
            if (strcmp(dst, src1) != 0) {
                emit("mov %s, %s", dst, src1);
            }
            emit("neg %s", dst);
            break;

        case IR_RET:
            src1 = get_reg(inst->src1.vreg);
            if (strcmp(src1, "rax") != 0) {
                emit("mov rax, %s", src1);
            }
            emit("ret");
            break;

        default:
            break;
    }
}

static void gen_func(IRFunction *func) {
    fprintf(out, "%s:\n", func->name);

    alloc_regs(func);

    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        gen_inst(inst);
    }

    free(vreg_to_reg);
}

void codegen(IRProgram *prog, const char *filename) {
    out = fopen(filename, "w");
    if (!out) {
        return;
    }

    fprintf(out, ".intel_syntax noprefix\n");
    fprintf(out, ".global main\n\n");

    for (IRFunction *func = prog->functions; func; func = func->next) {
        gen_func(func);
    }

    fclose(out);
}
