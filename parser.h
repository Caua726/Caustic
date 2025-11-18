#pragma once
#include "lexer.h"

typedef enum {
    TY_INT,
    TY_I8,
    TY_I16,
    TY_I32,
    TY_I64,
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    TY_FLOAT,
    TY_F32,
    TY_F64,
    TY_BOOL,
    TY_CHAR,
    TY_STRING,
    TY_VOID,
    TY_PTR,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    int size;
    int is_signed;
} Type;

typedef enum {
    NODE_KIND_NUM,
    NODE_KIND_IDENTIFIER,
    NODE_KIND_ADD,
    NODE_KIND_SUBTRACTION,
    NODE_KIND_MULTIPLIER,
    NODE_KIND_DIVIDER,
    NODE_KIND_MOD,
    NODE_KIND_EQ,
    NODE_KIND_NE,
    NODE_KIND_LT,
    NODE_KIND_LE,
    NODE_KIND_GT,
    NODE_KIND_GE,
    NODE_KIND_ASSIGN,
    NODE_KIND_EXPR_STMT,
    NODE_KIND_RETURN,
    NODE_KIND_FN,
    NODE_KIND_FNCALL,
    NODE_KIND_BLOCK,
    NODE_KIND_LET,
    NODE_KIND_IF,
    NODE_KIND_ASM,
    NODE_KIND_WHILE,
    NODE_KIND_CAST,
} NodeKind;

typedef enum {
    VAR_FLAG_NONE,
    VAR_FLAG_MUT,
    VAR_FLAG_IMUT,
} VarFlags;

typedef struct Node {
    NodeKind kind;
    struct Node *next;
    struct Type *ty;
    Token *tok;

    struct Node *lhs;
    struct Node *rhs;
    long val;

    struct Node *expr;
    struct Node *init_expr;

    char *name;
    Type *return_type;
    VarFlags flags;
    struct Node *body;
    struct Node *stmts;

    int offset;
    struct Variable *var;
    struct {
        struct Node *cond;
        struct Node *then_b;
        struct Node *else_b;
    } if_stmt;
    struct {
        struct Node *cond;
        struct Node *body;
    } while_stmt;
} Node;

void ast_print(Node *node);
void free_ast(Node *node);
void parser_init();
Node *parse();
