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
    int is_leaf;
    int arg_spill_base;
    IRFunction *current_func; // Added to access func info in gen_inst
} AllocCtx;

static FILE *out;
static const char *regs[] = {"rax", "rcx", "rdx", "rbx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13"};
static const int num_regs = 12;


// System V AMD64 ABI argument registers: rdi, rsi, rdx, rcx, r8, r9
static const char *arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static const int num_arg_regs = 6;

static const char *syscall_regs[] = {"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"};

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(out, "  ");
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}





static const char *reg64_to_reg32(const char *reg64) {
    if (strcmp(reg64, "rax") == 0) return "eax";
    if (strcmp(reg64, "rbx") == 0) return "ebx";
    if (strcmp(reg64, "rcx") == 0) return "ecx";
    if (strcmp(reg64, "rdx") == 0) return "edx";
    if (strcmp(reg64, "rsi") == 0) return "esi";
    if (strcmp(reg64, "rdi") == 0) return "edi";
    if (strcmp(reg64, "r8") == 0) return "r8d";
    if (strcmp(reg64, "r9") == 0) return "r9d";
    if (strcmp(reg64, "r10") == 0) return "r10d";
    if (strcmp(reg64, "r11") == 0) return "r11d";
    if (strcmp(reg64, "r12") == 0) return "r12d";
    if (strcmp(reg64, "r13") == 0) return "r13d";
    if (strcmp(reg64, "r14") == 0) return "r14d";
    if (strcmp(reg64, "r15") == 0) return "r15d";
    return reg64;
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
    int allowed[] = {3, 10, 11}; // rbx, r12, r13
    for (int k = 0; k < 3; k++) {
        int i = allowed[k];
        if (ctx->regs[i].vreg == -1) return i;
        if (ctx->regs[i].last_use < pos) return i;
    }
    return -1;
}

