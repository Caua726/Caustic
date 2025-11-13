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
