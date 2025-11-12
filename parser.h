#pragma once
#include "lexer.h"

typedef enum {
    NODE_KIND_NUM,
    NODE_KIND_IDENTIFIER,
    NODE_KIND_ADD,
    NODE_KIND_SUBTRACTION,
    NODE_KIND_MULTIPLIER,
    NODE_KIND_DIVIDER,
    NODE_KIND_ASSIGN,
    NODE_KIND_EXPR_STMT,
    NODE_KIND_RETURN,
    NODE_KIND_FN,
    NODE_KIND_BLOCK,
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

    char *name;
    struct Node *body;
    struct Node *stmts;
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

static Token current_token;
static Token lookahead_token;

static Node *parse_expr();
static Node *parse_stmt();
static Node *new_node(NodeKind kind);
static Node *new_node_num(long val);
static Node *new_node_return(Node *expr);
static void consume();
static Node *parse_expr();
static Node *parse_add();
static Node *parse_mul();
