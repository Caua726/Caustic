#include "semantic.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Scope {
    Variable *vars;
    struct Scope *parent;
} Scope;

static Scope *current_scope = NULL;
static int stack_offset = 0;
static Function *function_table = NULL;

Type *type_int;
Type *type_i8;
Type *type_i16;
Type *type_i32;
Type *type_i64;
Type *type_u8;
Type *type_u16;
Type *type_u32;
Type *type_u64;
Type *type_f32;
Type *type_f64;
Type *type_bool;
Type *type_char;
Type *type_string;
Type *type_void;

static Type *new_type_full(TypeKind kind, int size, int is_signed) {
    Type *ty = new_type(kind);
    ty->size = size;
    ty->is_signed = is_signed;
    return ty;
}

static void declare_function(char *name, Type *return_type, Type **param_types, int param_count) {
    for (Function *fs = function_table; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) {
            fprintf(stderr, "Erro: redefinição da função '%s'\n", name);
            exit(1);
        }
    }
    Function *fs = calloc(1, sizeof(Function));
    fs->name = strdup(name);
    fs->return_type = return_type;
    fs->param_types = param_types;
    fs->param_count = param_count;
    fs->next = function_table;
    function_table = fs;
}

static Function *lookup_function(char *name) {
    for (Function *fs = function_table; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) {
            return fs;
        }
    }
    return NULL;
}

void types_init() {
    type_i8 = new_type_full(TY_I8, 1, 1);
    type_i16 = new_type_full(TY_I16, 2, 1);
    type_i32 = new_type_full(TY_I32, 4, 1);
    type_i64 = new_type_full(TY_I64, 8, 1);
    type_u8 = new_type_full(TY_U8, 1, 0);
    type_u16 = new_type_full(TY_U16, 2, 0);
    type_u32 = new_type_full(TY_U32, 4, 0);
    type_u64 = new_type_full(TY_U64, 8, 0);
    type_f32 = new_type_full(TY_F32, 4, 1);
    type_f64 = new_type_full(TY_F64, 8, 1);
    type_bool = new_type_full(TY_BOOL, 1, 0);
    type_char = new_type_full(TY_CHAR, 1, 1);
    type_string = new_type_full(TY_STRING, 8, 0);
    type_void = new_type_full(TY_VOID, 0, 0);
    type_int = type_i32;
}

static int is_safe_implicit_conversion(Type *from, Type *to) {
    if (from == to) return 1;

    if (from->size > to->size) return 0;

    if (from->is_signed != to->is_signed) return 0;

    return 1;
}

static void symtab_init() {
    current_scope = NULL;
    stack_offset = 48; // Reserve 48 bytes for saved registers (rbx, r12-r15) + alignment
}

static void symtab_enter_scope() {
    Scope *new_scope = calloc(1, sizeof(Scope));
    new_scope->parent = current_scope;
    current_scope = new_scope;
}

static void symtab_exit_scope() {
    if (!current_scope) return;

    Scope *old_scope = current_scope;
    current_scope = current_scope->parent;

    Variable *var = old_scope->vars;
    while (var) {
        Variable *next = var->next;
        free(var->name);
        free(var);
        var = next;
    }
    free(old_scope);
}

static Variable *symtab_declare(char *name, Type *type, VarFlags flags) {
    if (!current_scope) {
        fprintf(stderr, "Erro interno: sem escopo ativo\n");
        exit(1);
    }

    for (Variable *v = current_scope->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            fprintf(stderr, "Erro: variavel '%s' ja declarada neste escopo\n", name);
            exit(1);
        }
    }

    Variable *var = calloc(1, sizeof(Variable));
    var->name = strdup(name);
    var->type = type;
    var->flags = flags;
    var->offset = stack_offset;
    stack_offset += type->size;

    var->next = current_scope->vars;
    current_scope->vars = var;

    return var;
}

static Variable *symtab_lookup(char *name) {
    for (Scope *scope = current_scope; scope; scope = scope->parent) {
        for (Variable *v = scope->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                return v;
            }
        }
    }
    return NULL;
}

static void register_functions(Node *program_node) {
    for (Node *node = program_node; node; node = node->next) {
        if (node->kind == NODE_KIND_FN) {
            int param_count = 0;
            for (Node *p = node->params; p; p = p->next) param_count++;
            
            Type **param_types = calloc(param_count, sizeof(Type*));
            int i = 0;
            for (Node *p = node->params; p; p = p->next) {
                param_types[i++] = p->ty;
            }

            declare_function(node->name, node->return_type, param_types, param_count);
        }
    }
}

