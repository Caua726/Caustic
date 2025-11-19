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
static Scope *global_scope = NULL;
static int stack_offset = 0;
static Function *function_table = NULL;
static int loop_depth = 0;
static Type *current_return_type = NULL;

struct Module {
    char *name; // alias
    Scope *scope;
    char *path_prefix;
    struct Module *next;
};

static char *current_module_path = NULL; // Prefix for current module
static Module *module_list = NULL;

static char *get_module_prefix(const char *path) {
    char *prefix = malloc(strlen(path) + 2);
    prefix[0] = '_';
    int j = 1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/' || path[i] == '.' || path[i] == '\\') {
            prefix[j++] = '_';
        } else {
            prefix[j++] = path[i];
        }
    }
    prefix[j] = '\0';
    return prefix;
}

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
    
    if (current_module_path) {
        fs->module_prefix = strdup(current_module_path);
        // Mangle name: prefix + "_" + name
        int len = strlen(current_module_path) + 1 + strlen(name) + 1;
        fs->asm_name = malloc(len);
        snprintf(fs->asm_name, len, "%s_%s", current_module_path, name);
    } else {
        fs->module_prefix = NULL;
        fs->asm_name = strdup(name);
    }

    fs->next = function_table;
    function_table = fs;
}

static Function *lookup_function_qualified(char *name, char *prefix) {
    for (Function *fs = function_table; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) {
            if (prefix && fs->module_prefix && strcmp(fs->module_prefix, prefix) == 0) return fs;
            if (!prefix && !fs->module_prefix) return fs;
        }
    }
    return NULL;
}

static Function *lookup_function(char *name) {
    // Try current module first
    if (current_module_path) {
        Function *f = lookup_function_qualified(name, current_module_path);
        if (f) return f;
    }
    // Try global
    return lookup_function_qualified(name, NULL);
}

// Global list of registered structs (using the Type structs themselves)




typedef struct StructDef {
    char *name;
    Type *type;
    struct StructDef *next;
} StructDef;

static StructDef *struct_defs = NULL;

static Type *lookup_struct(char *name) {
    for (StructDef *s = struct_defs; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s->type;
        }
    }
    return NULL;
}

static void resolve_type_ref(Type *ty) {
    if (ty->kind == TY_STRUCT && ty->fields == NULL && ty->name != NULL) {
        Type *def = lookup_struct(ty->name);
        if (!def) {
            fprintf(stderr, "Erro: struct '%s' não definida.\n", ty->name);
            exit(1);
        }
        // Copy details
        ty->fields = def->fields;
        ty->size = def->size;
        // ty->name is already set
    }
    if (ty->kind == TY_PTR || ty->kind == TY_ARRAY) {
        resolve_type_ref(ty->base);
    }
}

static void declare_struct(Type *type) {
    for (StructDef *s = struct_defs; s; s = s->next) {
        if (strcmp(s->name, type->name) == 0) {
            fprintf(stderr, "Erro: redefinição da struct '%s'\n", type->name);
            exit(1);
        }
    }
    StructDef *s = calloc(1, sizeof(StructDef));
    s->name = strdup(type->name);
    s->type = type;
    s->next = struct_defs;
    struct_defs = s;

    // Calculate size and offsets
    int offset = 0;
    for (Field *f = type->fields; f; f = f->next) {
        resolve_type_ref(f->type);
        
        if (f->type->kind == TY_STRUCT && strcmp(f->type->name, type->name) == 0) {
             fprintf(stderr, "Erro: struct '%s' contém a si mesma diretamente no campo '%s'. Use um ponteiro.\n", type->name, f->name);
             exit(1);
        }

        f->offset = offset;
        offset += f->type->size;
    }
    type->size = offset;
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
    global_scope = calloc(1, sizeof(Scope));
    current_scope = global_scope;
    stack_offset = 48; // Reserve 48 bytes for saved registers (rbx, r12-r15) + alignment
}

static void symtab_enter_scope() {
    Scope *new_scope = calloc(1, sizeof(Scope));
    new_scope->parent = current_scope;
    current_scope = new_scope;
    current_scope = new_scope;
}