static int spill_register(AllocCtx *ctx) {
    int max_cost = -1;
    int victim = -1;

    int allowed[] = {3, 10, 11}; // rbx, r12, r13
    for (int k = 0; k < 3; k++) {
        int i = allowed[k];
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
    // ctx->next_spill is already initialized in gen_func

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
            reg = spill_register(ctx);
        }

        if (reg != -1) {
            ctx->regs[reg].vreg = iv->vreg;
            ctx->regs[reg].last_use = iv->end;
            ctx->vreg_to_loc[iv->vreg] = reg;
            iv->reg = reg;
        } else {
            ctx->vreg_to_loc[iv->vreg] = -(ctx->next_spill++);
            iv->spill_loc = ctx->next_spill - 1;
            printf("; Spilled vreg %d to loc %d (offset %d)\n", iv->vreg, ctx->vreg_to_loc[iv->vreg], (-ctx->vreg_to_loc[iv->vreg])*8);
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

static void gen_inst(IRInst *inst, AllocCtx *ctx, int alloc_stack_size) {
    char dst[64], src1[64];

    switch (inst->op) {
        case IR_IMM:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            if (is_mem(inst->dest.vreg, ctx) && (inst->src1.imm > INT_MAX || inst->src1.imm < INT_MIN)) {
                emit("mov r15, %ld", inst->src1.imm);
                emit("mov %s, r15", dst);
            } else {
                emit("mov %s, %ld", dst, inst->src1.imm);
            }
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
            emit("push rdx");
            emit("cqo");
            emit("idiv r15");
            emit("mov r15, rax");
            emit("pop rdx");
            store_operand(inst->dest.vreg, ctx, "r15");
            break;

        case IR_MOD:
            load_operand(inst->src1.vreg, ctx, "rax");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("push rdx");
            emit("cqo");
            emit("idiv r15");
            emit("mov r15, rdx");
            emit("pop rdx");
            store_operand(inst->dest.vreg, ctx, "r15");
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

        // Float operations using SSE
        case IR_FADD:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("movq xmm0, r14");
            emit("movq xmm1, r15");
            emit("addsd xmm0, xmm1");
            emit("movq r14, xmm0");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_FSUB:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("movq xmm0, r14");
            emit("movq xmm1, r15");
            emit("subsd xmm0, xmm1");
            emit("movq r14, xmm0");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_FMUL:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("movq xmm0, r14");
            emit("movq xmm1, r15");
            emit("mulsd xmm0, xmm1");
            emit("movq r14, xmm0");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_FDIV:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("movq xmm0, r14");
            emit("movq xmm1, r15");
            emit("divsd xmm0, xmm1");
            emit("movq r14, xmm0");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_FNEG:
            load_operand(inst->src1.vreg, ctx, "r14");
            emit("movq xmm0, r14");
            // XOR with sign bit to negate: -0.0 has the sign bit set
            emit("mov r15, 0x8000000000000000");
            emit("movq xmm1, r15");
            emit("xorpd xmm0, xmm1");
            emit("movq r14, xmm0");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_RET:
            if (inst->src1.type != OPERAND_NONE) {
                load_operand(inst->src1.vreg, ctx, "rax");
            }

            int stack_size = ctx->stack_slots * 8 + alloc_stack_size;
            if (stack_size > 0) {
                stack_size = (stack_size + 15) & ~15;
                emit("add rsp, %d", stack_size);
            }

            emit("pop r15");
            emit("pop r14");
            emit("pop r13");
            emit("pop r12");
            emit("pop rbx");

            if (!ctx->is_leaf) {
                emit("pop rbp");
            }
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

        case IR_SHL:
            load_operand(inst->src2.vreg, ctx, "rcx");
            load_operand(inst->src1.vreg, ctx, "r15");
            emit("shl r15, cl");
            store_operand(inst->dest.vreg, ctx, "r15");
            break;

        case IR_SHR:
            load_operand(inst->src2.vreg, ctx, "rcx");
            load_operand(inst->src1.vreg, ctx, "r15");
            emit("shr r15, cl");
            store_operand(inst->dest.vreg, ctx, "r15");
            break;

        case IR_AND:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("and r14, r15");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_OR:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("or r14, r15");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_XOR:
            load_operand(inst->src1.vreg, ctx, "r14");
            load_operand(inst->src2.vreg, ctx, "r15");
            emit("xor r14, r15");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_NOT:
            load_operand(inst->src1.vreg, ctx, "r14");
            emit("not r14");
            store_operand(inst->dest.vreg, ctx, "r14");
            break;

        case IR_ADDR:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            if (strstr(dst, "PTR") != NULL || strstr(dst, "[rbp-") != NULL) {
                emit("lea r15, [rbp-%ld]", inst->src1.imm);
                emit("mov %s, r15", dst);
            } else {
                emit("lea %s, [rbp-%ld]", dst, inst->src1.imm);
            }
            break;

        case IR_ADDR_GLOBAL:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            if (strstr(dst, "PTR") != NULL || strstr(dst, "[rbp-") != NULL) {
                emit("lea r15, [rip+%s]", inst->global_name);
                emit("mov %s, r15", dst);
            } else {
                emit("lea %s, [rip+%s]", dst, inst->global_name);
            }
            break;

        case IR_GET_ALLOC_ADDR:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            // Base of alloc area is [rbp - (ctx->stack_slots * 8)]
            // Offset is inst->src1.imm
            // Addr = rbp - (ctx->stack_slots * 8 + inst->src1.imm)
            if (strstr(dst, "PTR") != NULL || strstr(dst, "[rbp-") != NULL) {
                emit("lea r15, [rbp-%ld]", (long)(ctx->stack_slots * 8 + inst->src1.imm));
                emit("mov %s, r15", dst);
            } else {
                emit("lea %s, [rbp-%ld]", dst, (long)(ctx->stack_slots * 8 + inst->src1.imm));
            }
            break;

        case IR_SET_CTX:
            if (inst->src1.type == OPERAND_IMM) {
                emit("xor r10, r10");
            } else {
                load_operand(inst->src1.vreg, ctx, "r10");
            }
            break;

        case IR_LOAD:
            get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
            
            // Check if source is a VREG (pointer/address) or IMM (stack offset)
            if (inst->src1.type == OPERAND_VREG) {
                load_operand(inst->src1.vreg, ctx, "r15"); // Load address into r15
                
                if (inst->cast_to_type) {
                    int size = inst->cast_to_type->size;
                    const char *target_reg = dst;
                    // If dest is mem, use temp reg
                    if (strstr(dst, "PTR") != NULL) target_reg = "rax";

                    if (size == 8) emit("mov %s, QWORD PTR [r15]", target_reg);
                    else if (size == 4) {
                        if (inst->cast_to_type->is_signed) emit("movsxd %s, DWORD PTR [r15]", target_reg);
                        else emit("mov %s, DWORD PTR [r15]", reg64_to_reg32(target_reg));
                    }
                    else if (size == 2) {
                        if (inst->cast_to_type->is_signed) emit("movsx %s, WORD PTR [r15]", target_reg);
                        else emit("movzx %s, WORD PTR [r15]", target_reg);
                    }
                    else if (size == 1) {
                        if (inst->cast_to_type->is_signed) emit("movsx %s, BYTE PTR [r15]", target_reg);
                        else emit("movzx %s, BYTE PTR [r15]", target_reg);
                    }

                    if (strstr(dst, "PTR") != NULL) emit("mov %s, rax", dst);

                } else {
                    emit("mov %s, QWORD PTR [r15]", dst);
                }
            } else {
                // Existing logic for stack offset
                if (inst->cast_to_type) {
                    int size = inst->cast_to_type->size;
                    int dst_is_mem = (strstr(dst, "PTR") != NULL);
                    const char *target_reg = dst_is_mem ? "r15" : dst;

                    if (size == 8) {
                        emit("mov %s, QWORD PTR [rbp-%ld]", target_reg, inst->src1.imm);
                    } else if (size == 4) {
                        if (inst->cast_to_type->is_signed) {
                            emit("movsxd %s, DWORD PTR [rbp-%ld]", target_reg, inst->src1.imm);
                        } else {
                            emit("mov %s, DWORD PTR [rbp-%ld]", reg64_to_reg32(target_reg), inst->src1.imm);
                        }
                    } else if (size == 2) {
                        if (inst->cast_to_type->is_signed) {
                            emit("movsx %s, WORD PTR [rbp-%ld]", target_reg, inst->src1.imm);
                        } else {
                            emit("movzx %s, WORD PTR [rbp-%ld]", target_reg, inst->src1.imm);
                        }
                    } else if (size == 1) {
                        if (inst->cast_to_type->is_signed) {
                            emit("movsx %s, BYTE PTR [rbp-%ld]", target_reg, inst->src1.imm);
                        } else {
                            emit("movzx %s, BYTE PTR [rbp-%ld]", target_reg, inst->src1.imm);
                        }
                    }

                    if (dst_is_mem) {
                        emit("mov %s, r15", dst);
                    }
                } else {
                    emit("mov %s, QWORD PTR [rbp-%ld]", dst, inst->src1.imm);
                }
            }
            break;

        case IR_STORE:
            get_operand_loc(inst->src1.vreg, ctx, src1, sizeof(src1));
            
            // Check if dest is VREG (pointer/address) or IMM (stack offset)
            if (inst->dest.type == OPERAND_VREG) {
                load_operand(inst->dest.vreg, ctx, "r15"); // Load address into r15
                
                // Load value into rax if it's in memory
                const char *val_reg = src1;
                if (strstr(src1, "PTR") != NULL) {
                    emit("mov rax, %s", src1);
                    val_reg = "rax";
                }

                if (inst->cast_to_type) {
                    int size = inst->cast_to_type->size;
                    if (size == 8) emit("mov QWORD PTR [r15], %s", val_reg);
                    else if (size == 4) {
                        // Extract 32-bit part of reg
                        char reg32[128];
                        if (strcmp(val_reg, "rax") == 0) strcpy(reg32, "eax");
                        else if (strcmp(val_reg, "rbx") == 0) strcpy(reg32, "ebx");
                        // ... simplified for brevity, assuming standard regs ...
                        else if (val_reg[0] == 'r' && val_reg[2] == 0) { snprintf(reg32, sizeof(reg32), "%sd", val_reg); } // r8 -> r8d
                        else if (val_reg[0] == 'r') { snprintf(reg32, sizeof(reg32), "%sd", val_reg); } // r12 -> r12d
                        else { snprintf(reg32, sizeof(reg32), "e%s", val_reg+1); } // rbx -> ebx (approx)
                        
                        // Better reg mapping needed or reuse existing helper logic?
                        // Let's reuse the logic from existing STORE but adapted.
                        // Actually, let's just force load to rax for simplicity if size < 8
                        if (strcmp(val_reg, "rax") != 0) emit("mov rax, %s", val_reg);
                        emit("mov DWORD PTR [r15], eax");
                    }
                    else if (size == 2) {
                        if (strcmp(val_reg, "rax") != 0) emit("mov rax, %s", val_reg);
                        emit("mov WORD PTR [r15], ax");
                    }
                    else if (size == 1) {
                        if (strcmp(val_reg, "rax") != 0) emit("mov rax, %s", val_reg);
                        emit("mov BYTE PTR [r15], al");
                    }
                } else {
                    emit("mov QWORD PTR [r15], %s", val_reg);
                }

            } else {
                // Existing logic for stack offset
                if (inst->cast_to_type) {
                    int size = inst->cast_to_type->size;
                    int src_is_mem = (strstr(src1, "PTR") != NULL);
                    const char *source_reg = src1;

                    if (src_is_mem) {
                        emit("mov r15, %s", src1);
                        source_reg = "r15";
                    }

                    if (size == 8) {
                        emit("mov QWORD PTR [rbp-%ld], %s", inst->dest.imm, source_reg);
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
                        emit("mov DWORD PTR [rbp-%ld], %s", inst->dest.imm, reg32);
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
                        emit("mov WORD PTR [rbp-%ld], %s", inst->dest.imm, reg16);
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
                        emit("mov BYTE PTR [rbp-%ld], %s", inst->dest.imm, reg8);
                    } else {
                        // Struct > 8 bytes (passed by reference/pointer in register)
                        // Copy from [src_reg] to [rbp-dest]
                        // src1 holds the ADDRESS of the source struct
                        
                        emit("lea rdi, [rbp-%ld]", inst->dest.imm); // Dest address (local var)
                        emit("mov rsi, %s", source_reg);            // Source address (param)
                        emit("mov rcx, %d", size);
                        emit("cld");
                        emit("rep movsb");
                    }
                } else {
                    emit("mov QWORD PTR [rbp-%ld], %s", inst->dest.imm, src1);
                }
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

            // Handle float <-> int conversions
            if (inst->cast_from_type) {
                TypeKind from_kind = inst->cast_from_type->kind;
                TypeKind to_kind = inst->cast_to_type->kind;

                // Float to integer conversion
                if ((from_kind == TY_F64 || from_kind == TY_F32) &&
                    (to_kind != TY_F64 && to_kind != TY_F32)) {
                    load_operand(inst->src1.vreg, ctx, "r14");
                    emit("movq xmm0, r14");
                    emit("cvttsd2si r14, xmm0");
                    store_operand(inst->dest.vreg, ctx, "r14");
                    break;
                }

                // Integer to float conversion
                if ((from_kind != TY_F64 && from_kind != TY_F32) &&
                    (to_kind == TY_F64 || to_kind == TY_F32)) {
                    load_operand(inst->src1.vreg, ctx, "r14");
                    emit("cvtsi2sd xmm0, r14");
                    emit("movq r14, xmm0");
                    store_operand(inst->dest.vreg, ctx, "r14");
                    break;
                }
            }

            int to_size = inst->cast_to_type->size;

            int src_is_mem = (strstr(src1, "PTR") != NULL);
            int dst_is_mem = (strstr(dst, "PTR") != NULL);

            const char *work_src = src1;
            const char *work_dst = dst;

            if (src_is_mem) {
                emit("mov r14, %s", src1);
                work_src = "r14";
            }

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

            if (dst_is_mem) {
                emit("mov %s, r15", dst);
            }
            break;

        case IR_SET_SYS_ARG:
            if (inst->dest.imm < 7) {
                load_operand(inst->src1.vreg, ctx, syscall_regs[inst->dest.imm]);
            }
            break;

        case IR_SYSCALL:
            emit("push rcx");
            emit("push r11");
            emit("syscall");
            emit("pop r11");
            emit("pop rcx");
            // Result is in rax, move to dest vreg
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_COPY:
            // dest = dst_addr, src1 = src_addr, src2 = size (imm)
            emit("push rcx");
            emit("push rsi");
            emit("push rdi");
            load_operand(inst->dest.vreg, ctx, "rdi"); // Destination
            load_operand(inst->src1.vreg, ctx, "rsi"); // Source
            emit("mov rcx, %ld", inst->src2.imm);      // Size
            emit("cld");
            emit("rep movsb");
            emit("pop rdi");
            emit("pop rsi");
            emit("pop rcx");
            break;

        case IR_GET_ARG:
            if (inst->src1.imm < num_arg_regs) {
                if (ctx->current_func->is_variadic) {
                    // Variadic: Args are spilled to stack in reverse order
                    get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
                    int slot = ctx->arg_spill_base + (5 - inst->src1.imm);
                    if (strstr(dst, "PTR") != NULL || strstr(dst, "[") != NULL) {
                        emit("mov rax, QWORD PTR [rbp-%d]", slot * 8);
                        emit("mov %s, rax", dst);
                    } else {
                        emit("mov %s, QWORD PTR [rbp-%d]", dst, slot * 8);
                    }
                } else {
                    // Normal: Args are in registers
                    store_operand(inst->dest.vreg, ctx, arg_regs[inst->src1.imm]);
                }
            } else {
                get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
                // Stack args start at [rbp + 16] (skip rbp, ret addr)
                // Arg 6 is at 16, Arg 7 at 24...
                int offset = 16 + (inst->src1.imm - 6) * 8;
                emit("mov %s, QWORD PTR [rbp+%d]", dst, offset);
            }
            break;

        case IR_SET_ARG:
            if (inst->dest.imm < num_arg_regs) {
                load_operand(inst->src1.vreg, ctx, arg_regs[inst->dest.imm]);
            } else {
                load_operand(inst->src1.vreg, ctx, "rax"); // Use rax as temp
                emit("push rax");
            }
            break;

        case IR_CALL:
            // Handle builtins
            if (strcmp(inst->call_target_name, "__builtin_va_start") == 0) {
                // __builtin_va_start(va_list_ptr)
                // va_list struct:
                // 0: gp_offset (i32)
                // 4: fp_offset (i32)
                // 8: overflow_arg_area (*u8)
                // 16: reg_save_area (*u8)
                
                // The pointer to va_list is in rdi (Arg 0 of the intrinsic call).
                // But wait, rdi is used for arguments of the CURRENT function?
                // No, rdi is the argument passed TO __builtin_va_start.
                // So we need to ensure rdi contains the address of 'va' local variable.
                // The IR should have SET_ARG(0, &va) before CALL.
                // So 'rdi' register holds the address.
                
                // We emit code to populate [rdi].
                
                int num_fixed = ctx->current_func->num_args;
                
                // 1. gp_offset = num_fixed * 8
                int gp_offset = num_fixed * 8;
                if (gp_offset > 48) gp_offset = 48; // Cap at 48 if > 6 args
                emit("mov DWORD PTR [rdi], %d", gp_offset);
                
                // 2. fp_offset = 48 (skip floats)
                emit("mov DWORD PTR [rdi+4], 48");
                
                // 3. overflow_arg_area = rbp + 16 + (num_fixed > 6 ? (num_fixed - 6) * 8 : 0)
                // If num_fixed > 6, the first variadic arg is further up the stack.
                int stack_offset = 16;
                if (num_fixed > 6) {
                    stack_offset += (num_fixed - 6) * 8;
                }
                emit("lea rax, [rbp+%d]", stack_offset);
                emit("mov QWORD PTR [rdi+8], rax");
                
                // 4. reg_save_area = rbp - (base + 5) * 8
                // This points to the start of the spill area (where rdi is stored).
                // rdi is at base + 5.
                int slot = ctx->arg_spill_base + 5;
                emit("lea rax, [rbp-%d]", slot * 8);
                emit("mov QWORD PTR [rdi+16], rax");
                
                break;
            }

            // Caller-saved registers are not explicitly saved here because:
            // 1. Our allocator only uses callee-saved registers (rbx, r12, r13) for variables.
            // 2. Scratch registers (r14, r15) don't need preservation across calls.
            // 3. Argument registers (rdi, etc.) are set immediately before call.
            // If we start using caller-saved regs for vars, we must save them here.
            emit("call %s", inst->call_target_name);

            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        default:
            break;
    }
}

static void gen_func(IRFunction *func) {
    AllocCtx ctx = {0};
    ctx.current_func = func;

    // Calculate max stack offset used by variables
    long max_offset = 48; // Start after saved regs
    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        if (inst->op == IR_LOAD || inst->op == IR_ADDR) {
             if (inst->src1.type == OPERAND_IMM) {
                 // For LOAD/ADDR, src1 is offset.
                 // We need to account for the size of the access if possible, but for now assume offset is the start.
                 long off = inst->src1.imm;
                 if (off + 8 > max_offset) max_offset = off + 8;
             }
        }
        if (inst->op == IR_STORE) {
            if (inst->dest.type == OPERAND_IMM) {
                long off = inst->dest.imm;
                if (off + 8 > max_offset) max_offset = off + 8;
            }
        }
    }

    build_intervals(func, &ctx);
    
    // Initialize next_spill to start after user variables
    ctx.next_spill = (max_offset + 7) / 8;
    
    // Reserve space for register arguments
    ctx.arg_spill_base = ctx.next_spill;
    ctx.next_spill += 6;

    // if (ctx.next_spill < 6) ctx.next_spill = 6; // Minimum 6 slots (48 bytes) - Removed for leaf opt

    // DEBUG
    // printf("; max_offset: %ld, next_spill: %d\n", max_offset, ctx.next_spill);

    linear_scan_alloc(func, &ctx);
    
    // Label and prologue are handled in codegen() loop
    
    // Check for leaf function optimization
    int has_call = 0;
    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        if (inst->op == IR_CALL) {
            has_call = 1;
            break;
        }
    }
    // Only optimize if no calls, no locals, no allocs, and arguments fit in registers
    // Also check if stack_slots is small (just saved regs)
    ctx.is_leaf = !has_call && ctx.stack_slots <= 6 && func->alloc_stack_size == 0 && func->num_args <= 6;

    if (!ctx.is_leaf) {
        emit("push rbp");
        emit("mov rbp, rsp");
    }

    emit("push rbx");
    emit("push r12");
    emit("push r13");
    emit("push r14");
    emit("push r15");

    int stack_size = ctx.stack_slots * 8 + func->alloc_stack_size;
    if (stack_size > 0) {
        stack_size = (stack_size + 15) & ~15; // Align to 16 bytes
        emit("sub rsp, %d", stack_size);
    }

    if (func->is_variadic) {
        // Spill ALL 6 register args
            for (int i = 0; i < 6; i++) {
                 // Spill in reverse order so Arg 0 is at lowest address (highest offset)
                 // Arg 0 at base + 5
                 // Arg 5 at base + 0
                 int slot = ctx.arg_spill_base + (5 - i);
                 emit("mov QWORD PTR [rbp-%d], %s", slot * 8, arg_regs[i]);
            }
    } else {
        // Normal function: we don't need to spill all args, only those that the allocator decided to spill?
        // Wait, the allocator handles spilling of LIVE ranges.
        // But the initial values come from registers.
        // If a param is spilled, we need to store it?
        // No, the IR usually has "mov vreg, arg_reg".
        // If vreg is spilled, the "mov" becomes "mov [stack], arg_reg".
        // So we don't need to do anything here for normal functions.
        // The loop I saw before:
        // for (int i = 0; i < 6; i++) { ... }
        // was probably my previous attempt or existing code?
        // Ah, the existing code had:
        /*
        for (int i = 0; i < 6; i++) {
            int slot = ctx.arg_spill_base + i;
            emit("mov QWORD PTR [rbp-%d], %s", slot * 8, arg_regs[i]);
        }
        */
        // This was spilling ALL args ALWAYS. This is inefficient for normal functions but safe.
        // If I keep it, it works for varargs too.
        // But for optimization, we should only do it for varargs OR if we want to be lazy.
        // Let's keep it for now as it was already there (or I misread).
        // Wait, looking at the file content I requested (lines 800-1086), lines 977-980 ARE:
        /*
        for (int i = 0; i < 6; i++) {
            int slot = ctx.arg_spill_base + i;
            emit("mov QWORD PTR [rbp-%d], %s", slot * 8, arg_regs[i]);
        }
        */
        // So it WAS already spilling everything.
        // So `is_variadic` support is implicitly "half-done" by this inefficiency?
        // Yes, if we spill everything, then `va_start` just needs to point to `ctx.arg_spill_base`.
        // So I just need to ensure `ctx.arg_spill_base` is correct and maybe optimize this later.
        // But wait, if I want to implement `va_start` builtin, I need to know WHERE they are.
        // They are at `[rbp - (ctx.arg_spill_base * 8)]` (first arg, rdi)
        // down to `[rbp - ((ctx.arg_spill_base + 5) * 8)]` (6th arg, r9).
        // Since stack grows down, `rbp - small` is higher address than `rbp - large`.
        // `ctx.arg_spill_base` is the start index.
        // `slot * 8` is the offset.
        // `rbp - slot*8`.
        // If `arg_spill_base` is e.g. 1.
        // rdi at `rbp - 8`.
        // rsi at `rbp - 16`.
        // ...
        // This is REVERSE order in memory compared to C array?
        // C array: `args[0]` is lower address?
        // If I want `va_arg` to just increment a pointer...
        // I need them in memory as: `arg1, arg2, arg3...` (increasing address).
        // `rbp - 8` (high addr)
        // `rbp - 16` (lower)
        // So `rdi` is at HIGHER address than `rsi`.
        // So if I have `ptr` pointing to `rdi` (rbp-8), and I do `ptr++` (add 8), I get `rbp`. WRONG.
        // I need `ptr` to go DOWN? Or I need to store them in REVERSE order?
        // If I store `rdi` at `rbp - 48`, `rsi` at `rbp - 40`...
        // Then `rdi` is at lower address.
        // `ptr` at `rdi`. `ptr + 8` is `rsi`.
        // This allows standard `va_arg` logic (pointer increment).
        
        // So I should change the storage order for Variadic Functions (or all).
        // Let's change it for ALL for simplicity, or just Variadic.
        // Existing code: `slot = base + i`. `offset = slot * 8`. `[rbp - offset]`.
        // i=0 (rdi) -> offset small -> High Addr.
        // i=5 (r9) -> offset large -> Low Addr.
        // Memory: [r9] [r8] ... [rsi] [rdi] ... [rbp]
        //           Low <----------- High
        // This is "Stack Growth" order.
        // But for `va_arg` (array-like access), we usually want:
        // [rdi] [rsi] ... [r9]
        // Low -----------> High
        
        // So I should store `rdi` at the LOWEST address (largest offset)?
        // No, `rdi` is the first argument.
        // If I have `fn(a, b, ...)`. `a` is `rdi`. `b` is `rsi`. `...` starts at `rdx`.
        // `va_start` should point to `rdx`.
        // If `rdx` is at `[rbp - 24]` and `rcx` is at `[rbp - 32]`.
        // `ptr = &rdx`. `ptr + 8` = `rbp - 16` (rsi). WRONG.
        // `ptr + 8` should be `rcx`.
        
        // So I need to store them such that `Arg N` is at `Address X`, and `Arg N+1` is at `Address X+8`.
        // So `rdi` (Arg 0) at `Base`.
        // `rsi` (Arg 1) at `Base + 8`.
        // ...
        // `r9` (Arg 5) at `Base + 40`.
        
        // Current: `rdi` at `rbp - 8` (if base=1).
        // `rsi` at `rbp - 16`.
        // `rsi` is at `Address(rdi) - 8`.
        // So addresses are DECREASING.
        
        // FIX: Store them in REVERSE order of `i`.
        // Store `rdi` at `rbp - 48`.
        // Store `rsi` at `rbp - 40`.
        // ...
        // Store `r9` at `rbp - 8`.
        
        // Then `rdi` (Arg 0) is at Low Addr.
        // `rsi` (Arg 1) is at `rdi + 8`.
        // This matches `va_arg` expectation.
        
        // So I will modify the loop.
        // And I should probably only do this for `is_variadic` to avoid breaking existing stuff?
        // Existing stuff doesn't rely on this layout (it uses `arg_regs` directly or `mov` instructions).
        // EXCEPT `IR_GET_ARG` implementation in `gen_inst`.
        // `IR_GET_ARG` uses `ctx.arg_spill_base + inst->src1.imm`.
        // `slot * 8`. `[rbp - slot*8]`.
        // If I change the layout, I must update `IR_GET_ARG` too.
        
        // Let's stick to modifying ONLY `gen_func` and `IR_GET_ARG` logic if needed.
        // Actually, `IR_GET_ARG` is used for retrieving arguments.
        // If I change storage to:
        // `rdi` at `rbp - (base + 5)*8`
        // `r9` at `rbp - (base + 0)*8`
        // Then `IR_GET_ARG(0)` (rdi) should look at `base + 5`.
        
        // Let's just implement `is_variadic` specific spilling logic and leave the default one for non-variadic?
        // But `IR_GET_ARG` is generic.
        // Does `IR_GET_ARG` rely on the spill area?
        // Yes: `int slot = ctx.arg_spill_base + inst->src1.imm; emit("mov %s, QWORD PTR [rbp-%d]", dst, slot * 8);`
        // So currently `IR_GET_ARG` expects `rdi` at `base + 0`.
        
        // If I want to support `va_start`, I should probably just make `va_start` smart enough to handle the decreasing addresses?
        // `va_arg` usually does `val = *ptr; ptr += 8;`.
        // If I make `va_start` return the address of the first variadic arg.
        // And `va_arg` does `ptr -= 8`?
        // That would be non-standard `va_arg` logic (if `va_arg` is a macro/builtin).
        // If `va_arg` is generated by compiler (IR_VA_ARG), I can do whatever I want.
        // But the plan says `__builtin_va_arg`.
        // Usually `va_arg(ap, type)` expands to `*(type*)((ap += 8) - 8)`.
        
        // So standard `va_arg` expects increasing addresses.
        // So I SHOULD fix the layout to be increasing.
        
        // Plan:
        // 1. Change spilling loop to spill in increasing address order (r9 at top/high, rdi at bottom/low).
        //    `r9` at `rbp - (base)*8`.
        //    `r8` at `rbp - (base+1)*8`.
        //    ...
        //    `rdi` at `rbp - (base+5)*8`.
        //    Wait, `rbp - small` is HIGH. `rbp - large` is LOW.
        //    So `r9` (Arg 5) at `rbp - (base)*8` (High).
        //    `rdi` (Arg 0) at `rbp - (base+5)*8` (Low).
        //    This gives: `&rdi < &rsi < ... < &r9`.
        //    This is what we want!
        
        // 2. Update `IR_GET_ARG` to match this new layout.
        //    `IR_GET_ARG(0)` (rdi) -> `slot = base + 5`.
        //    `IR_GET_ARG(i)` -> `slot = base + (5 - i)`.
        
        // 3. Update `gen_func` loop.
        
        // Let's do this.
        
        for (int i = 0; i < 6; i++) {
             // We want Arg 0 (rdi) at Low Addr (High Offset).
             // We want Arg 5 (r9) at High Addr (Low Offset).
             // Slot 0 (base) -> High Addr.
             // Slot 5 (base+5) -> Low Addr.
             // So Arg i should be at Slot (5 - i).
             int slot = ctx.arg_spill_base + (5 - i);
             emit("mov QWORD PTR [rbp-%d], %s", slot * 8, arg_regs[i]);
        }
    }


    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        gen_inst(inst, &ctx, func->alloc_stack_size);
    }

    // Implicit return for void functions (or fallthrough safety)
    IRInst *last = func->instructions;
    while (last && last->next) last = last->next;
    
    if (!last || last->op != IR_RET) {
        // Emit epilogue
        int stack_size = ctx.stack_slots * 8 + func->alloc_stack_size;
        if (stack_size > 0) {
            stack_size = (stack_size + 15) & ~15;
            emit("add rsp, %d", stack_size);
        }
        emit("pop r15");
        emit("pop r14");
        emit("pop r13");
        emit("pop r12");
        emit("pop rbx");
        if (!ctx.is_leaf) {
            emit("pop rbp");
        }
        emit("ret");
    }

    // Cleanup
    if (ctx.intervals) free(ctx.intervals);
    if (ctx.regs) free(ctx.regs);
    if (ctx.vreg_to_loc) free(ctx.vreg_to_loc);
}

