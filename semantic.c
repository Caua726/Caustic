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

Variable *symtab_declare(char *name, Type *type, VarFlags flags) {
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

Variable *symtab_lookup(char *name) {
    for (Scope *scope = current_scope; scope; scope = scope->parent) {
        for (Variable *v = scope->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                return v;
            }
        }
    }
    return NULL;
}

void symtab_enter_scope_public() {
    symtab_enter_scope();
}

void symtab_exit_scope_public() {
    symtab_exit_scope();
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
        case NODE_KIND_IDENTIFIER:
            return;
        case NODE_KIND_RETURN:
            walk(node->expr);
            return;
        case NODE_KIND_FN:
            walk(node->body);
            return;
        case NODE_KIND_BLOCK:
            for (Node *stmt = node->stmts; stmt; stmt = stmt->next) {
                walk(stmt);
            }
            return;
        case NODE_KIND_LET:
            if (node->init_expr) {
                walk(node->init_expr);
            }
            return;
        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
            walk(node->lhs);
            walk(node->rhs);
            node->ty = node->lhs->ty;
            return;
    }
}

void analyze(Node *node) {
    ty_int = new_type(TY_INT);
    symtab_init();
    walk(node);
}