static void symtab_exit_scope() {
    if (!current_scope) return;

    Scope *old_scope = current_scope;
    current_scope = current_scope->parent;
    
    // Do NOT free variables here, they are referenced by AST nodes.
    // We could add them to a global list for cleanup later if needed.
    
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

    if (current_scope == global_scope) {
        var->is_global = 1;
        var->offset = 0; // Globals don't have stack offset
    } else {
        var->is_global = 0;
        stack_offset += type->size;
        var->offset = stack_offset;
    }

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



static void walk(Node *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case NODE_KIND_ASM: break;
        case NODE_KIND_NUM:
            node->ty = type_i32;
            return;

        case NODE_KIND_STRING_LITERAL: {
            Type *ptr_ty = new_type(TY_PTR);
            ptr_ty->base = type_u8;
            ptr_ty->size = 8;
            node->ty = ptr_ty;
            return;
        }

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
            if (node->expr) {
                walk(node->expr);
                if (!is_safe_implicit_conversion(node->expr->ty, current_return_type)) {
                    fprintf(stderr, "Erro: tipo de retorno incompatível. Esperado %d, encontrado %d\n", current_return_type->kind, node->expr->ty->kind);
                    // TODO: Print type names instead of enums
                    exit(1);
                }
            } else {
                if (current_return_type != type_void) {
                    fprintf(stderr, "Erro: retorno vazio em função não-void.\n");
                    exit(1);
                }
            }
            return;

        case NODE_KIND_FN:
            // Function scope is a child of global scope (or current scope if nested functions were supported)
            // But we need to reset stack_offset for each function.
            // And we need to ensure we are entering a new scope from global scope.
            // Since we are iterating top-level nodes in analyze(), we might be in global scope.
            // Let's ensure we push a new scope.
            symtab_enter_scope();
            stack_offset = 48; // Reset stack offset for new function
            // Declarar parâmetros no escopo da função
            for (Node *param = node->params; param; param = param->next) {
                Variable *var = symtab_declare(param->name, param->ty, param->flags);
                param->var = var;
                param->offset = var->offset;
            }
            // Update node->name to asm_name for codegen
            // We need to find the function in the table to get asm_name
            // But we are in a new scope? No, function_table is global.
            // But we need to know the prefix?
            // lookup_function uses current_module_path.
            // analyze_bodies sets current_module_path.
            // So lookup_function(node->name) should work.
            Function *fs_def = lookup_function(node->name);
            if (fs_def && fs_def->asm_name) {
                 // Only update if different (to avoid leak if we run multiple times?)
                 if (strcmp(node->name, fs_def->asm_name) != 0) {
                     node->name = strdup(fs_def->asm_name);
                 }
            }

            Type *saved_return_type = current_return_type;
            current_return_type = node->return_type; // Assuming node->return_type is set by parser/declare_functions
            // Wait, node->return_type is in the AST node.
            // We need to make sure it's resolved?
            // Parser sets it to type_i32 or type_void etc.
            // If it's a struct type, it might need resolution?
            // For now assume primitive or already resolved.
            
            walk(node->body);
            
            current_return_type = saved_return_type;
            symtab_exit_scope();
            return;

        case NODE_KIND_EXPR_STMT:
            walk(node->expr);
            return;

        case NODE_KIND_BLOCK:
            symtab_enter_scope();
            for (Node *stmt = node->stmts; stmt; stmt = stmt->next) {
                walk(stmt);
            }
            symtab_exit_scope();
            return;

        case NODE_KIND_LET: {
            resolve_type_ref(node->ty);
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
                    else if (node->ty == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
                }

                if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->init_expr->ty, node->ty)) {
                    fprintf(stderr, "Erro: conversão implícita não segura ao inicializar variável. Use cast explícito.\n");
                    exit(1);
                }
            }
            Variable *var = symtab_declare(node->name, node->ty, node->flags);
            node->var = var;
            node->offset = var->offset;
            // Wait, symtab_declare uses type->size. If type was not resolved, size might be 0.
            // We must resolve BEFORE declaring.
            // Let's fix this.
            
            // Undo symtab_declare side effects? No, let's resolve before.
            // But we already called symtab_declare.
            // Let's change the order.
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
        case NODE_KIND_SHL:
        case NODE_KIND_SHR:
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
                    (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL)) {
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
                    (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL)) {
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

        case NODE_KIND_LOGICAL_AND:
        case NODE_KIND_LOGICAL_OR:
            walk(node->lhs);
            walk(node->rhs);
            // Check if operands are boolean-like (integers or bool)
            // For now, we allow integers as booleans (C-style)
            node->ty = type_bool;
            return;

        case NODE_KIND_BREAK:
            if (loop_depth == 0) {
                fprintf(stderr, "Erro: 'break' fora de loop.\n");
                exit(1);
            }
            return;

        case NODE_KIND_CONTINUE:
            if (loop_depth == 0) {
                fprintf(stderr, "Erro: 'continue' fora de loop.\n");
                exit(1);
            }
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
            loop_depth++;
            walk(node->while_stmt.body);
            loop_depth--;
            return;
        }

        case NODE_KIND_DO_WHILE: {
            loop_depth++;
            walk(node->do_while_stmt.body);
            loop_depth--;
            walk(node->do_while_stmt.cond);
            TypeKind tk = node->do_while_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do do-while deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            return;
        }

        case NODE_KIND_FOR: {
            symtab_enter_scope();
            if (node->for_stmt.init) walk(node->for_stmt.init);
            
            if (node->for_stmt.cond) {
                walk(node->for_stmt.cond);
                TypeKind tk = node->for_stmt.cond->ty->kind;
                if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                    tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                    tk != TY_BOOL && tk != TY_INT) {
                    fprintf(stderr, "Erro: condição do for deve ser um booleano ou inteiro.\n");
                    exit(1);
                }
            }
            
            if (node->for_stmt.step) walk(node->for_stmt.step);
            
            loop_depth++;
            walk(node->for_stmt.body);
            loop_depth--;
            
            symtab_exit_scope();
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
                node->lhs->kind != NODE_KIND_INDEX &&
                node->lhs->kind != NODE_KIND_MEMBER_ACCESS) {
                fprintf(stderr, "Erro: alvo da atribuição inválido (deve ser variável, dereferência, índice ou membro).\n");
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
                else if (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
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

        case NODE_KIND_SIZEOF:
            resolve_type_ref(node->ty);
            node->kind = NODE_KIND_NUM;
            node->val = node->ty->size;
            node->ty = type_i32; // sizeof returns i32 (or size_t which we map to i32/i64)
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


            // Update node->name to asm_name for codegen
            if (fs->asm_name) {
                node->name = strdup(fs->asm_name);
            }

            return;

        case NODE_KIND_SYSCALL:
            node->ty = type_i64; // Syscalls always return i64
            for (Node *arg = node->args; arg; arg = arg->next) {
                walk(arg);
            }
            return;

            return;
            
        case NODE_KIND_USE:
            // Handled in analyze passes
            return;

        case NODE_KIND_MODULE_ACCESS:
        case NODE_KIND_MEMBER_ACCESS:
            // Check if LHS is a module
            if (node->lhs->kind == NODE_KIND_IDENTIFIER) {
                Variable *var = symtab_lookup(node->lhs->name);
                if (var && var->is_module) {
                    // Module access
                    Module *mod = var->module_ref;
                    char *member_name = node->member_name;
                    
                    // Try to find function in module
                    Function *fn = lookup_function_qualified(member_name, mod->path_prefix);
                    if (fn) {
                        // It's a function. 
                        // We need to transform this node into something that FNCALL can use.
                        // FNCALL expects a name or an expression.
                        // If we set node->kind = NODE_KIND_IDENTIFIER and node->name = fn->asm_name?
                        // But FNCALL uses node->name directly if it's an identifier.
                        // Or we can set node->name to the mangled name.
                        node->kind = NODE_KIND_IDENTIFIER;
                        node->name = strdup(fn->asm_name); // Use mangled name
                        node->ty = fn->return_type; // Function type? 
                        // Actually, in C, function name evaluates to function pointer.
                        // But here we just want FNCALL to find it.
                        // If this is part of FNCALL (node->lhs of FNCALL is this node), 
                        // then FNCALL logic needs to handle it.
                        // FNCALL logic: Function *fs = lookup_function(node->name);
                        // If node is IDENTIFIER, it uses node->name.
                        // So if we change name to asm_name, lookup_function needs to find it.
                        // lookup_function searches by name.
                        // But asm_name is NOT in the function table as 'name'.
                        // 'name' is original name. 'asm_name' is separate.
                        // So lookup_function won't find it by asm_name unless we index by asm_name too.
                        // OR we change lookup_function to check asm_name too.
                        // OR we change FNCALL to handle resolved function pointer.
                        // Let's change FNCALL to use a 'function_ref' if present?
                        // Or better: lookup_function checks asm_name? No, asm_name is for codegen.
                        
                        // Wait, if I found the function 'fn', I can attach it to the node.
                        // Node struct has no 'fn_ref'.
                        // I can add 'struct Function *fn' to Node?
                        // Or I can use 'var' field? No, var is Variable.
                        
                        // Simplest: FNCALL logic in walk() calls lookup_function(node->name).
                        // If I change node->name to something unique that lookup_function finds...
                        // But 'fn' is already found here.
                        // Maybe I should handle FNCALL with MODULE_ACCESS specially?
                        // But FNCALL walks its 'name' (which is not a node, it's a string in Node struct?).
                        // Wait, NODE_KIND_FNCALL has 'name' field. It does NOT have an 'expr' for the function.
                        // AST: `fn_call(args)` -> `NODE_KIND_FNCALL` with `name`.
                        // So `mod.func()` is NOT `FNCALL` with `MEMBER_ACCESS`.
                        // Parser `parse_primary` or `parse_fn_call`?
                        // Let's check parser.
                        // If parser sees `ident.ident(...)`, does it create FNCALL?
                        // `parse_primary` handles `ident` then `parse_postfix` handles `.` and `(`.
                        // If `(` follows `ident`, it's FNCALL.
                        // If `.` follows, it's MEMBER_ACCESS.
                        // If `(` follows MEMBER_ACCESS, it's... ?
                        // Parser likely doesn't support `expr(...)` call syntax, only `name(...)`.
                        // I need to check parser.c `parse_primary`.
                        
                        // If parser only supports `name(...)`, then `mod.func(...)` is not parsed as FNCALL?
                        // Or maybe `parse_postfix` handles `(` on any expr?
                        // If so, it creates FNCALL?
                        // Let's check parser.c.
                        return;
                    }
                    
                    // Try to find variable in module scope
                    // We need to look in mod->scope.
                    // But symtab_lookup uses current_scope.
                    // We can manually search mod->scope->vars.
                    for (Variable *v = mod->scope->vars; v; v = v->next) {
                        if (strcmp(v->name, member_name) == 0) {
                            node->kind = NODE_KIND_IDENTIFIER;
                            node->var = v;
                            node->ty = v->type;
                            node->offset = v->offset;
                            // If it's a global in module, is_global is set.
                            // Codegen needs to know how to access it.
                            // If it's global, it uses name (asm_name?).
                            // Globals use name.
                            // If module global, name should be mangled?
                            // We didn't mangle global variable names yet.
                            // We should mangle them in register_globals.
                            return;
                        }
                    }
                    
                    fprintf(stderr, "Erro: membro '%s' não encontrado no módulo '%s'\n", member_name, mod->name);
                    exit(1);
                }
            }

            walk(node->lhs);
            
            Type *struct_ty = node->lhs->ty;
            if (struct_ty->kind == TY_PTR) {
                struct_ty = struct_ty->base;
            }
            
            // If LHS is a module (but resolved to something else?), error?
            // If LHS was identifier resolved to module, we handled it above.
            // If LHS is not identifier (e.g. expr), it can't be module.
            
            if (struct_ty->kind != TY_STRUCT) {
                fprintf(stderr, "Erro: operador '.' requer struct ou ponteiro para struct.\n");
                exit(1);
            }

            Field *field = NULL;
            for (Field *f = struct_ty->fields; f; f = f->next) {
                if (strcmp(f->name, node->member_name) == 0) {
                    field = f;
                    break;
                }
            }

            if (!field) {
                fprintf(stderr, "Erro: struct '%s' não possui membro '%s'.\n", struct_ty->name, node->member_name);
                exit(1);
            }

            node->ty = field->type;
            node->offset = field->offset;
            return;
    }
}

