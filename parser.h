#pragma once
#include "lexer.h"

typedef enum {
    NODE_NUM,
    NODE_IDENTIFIER,
    NODE_ADD,
    NODE_SUBTRACTION,
    NODE_MULTIPLIER,
    NODE_DIVIDER,
    NODE_ASSIGN,
    NODE_EXPR_STMT,
    NODE_RETURN,
} NodeKind;

typedef struct Node {
    NodeKind kind;
    struct Node *next;
    struct Type *ty;

    Token *tok;

    struct Node *lhs;
    struct Node *rhs;

    long val;

    char ident[64];

    struct Node *expr;
} Node;

typedef enum {
    TY_INT
} TypeKind;

typedef struct Type {
    TypeKind kind;
} Type;

void ast_print(Node *node);
void parser_init();
Node *parse();
