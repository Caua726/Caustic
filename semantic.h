#pragma once
#include "parser.h"

typedef struct Variable {
    char *name;
    Type *type;
    VarFlags flags;
    int offset;
    struct Variable *next;
} Variable;

extern Type *type_int;
extern Type *type_i8;
extern Type *type_i16;
extern Type *type_i32;
extern Type *type_i64;
extern Type *type_u8;
extern Type *type_u16;
extern Type *type_u32;
extern Type *type_u64;
extern Type *type_f32;
extern Type *type_f64;
extern Type *type_bool;
extern Type *type_char;
extern Type *type_string;
extern Type *type_void;

typedef struct Function {
    char *name;
    Type *return_type;
    Type **param_types;
    int param_count;
    struct Function *next;
} Function;

void types_init();
void analyze(Node *node);
void semantic_cleanup();