static void emit_data_section(IRProgram *prog) {
    if (!prog->globals) return;

    fprintf(out, ".section .data\n");
    for (IRGlobal *g = prog->globals; g; g = g->next) {
        if (!g->is_reachable) continue;
        if (g->is_initialized) {
            fprintf(out, ".global %s\n", g->name);
            fprintf(out, "%s:\n", g->name);
            if (g->size == 1) fprintf(out, "  .byte %ld\n", g->init_value);
            else if (g->size == 2) fprintf(out, "  .value %ld\n", g->init_value);
            else if (g->size == 4) fprintf(out, "  .long %ld\n", g->init_value);
            else if (g->size == 8) fprintf(out, "  .quad %ld\n", g->init_value);
            else fprintf(out, "  .zero %d\n", g->size);
        }
    }

    fprintf(out, ".section .bss\n");
    for (IRGlobal *g = prog->globals; g; g = g->next) {
        if (!g->is_reachable) continue;
        if (!g->is_initialized) {
            fprintf(out, ".global %s\n", g->name);
            fprintf(out, "%s:\n", g->name);
            fprintf(out, "  .zero %d\n", g->size);
        }
    }
}

static void emit_rodata(IRProgram *prog) {
    if (!prog->strings) return;

    fprintf(out, ".section .rodata\n");
    for (StringLiteral *s = prog->strings; s; s = s->next) {
        fprintf(out, ".LC%d:\n", s->id);
        fprintf(out, "  .string \"");
        for (char *p = s->value; *p; p++) {
            if (*p == '\n') fprintf(out, "\\n");
            else if (*p == '\t') fprintf(out, "\\t");
            else if (*p == '\"') fprintf(out, "\\\"");
            else if (*p == '\\') fprintf(out, "\\\\");
            else fprintf(out, "%c", *p);
        }
        fprintf(out, "\"\n");
    }
    fprintf(out, ".text\n");
}

void codegen(IRProgram *prog, FILE *output) {
    out = output;

    fprintf(out, ".intel_syntax noprefix\n");
    
    emit_rodata(prog);
    emit_data_section(prog);

    fprintf(out, ".text\n");
    fprintf(out, ".globl main\n");
    
    for (IRFunction *func = prog->functions; func; func = func->next) {
        if (!func->is_reachable) continue;
        if (func->asm_name) {
            fprintf(out, "%s:\n", func->asm_name);
        } else {
            fprintf(out, "%s:\n", func->name);
        }
        

        
        gen_func(func);
        fprintf(out, "\n");
    }
}
