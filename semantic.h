#pragma once
#include "parser.h"

typedef struct Variable {
    char *name;
    Type *type;
    VarFlags flags;
    int offset;
    struct Variable *next;
} Variable;

void analyze(Node *node);
Variable *symtab_declare(char *name, Type *type, VarFlags flags);
Variable *symtab_lookup(char *name);
void symtab_enter_scope_public();
void symtab_exit_scope_public();
