
#pragma once
#include "parser.h"



typedef struct Module Module;

typedef struct Variable {
    char *name;
    Type *type;
    VarFlags flags;
    int offset;
    int is_global;
    int is_module;
    Module *module_ref;
    char *asm_name;
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
    char *asm_name;
    char *module_prefix;
    int is_variadic;
    struct Function *next;
} Function;

typedef struct GenericTemplate {
    char *name;              // "max", "Vec"
    char **params;           // ["T", "U"]
    int param_count;
    Node *ast;               // AST original (for functions)
    Type *struct_type;       // For generic structs (Type with template fields)
    int is_function;         // 1=func, 0=struct
    char *module_prefix;     // Module context
    struct GenericTemplate *next;
} GenericTemplate;

typedef struct GenericInstance {
    char *mangled_name;
    struct GenericInstance *next;
} GenericInstance;

void types_init();
void analyze(Node *node);
void semantic_cleanup();