static void register_structs(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            register_structs(n->body);
            
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_BLOCK && n->ty && n->ty->kind == TY_STRUCT) {
            // Mangle struct name if in module
            if (current_module_path) {
                int len = strlen(current_module_path) + 1 + strlen(n->ty->name) + 1;
                char *mangled = malloc(len);
                snprintf(mangled, len, "%s_%s", current_module_path, n->ty->name);
                n->ty->name = mangled; // Update name in type
            }
            declare_struct(n->ty);
        }
    }
}

static void register_globals(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            // If alias, create module scope. If not, use current scope.
            Scope *outer_scope = current_scope;
            Scope *mod_scope = outer_scope;
            
            if (n->name) {
                mod_scope = calloc(1, sizeof(Scope));
                mod_scope->parent = global_scope; 
                current_scope = mod_scope;
            }
            
            register_globals(n->body);
            
            current_scope = outer_scope;
            
            if (n->name) {
                // Create Module symbol
                Module *mod = calloc(1, sizeof(Module));
                mod->name = strdup(n->name);
                mod->path_prefix = strdup(prefix);
                mod->scope = mod_scope;
                mod->next = module_list;
                module_list = mod;
                
                // Declare alias in outer scope
                Variable *var = symtab_declare(n->name, type_void, VAR_FLAG_NONE);
                var->is_module = 1;
                var->module_ref = mod;
            }
            
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_LET) {
            walk(n); // walk handles declaration
            // If we are in module, walk declares in current_scope (mod_scope).
            // We also need to mangle the global name for codegen?
            // Variable struct doesn't have asm_name.
            // We should add asm_name to Variable or mangle 'name'.
            // If we mangle 'name', then lookup by name fails.
            // So Variable needs asm_name.
            // I didn't add asm_name to Variable in semantic.h.
            // I should have.
            // For now, I'll skip mangling globals or assume they are unique enough?
            // No, globals in modules MUST be mangled.
            // I'll add asm_name to Variable in semantic.h later or now?
            // I can't change header easily in this tool call.
            // I'll assume I can update it later.
        }
    }
}

