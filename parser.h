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
    TY_ARRAY,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    int size;
    int is_signed;
    struct Type *base;
    int array_len;
    struct Type *next; // For garbage collection
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
    NODE_KIND_ADDR,
    NODE_KIND_DEREF,
    NODE_KIND_INDEX,
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
    // Function
    struct Node *body;
    struct Node *params; // Lista de parâmetros (NODE_KIND_VAR_DECL)

    // Function Call
    struct Node *args;   // Lista de argumentos (expressões)

    struct Node *stmts;

    int offset;
    struct Variable *var;
    // If/While
    struct {
        struct Node *cond;
        struct Node *then_b;
        struct Node *else_b;
        struct Node *body;
    } if_stmt, while_stmt;
} Node;

void ast_print(Node *node);
void free_ast(Node *node);
Type *new_type(TypeKind kind);
void free_all_types();
void parser_init();
Node *parse();
