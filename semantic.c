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
        case NODE_NUM:
            node->ty = ty_int;
            return;
        case NODE_RETURN:
            walk(node->expr);
            return;
    }
}

void analyze(Node *node) {
    ty_int = new_type(TY_INT);
    walk(node);
}