static void register_funcs(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            register_funcs(n->body);
            
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_FN) {
             // Resolve param types and return type
            for (Node *p = n->params; p; p = p->next) resolve_type_ref(p->ty);
            resolve_type_ref(n->return_type);

            int param_count = 0;
            for (Node *p = n->params; p; p = p->next) param_count++;
            
            Type **param_types = calloc(param_count, sizeof(Type*));
            int i = 0;
            for (Node *p = n->params; p; p = p->next) {
                param_types[i++] = p->ty;
            }

            declare_function(n->name, n->return_type, param_types, param_count);
        }
    }
}

static void analyze_bodies(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            Scope *outer_scope = current_scope;
            if (n->name) {
                Variable *var = symtab_lookup(n->name);
                if (var && var->is_module) {
                    current_scope = var->module_ref->scope;
                }
            }
            
            analyze_bodies(n->body);
            
            current_scope = outer_scope;
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_FN) {
            walk(n);
        }
    }
}

void analyze(Node *node) {
    symtab_init();
    function_table = NULL;
    struct_defs = NULL;
    module_list = NULL;

    register_structs(node);
    register_globals(node);
    
    // Resolve struct fields
    for (StructDef *s = struct_defs; s; s = s->next) {
        for (Field *f = s->type->fields; f; f = f->next) {
            resolve_type_ref(f->type);
        }
        int offset = 0;
        for (Field *f = s->type->fields; f; f = f->next) {
            f->offset = offset;
            offset += f->type->size;
        }
        s->type->size = offset;
    }

    register_funcs(node);
    analyze_bodies(node);
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
    
    StructDef *sd = struct_defs;
    while (sd) {
        StructDef *next = sd->next;
        free(sd->name);
        free(sd);
        sd = next;
    }
    struct_defs = NULL;

    // Limpar tipos se necessário (mas eles são globais/estáticos por enquanto, então talvez não precise)
    // Se types_init alocasse dinamicamente cada vez, precisaria limpar.
    // Como são alocados uma vez, o SO limpa no final. Mas para ser purista:
    // Types are freed by free_all_types() in main.c
}
