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

static void gen_inst(IRInst *inst, AllocCtx *ctx) {
    char dst[64], src1[64];

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
                        else emit("mov %sd, DWORD PTR [r15]", target_reg);
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
                            emit("mov %sd, DWORD PTR [rbp-%ld]", target_reg, inst->src1.imm);
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

        case IR_SET_SYS_ARG:
            if (inst->dest.imm < 7) {
                load_operand(inst->src1.vreg, ctx, syscall_regs[inst->dest.imm]);
            }
            break;

        case IR_SYSCALL:
            emit("syscall");
            // Result is in rax, move to dest vreg
            store_operand(inst->dest.vreg, ctx, "rax");
            break;

        case IR_COPY:
            // dest = dst_addr, src1 = src_addr, src2 = size (imm)
            load_operand(inst->dest.vreg, ctx, "rdi"); // Destination
            load_operand(inst->src1.vreg, ctx, "rsi"); // Source
            emit("mov rcx, %ld", inst->src2.imm);      // Size
            emit("cld");
            emit("rep movsb");
            break;

        case IR_GET_ARG:
            if (inst->src1.imm < num_arg_regs) {
                get_operand_loc(inst->dest.vreg, ctx, dst, sizeof(dst));
                emit("mov %s, %s", dst, arg_regs[inst->src1.imm]);
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
            // Caller-saved registers are not explicitly saved here because:
            // 1. Our allocator only uses callee-saved registers (rbx, r12, r13) for variables.
            // 2. Scratch registers (r14, r15) don't need preservation across calls.
            // 3. Argument registers (rdi, etc.) are set immediately before call.
            // If we start using caller-saved regs for vars, we must save them here.
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

    // Calculate max stack offset used by variables
    long max_offset = 48; // Start after saved regs
    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        if (inst->op == IR_LOAD || inst->op == IR_ADDR) {
             if (inst->src1.type == OPERAND_IMM) {
                 // For LOAD/ADDR, src1 is offset.
                 // We need to account for the size of the access if possible, but for now assume offset is the start.
                 // Actually, codegen adds size to offset for rbp access: [rbp - (offset + size)]
                 // So we need to find max (offset + size).
                 // Since we don't easily know size here without type, let's approximate.
                 // Semantic assigns offsets sequentially. The max offset seen is the start of the last var.
                 // We need to add its size. Let's assume max size 8 for safety if unknown.
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
    if (ctx.next_spill < 6) ctx.next_spill = 6; // Minimum 6 slots (48 bytes)

    // DEBUG
    // printf("; max_offset: %ld, next_spill: %d\n", max_offset, ctx.next_spill);

    linear_scan_alloc(func, &ctx);
    
    // Label and prologue are handled in codegen() loop
    
    emit("push rbx");
    emit("push r12");
    emit("push r13");
    emit("push r14");
    emit("push r15");

    int stack_size = ctx.stack_slots * 8;
    if (stack_size > 0) {
        stack_size = (stack_size + 15) & ~15; // Align to 16 bytes
        emit("sub rsp, %d", stack_size);
    }

    for (IRInst *inst = func->instructions; inst; inst = inst->next) {
        gen_inst(inst, &ctx);
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
        fprintf(out, "  .string \"%s\"\n", s->value);
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
        
        emit("push rbp");
        emit("mov rbp, rsp");
        
        gen_func(func);
        fprintf(out, "\n");
    }
}