static void walk(Node *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case NODE_KIND_NUM:
            node->ty = type_i32;
            return;

        case NODE_KIND_IDENTIFIER: {
            Variable *var = symtab_lookup(node->name);
            if (!var) {
                fprintf(stderr, "Erro: variavel '%s' nao declarada\n", node->name);
                exit(1);
            }
            node->var = var;
            node->ty = var->type;
            node->offset = var->offset;
            return;
        }

        case NODE_KIND_RETURN:
            walk(node->expr);
            return;

        case NODE_KIND_FN:
            symtab_enter_scope();
            // Declarar parâmetros no escopo da função
            for (Node *param = node->params; param; param = param->next) {
                Variable *var = symtab_declare(param->name, param->ty, param->flags);
                param->var = var;
                param->offset = var->offset;
            }
            walk(node->body);
            symtab_exit_scope();
            return;

        case NODE_KIND_BLOCK:
            symtab_enter_scope();
            for (Node *stmt = node->stmts; stmt; stmt = stmt->next) {
                walk(stmt);
            }
            symtab_exit_scope();
            return;

        case NODE_KIND_LET: {
            if (node->init_expr) {
                walk(node->init_expr);

                int allow_literal_narrowing = 0;
                if (node->init_expr->kind == NODE_KIND_NUM) {
                    long val = node->init_expr->val;
                    if (node->ty == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                    else if (node->ty == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                    else if (node->ty == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                    else if (node->ty == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                    else if (node->ty == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                    else if (node->ty == type_u32 && val >= 0 && val <= 4294967295ULL) allow_literal_narrowing = 1;
                }

                if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->init_expr->ty, node->ty)) {
                    fprintf(stderr, "Erro: conversão implícita não segura ao inicializar variável. Use cast explícito.\n");
                    exit(1);
                }
            }
            Variable *var = symtab_declare(node->name, node->ty, node->flags);
            node->var = var;
            node->offset = var->offset;
            return;
        }

        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
        case NODE_KIND_MOD:
        case NODE_KIND_EQ:
        case NODE_KIND_NE:
        case NODE_KIND_LT:
        case NODE_KIND_LE:
        case NODE_KIND_GT:
        case NODE_KIND_GE:
            walk(node->lhs);
            walk(node->rhs);

            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind != NODE_KIND_NUM) {
                long val = node->lhs->val;
                Type *target = node->rhs->ty;
                if ((target == type_i8 && val >= -128 && val <= 127) ||
                    (target == type_i16 && val >= -32768 && val <= 32767) ||
                    (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) ||
                    (target == type_u8 && val >= 0 && val <= 255) ||
                    (target == type_u16 && val >= 0 && val <= 65535) ||
                    (target == type_u32 && val >= 0 && val <= 4294967295ULL)) {
                    node->lhs->ty = target;
                }
            }

            if (node->rhs->kind == NODE_KIND_NUM && node->lhs->kind != NODE_KIND_NUM) {
                long val = node->rhs->val;
                Type *target = node->lhs->ty;
                if ((target == type_i8 && val >= -128 && val <= 127) ||
                    (target == type_i16 && val >= -32768 && val <= 32767) ||
                    (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) ||
                    (target == type_u8 && val >= 0 && val <= 255) ||
                    (target == type_u16 && val >= 0 && val <= 65535) ||
                    (target == type_u32 && val >= 0 && val <= 4294967295ULL)) {
                    node->rhs->ty = target;
                }
            }

            Type *target_type;
            if (node->lhs->ty->size > node->rhs->ty->size) {
                target_type = node->lhs->ty;
            } else {
                target_type = node->rhs->ty;
            }

            if (!is_safe_implicit_conversion(node->lhs->ty, target_type)) {
                fprintf(stderr, "Erro: conversão implícita não segura de tipo menor/incompatível para maior. Use cast explícito.\n");
                exit(1);
            }
            if (!is_safe_implicit_conversion(node->rhs->ty, target_type)) {
                fprintf(stderr, "Erro: conversão implícita não segura de tipo menor/incompatível para maior. Use cast explícito.\n");
                exit(1);
            }

            node->ty = target_type;
            return;

        case NODE_KIND_IF: {
            walk(node->if_stmt.cond);
            TypeKind tk = node->if_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do if deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            walk(node->if_stmt.then_b);
            if (node->if_stmt.else_b) {
                walk(node->if_stmt.else_b);
            }
            return;
        }
        case NODE_KIND_WHILE: {
            walk(node->while_stmt.cond);
            TypeKind tk = node->while_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do while deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            walk(node->while_stmt.body);
            return;
        }
        case NODE_KIND_ADDR:
            walk(node->expr);
            if (node->expr->kind != NODE_KIND_IDENTIFIER && 
                node->expr->kind != NODE_KIND_DEREF && 
                node->expr->kind != NODE_KIND_INDEX) {
                fprintf(stderr, "Erro: operador '&' requer um lvalue (variável, dereferência ou índice).\n");
                exit(1);
            }
            // Create pointer type
            Type *ptr_ty = new_type(TY_PTR);
            ptr_ty->base = node->expr->ty;
            ptr_ty->size = 8;
            node->ty = ptr_ty;
            return;

        case NODE_KIND_DEREF:
            walk(node->expr);
            if (node->expr->ty->kind != TY_PTR) {
                fprintf(stderr, "Erro: operador '*' requer um ponteiro.\n");
                exit(1);
            }
            node->ty = node->expr->ty->base;
            return;

        case NODE_KIND_INDEX:
            walk(node->lhs);
            walk(node->rhs);

            if (node->lhs->ty->kind != TY_ARRAY && node->lhs->ty->kind != TY_PTR) {
                fprintf(stderr, "Erro: indexação requer array ou ponteiro.\n");
                exit(1);
            }

            // Check index type (must be integer)
            TypeKind idx_kind = node->rhs->ty->kind;
            if (idx_kind != TY_I32 && idx_kind != TY_I64 && idx_kind != TY_U32 && idx_kind != TY_U64 && idx_kind != TY_INT) {
                 fprintf(stderr, "Erro: índice deve ser um inteiro.\n");
                 exit(1);
            }

            node->ty = node->lhs->ty->base;
            return;

        case NODE_KIND_ASSIGN: {
            walk(node->lhs);
            walk(node->rhs);

            // Check if LHS is a valid lvalue
            if (node->lhs->kind != NODE_KIND_IDENTIFIER && 
                node->lhs->kind != NODE_KIND_DEREF && 
                node->lhs->kind != NODE_KIND_INDEX) {
                fprintf(stderr, "Erro: alvo da atribuição inválido (deve ser variável, dereferência ou índice).\n");
                exit(1);
            }

            if (node->lhs->kind == NODE_KIND_IDENTIFIER) {
                Variable *var = node->lhs->var;
                if (!(var->flags & VAR_FLAG_MUT)) {
                    fprintf(stderr, "Erro: não é possível atribuir à variável imutável '%s'.\n", var->name);
                    exit(1);
                }
            }
            // Note: For DEREF and INDEX, we assume memory is mutable for now, 
            // or we'd need to track mutability in the type system (e.g. *mut T vs *T).
            // For now, let's allow it.

            int allow_literal_narrowing = 0;
            if (node->rhs->kind == NODE_KIND_NUM) {
                long val = node->rhs->val;
                Type *target = node->lhs->ty;
                if (target == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                else if (target == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                else if (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                else if (target == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                else if (target == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                else if (target == type_u32 && val >= 0 && val <= 4294967295ULL) allow_literal_narrowing = 1;
            }

            if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->rhs->ty, node->lhs->ty)) {
                fprintf(stderr, "Erro: conversão implícita não segura na atribuição. Use cast explícito.\n");
                exit(1);
            }

            node->ty = node->lhs->ty;
            return;
        }

        case NODE_KIND_CAST:
            walk(node->expr);
            return;

        case NODE_KIND_FNCALL:
            Function *fs = lookup_function(node->name);
            if (!fs) {
                fprintf(stderr, "Erro: chamada para função não definida '%s'\n", node->name);
                exit(1);
            }
            node->ty = fs->return_type;

            int arg_count = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                walk(arg);
                arg_count++;
            }

            if (arg_count != fs->param_count) {
                fprintf(stderr, "Erro: função '%s' espera %d argumentos, mas recebeu %d\n", fs->name, fs->param_count, arg_count);
                exit(1);
            }

            Node *arg = node->args;
            for (int i = 0; i < arg_count; i++) {
                if (!is_safe_implicit_conversion(arg->ty, fs->param_types[i])) {
                    fprintf(stderr, "Erro: argumento %d da função '%s' tem tipo incompatível.\n", i + 1, fs->name);
                    exit(1);
                }
                arg = arg->next;
            }
            return;

        case NODE_KIND_SYSCALL:
            node->ty = type_i64; // Syscalls always return i64
            for (Node *arg = node->args; arg; arg = arg->next) {
                walk(arg);
            }
            return;
    }
}

void analyze(Node *node) {
    symtab_init();
    function_table = NULL;

    // Primeiro passo: registrar todas as funções
    for (Node *fn_node = node; fn_node; fn_node = fn_node->next) {
        if (fn_node->kind == NODE_KIND_FN) {
            int param_count = 0;
            for (Node *p = fn_node->params; p; p = p->next) param_count++;
            
            Type **param_types = calloc(param_count, sizeof(Type*));
            int i = 0;
            for (Node *p = fn_node->params; p; p = p->next) {
                param_types[i++] = p->ty;
            }

            declare_function(fn_node->name, fn_node->return_type, param_types, param_count);
        }
    }

    // Segundo passo: analisar cada função
    for (Node *fn_node = node; fn_node; fn_node = fn_node->next) {
        if (fn_node->kind == NODE_KIND_FN) {
            walk(fn_node);
        }
    }
}

void semantic_cleanup() {
    Function *fs = function_table;
    while (fs) {
        Function *next = fs->next;
        free(fs->name);
        if (fs->param_types) free(fs->param_types);
        free(fs);
        fs = next;
    }
    function_table = NULL;
    
    // Limpar tipos se necessário (mas eles são globais/estáticos por enquanto, então talvez não precise)
    // Se types_init alocasse dinamicamente cada vez, precisaria limpar.
    // Como são alocados uma vez, o SO limpa no final. Mas para ser purista:
    free(type_i8);
    free(type_i16);
    free(type_i32);
    free(type_i64);
    free(type_u8);
    free(type_u16);
    free(type_u32);
    free(type_u64);
    free(type_f32);
    free(type_f64);
    free(type_bool);
    free(type_char);
    free(type_string);
    free(type_void);
}
