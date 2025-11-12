#include "semantic.h"
#include "parser.h"
#include <stdlib.h>

Type *new_type(TypeKind kind) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    return ty;
}

Type *ty_int;

static void walk(Node *node) {
    if (!node) {
        return;
    }
    switch (node->kind) {
        case NODE_KIND_NUM:
            node->ty = ty_int;
            return;
        case NODE_KIND_RETURN:
            walk(node->expr);
            return;

        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
            walk(node->lhs);
            walk(node->rhs);
            if (node->lhs->ty != ty_int || node->rhs->ty != ty_int) {
                fprintf(stderr, "Erro semântico: operação aritmética requer inteiros.\n");
                exit(1);
            }
            node->ty = ty_int;
            return;
    }
}

void analyze(Node *node) {
    ty_int = new_type(TY_INT);
    walk(node);
}
