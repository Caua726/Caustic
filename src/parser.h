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
    TY_STRUCT,
} TypeKind;

typedef struct Field {
    char *name;
    struct Type *type;
    int offset;
    struct Field *next;
} Field;

typedef struct Type {
    TypeKind kind;
    int size;
    int is_signed;
    struct Type *base;
    int array_len;
    struct Type *next; // For garbage collection
    int is_static;     // If true, do not free name/fields (global primitive types)
    int owns_fields;   // If true, this type owns the field list memory
    
    // Struct fields
    char *name; // Struct name
    Field *fields;

    // Generic info
    char **generic_params;      // Template params (for uninstantiated): ["T", "U"]
    int generic_param_count;
    struct Type **generic_args;  // Concrete args (for instantiated): [type_i32, type_f64]
    int generic_arg_count;
    char *base_name;            // Original name before mangling (e.g. "Vec")
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
    NODE_KIND_SYSCALL,
    NODE_KIND_MEMBER_ACCESS,
    NODE_KIND_LOGICAL_AND,
    NODE_KIND_LOGICAL_OR,
    NODE_KIND_BREAK,
    NODE_KIND_CONTINUE,
    NODE_KIND_STRING_LITERAL,
    NODE_KIND_SHL,
    NODE_KIND_SHR,
    NODE_KIND_BITWISE_AND,
    NODE_KIND_BITWISE_OR,
    NODE_KIND_SIZEOF,
    NODE_KIND_USE,
    NODE_KIND_MODULE_ACCESS,
    NODE_KIND_FOR,
    NODE_KIND_DO_WHILE,
    NODE_KIND_BITWISE_XOR,
    NODE_KIND_BITWISE_NOT,
    NODE_KIND_LOGICAL_NOT,
    NODE_KIND_NEG,
} NodeKind;

typedef struct StringLiteral {
    int id;
    char *value;
    struct StringLiteral *next;
} StringLiteral;

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
    double fval;

    struct Node *expr;
    struct Node *init_expr;

    char *name;
    char *member_name; // For member access
    Type *return_type;
    VarFlags flags;
    // Function
    struct Node *body;
    struct Node *params; // Lista de parâmetros (NODE_KIND_VAR_DECL)
    int is_variadic;

    // Function Call
    struct Node *args;   // Lista de argumentos (expressões)

    // Generic parameters (declaration: gen T, U)
    char **generic_params;       // ["T", "U"]
    int generic_param_count;

    // Generic arguments (usage: gen i32, f64)
    Type **generic_args;         // [type_i32, type_f64]
    int generic_arg_count;

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
    struct {
        struct Node *init;
        struct Node *cond;
        struct Node *step;
        struct Node *body;
    } for_stmt;
    struct {
        struct Node *body;
        struct Node *cond;
    } do_while_stmt;
} Node;

void ast_print(Node *node);
// Memory Management
void free_ast(Node *node);
void free_ast_reset();
void cache_module(const char *path, Node *body);
Node *get_cached_module(const char *path);
void free_module_cache();
void free_source_paths();
void free_strings();
Type *new_type(TypeKind kind);
void free_all_types();
void parser_init();
Node *parse();
StringLiteral *get_strings();
