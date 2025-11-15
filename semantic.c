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

Type *ty_int;

static void symtab_init() {
    current_scope = NULL;
    stack_offset = 0;
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
    stack_offset += 8;

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

Type *new_type(TypeKind kind) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    return ty;
}

static void walk(Node *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case NODE_KIND_NUM:
            node->ty = ty_int;
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
            node->ty = node->lhs->ty;
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
        case NODE_KIND_ASSIGN: {
            walk(node->lhs);
            walk(node->rhs);

            if (node->lhs->kind != NODE_KIND_IDENTIFIER) {
                fprintf(stderr, "Erro: alvo da atribuição inválido.\n");
                exit(1);
            }

            Variable *var = node->lhs->var;
            if (!(var->flags & VAR_FLAG_MUT)) {
                fprintf(stderr, "Erro: não é possível atribuir à variável imutável '%s'.\n", var->name);
                exit(1);
            }

            if (node->lhs->ty->kind != node->rhs->ty->kind) {
                fprintf(stderr, "Erro: tipos incompatíveis na atribuição.\n");
                exit(1);
            }
            node->ty = node->lhs->ty;
            return;
        }
    }
}

void analyze(Node *node) {
    ty_int = new_type(TY_INT);
    symtab_init();
    walk(node);
}
