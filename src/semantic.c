#include "semantic.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Scope {
    Variable *vars;
    struct Scope *parent;
    int is_global_scope;
    struct Scope *next_allocated; // For cleanup
} Scope;

static Scope *all_scopes = NULL; // Global list of all allocated scopes
static Scope *current_scope = NULL;
static Scope *global_scope = NULL;
static int stack_offset = 0;
static Function *function_table = NULL;
static int loop_depth = 0;
static Type *current_return_type = NULL;

struct Module {
    char *name; // alias
    Scope *scope;
    char *path_prefix;
    struct Module *next;
};

static char *current_module_path = NULL; // Prefix for current module
static Module *module_list = NULL;

// Forward declarations for generics support
static const char *type_name(Type *ty);
static Variable *symtab_lookup(char *name);
static void resolve_type_ref(Type *ty);
static void declare_struct(char *name, Type *type);
static void declare_function(char *name, Type *return_type, Type **param_types, int param_count, int is_variadic);
void walk(Node *node);

// --- Generics support ---
static GenericTemplate *generic_templates = NULL;
static GenericInstance *generic_instances = NULL;
static Node *ast_tail = NULL; // Tail of top-level AST for appending instantiated functions

static void store_generic_template(char *name, char **params, int count, Node *ast, Type *struct_type, int is_function) {
    GenericTemplate *t = calloc(1, sizeof(GenericTemplate));
    t->name = strdup(name);
    t->params = calloc(count, sizeof(char*));
    for (int i = 0; i < count; i++) t->params[i] = strdup(params[i]);
    t->param_count = count;
    t->ast = ast;
    t->struct_type = struct_type;
    t->is_function = is_function;
    t->module_prefix = current_module_path ? strdup(current_module_path) : NULL;
    t->next = generic_templates;
    generic_templates = t;
}

static GenericTemplate *lookup_generic_template(char *name) {
    for (GenericTemplate *t = generic_templates; t; t = t->next) {
        if (strcmp(t->name, name) == 0) return t;
    }
    return NULL;
}

static int is_already_instantiated(char *mangled) {
    for (GenericInstance *i = generic_instances; i; i = i->next) {
        if (strcmp(i->mangled_name, mangled) == 0) return 1;
    }
    return 0;
}

static void mark_instantiated(char *mangled) {
    GenericInstance *i = calloc(1, sizeof(GenericInstance));
    i->mangled_name = strdup(mangled);
    i->next = generic_instances;
    generic_instances = i;
}

static char *mangle_generic_name(const char *base, Type **concrete, int count) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", base);
    for (int i = 0; i < count; i++) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "_%s", type_name(concrete[i]));
    }
    return strdup(buf);
}

// Deep clone a Type
static Type *clone_type(Type *ty) {
    if (!ty) return NULL;
    // Don't clone primitive singletons — preserve pointer equality
    if (ty->is_static) return ty;
    Type *clone = new_type(ty->kind);
    clone->size = ty->size;
    clone->is_signed = ty->is_signed;
    clone->array_len = ty->array_len;
    clone->is_static = 0;
    clone->owns_fields = 0; // Cloned types borrow fields initially

    if (ty->name) clone->name = strdup(ty->name);
    if (ty->base_name) clone->base_name = strdup(ty->base_name);
    clone->base = clone_type(ty->base);

    // Clone fields for structs
    if (ty->fields) {
        clone->owns_fields = 1;
        Field *head = NULL, *tail = NULL;
        for (Field *f = ty->fields; f; f = f->next) {
            Field *cf = calloc(1, sizeof(Field));
            cf->name = strdup(f->name);
            cf->type = clone_type(f->type);
            cf->offset = f->offset;
            if (!head) { head = cf; tail = cf; }
            else { tail->next = cf; tail = cf; }
        }
        clone->fields = head;
    }

    // Clone generic info
    if (ty->generic_params) {
        clone->generic_param_count = ty->generic_param_count;
        clone->generic_params = calloc(ty->generic_param_count, sizeof(char*));
        for (int i = 0; i < ty->generic_param_count; i++)
            clone->generic_params[i] = strdup(ty->generic_params[i]);
    }
    if (ty->generic_args) {
        clone->generic_arg_count = ty->generic_arg_count;
        clone->generic_args = calloc(ty->generic_arg_count, sizeof(Type*));
        for (int i = 0; i < ty->generic_arg_count; i++)
            clone->generic_args[i] = ty->generic_args[i]; // Share concrete types
    }

    return clone;
}

// Forward declaration
static Node *clone_node(Node *node);

static Node *clone_node_list(Node *node) {
    if (!node) return NULL;
    Node *head = clone_node(node);
    Node *cur = head;
    for (Node *n = node->next; n; n = n->next) {
        cur->next = clone_node(n);
        cur = cur->next;
    }
    return head;
}

static Node *clone_node(Node *node) {
    if (!node) return NULL;
    Node *c = calloc(1, sizeof(Node));
    *c = *node; // Shallow copy
    c->next = NULL;

    // Deep copy allocated strings
    if (node->name) c->name = strdup(node->name);
    if (node->member_name) c->member_name = strdup(node->member_name);

    // Deep copy type
    if (node->ty && !node->ty->is_static) {
        c->ty = clone_type(node->ty);
    }
    if (node->return_type && !node->return_type->is_static) {
        c->return_type = clone_type(node->return_type);
    }

    // Clear semantic-phase pointers (will be re-resolved)
    c->var = NULL;
    c->offset = 0;
    c->tok = node->tok; // Keep token for error reporting

    // Deep copy generic params
    if (node->generic_params) {
        c->generic_params = calloc(node->generic_param_count, sizeof(char*));
        c->generic_param_count = node->generic_param_count;
        for (int i = 0; i < node->generic_param_count; i++)
            c->generic_params[i] = strdup(node->generic_params[i]);
    }
    if (node->generic_args) {
        c->generic_args = calloc(node->generic_arg_count, sizeof(Type*));
        c->generic_arg_count = node->generic_arg_count;
        for (int i = 0; i < node->generic_arg_count; i++)
            c->generic_args[i] = node->generic_args[i];
    }

    // Deep copy children based on node kind
    switch (node->kind) {
        case NODE_KIND_FN:
            c->params = clone_node_list(node->params);
            c->body = clone_node(node->body);
            break;
        case NODE_KIND_BLOCK:
            c->stmts = clone_node_list(node->stmts);
            break;
        case NODE_KIND_RETURN:
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_CAST:
        case NODE_KIND_ADDR:
        case NODE_KIND_DEREF:
        case NODE_KIND_NEG:
        case NODE_KIND_BITWISE_NOT:
        case NODE_KIND_LOGICAL_NOT:
            c->expr = clone_node(node->expr);
            break;
        case NODE_KIND_LET:
            c->init_expr = clone_node(node->init_expr);
            break;
        case NODE_KIND_FNCALL:
        case NODE_KIND_SYSCALL:
            c->lhs = clone_node(node->lhs);
            c->args = clone_node_list(node->args);
            break;
        case NODE_KIND_IF:
            c->if_stmt.cond = clone_node(node->if_stmt.cond);
            c->if_stmt.then_b = clone_node(node->if_stmt.then_b);
            c->if_stmt.else_b = clone_node(node->if_stmt.else_b);
            break;
        case NODE_KIND_WHILE:
            c->while_stmt.cond = clone_node(node->while_stmt.cond);
            c->while_stmt.body = clone_node(node->while_stmt.body);
            break;
        case NODE_KIND_DO_WHILE:
            c->do_while_stmt.cond = clone_node(node->do_while_stmt.cond);
            c->do_while_stmt.body = clone_node(node->do_while_stmt.body);
            break;
        case NODE_KIND_FOR:
            c->for_stmt.init = clone_node(node->for_stmt.init);
            c->for_stmt.cond = clone_node(node->for_stmt.cond);
            c->for_stmt.step = clone_node(node->for_stmt.step);
            c->for_stmt.body = clone_node(node->for_stmt.body);
            break;
        case NODE_KIND_ASSIGN:
        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
        case NODE_KIND_MOD:
        case NODE_KIND_EQ:
        case NODE_KIND_NE:
        case NODE_KIND_LT:
        case NODE_KIND_LE:
        case NODE_KIND_GT:
        case NODE_KIND_GE:
        case NODE_KIND_INDEX:
        case NODE_KIND_LOGICAL_AND:
        case NODE_KIND_LOGICAL_OR:
        case NODE_KIND_SHL:
        case NODE_KIND_SHR:
        case NODE_KIND_BITWISE_AND:
        case NODE_KIND_BITWISE_OR:
        case NODE_KIND_BITWISE_XOR:
        case NODE_KIND_MEMBER_ACCESS:
        case NODE_KIND_MODULE_ACCESS:
            c->lhs = clone_node(node->lhs);
            c->rhs = clone_node(node->rhs);
            break;
        case NODE_KIND_USE:
            c->body = node->body; // Share module body
            break;
        default:
            // NUM, IDENTIFIER, STRING_LITERAL, ASM, BREAK, CONTINUE, SIZEOF — no deep children
            break;
    }

    return c;
}

// Check if a type matches a generic param; if so, return the concrete type pointer.
// Returns NULL if no match.
static Type *find_generic_substitution(Type *ty, char **param_names, Type **concrete, int count) {
    if (ty->kind == TY_STRUCT && ty->name && !ty->fields) {
        for (int i = 0; i < count; i++) {
            if (strcmp(ty->name, param_names[i]) == 0) {
                return concrete[i];
            }
        }
    }
    return NULL;
}

// Substitute generic type params with concrete types in a Type.
// For pointer/array base types that are generic params, we replace the base pointer.
static void substitute_type(Type *ty, char **param_names, Type **concrete, int count) {
    if (!ty) return;

    // Check if this type IS a generic param (TY_STRUCT with name matching a param)
    if (ty->kind == TY_STRUCT && ty->name && !ty->fields) {
        for (int i = 0; i < count; i++) {
            if (strcmp(ty->name, param_names[i]) == 0) {
                // Replace with concrete type
                free(ty->name);
                ty->kind = concrete[i]->kind;
                ty->size = concrete[i]->size;
                ty->is_signed = concrete[i]->is_signed;
                ty->is_static = concrete[i]->is_static;
                ty->name = concrete[i]->name ? strdup(concrete[i]->name) : NULL;
                ty->base = concrete[i]->base;
                ty->fields = concrete[i]->fields;
                ty->owns_fields = 0;
                ty->array_len = concrete[i]->array_len;
                return;
            }
        }
    }

    // Recurse into pointer base
    if (ty->base) substitute_type(ty->base, param_names, concrete, count);

    // Recurse into struct fields — replace field type pointers directly
    if (ty->owns_fields) {
        for (Field *f = ty->fields; f; f = f->next) {
            Type *sub = find_generic_substitution(f->type, param_names, concrete, count);
            if (sub) {
                f->type = sub; // Direct pointer replacement
            } else {
                substitute_type(f->type, param_names, concrete, count);
            }
        }
    }
}

// Substitute types in the entire AST tree
static void substitute_types_in_node(Node *node, char **param_names, Type **concrete, int count) {
    if (!node) return;

    // Substitute in this node's type
    if (node->ty) {
        Type *sub = find_generic_substitution(node->ty, param_names, concrete, count);
        if (sub) {
            node->ty = sub; // Point directly to concrete type (e.g. type_i32 global)
        } else if (!node->ty->is_static) {
            substitute_type(node->ty, param_names, concrete, count);
        }
    }
    if (node->return_type) {
        Type *sub = find_generic_substitution(node->return_type, param_names, concrete, count);
        if (sub) {
            node->return_type = sub;
        } else if (!node->return_type->is_static) {
            substitute_type(node->return_type, param_names, concrete, count);
        }
    }

    // Recurse into children
    switch (node->kind) {
        case NODE_KIND_FN:
            for (Node *p = node->params; p; p = p->next)
                substitute_types_in_node(p, param_names, concrete, count);
            substitute_types_in_node(node->body, param_names, concrete, count);
            break;
        case NODE_KIND_BLOCK:
            for (Node *s = node->stmts; s; s = s->next)
                substitute_types_in_node(s, param_names, concrete, count);
            break;
        case NODE_KIND_RETURN:
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_CAST:
        case NODE_KIND_ADDR:
        case NODE_KIND_DEREF:
        case NODE_KIND_NEG:
        case NODE_KIND_BITWISE_NOT:
        case NODE_KIND_LOGICAL_NOT:
            substitute_types_in_node(node->expr, param_names, concrete, count);
            break;
        case NODE_KIND_LET:
            substitute_types_in_node(node->init_expr, param_names, concrete, count);
            break;
        case NODE_KIND_FNCALL:
        case NODE_KIND_SYSCALL:
            // Substitute generic type args on the call itself (for nested generic calls)
            if (node->generic_arg_count > 0 && node->member_name) {
                for (int gi = 0; gi < node->generic_arg_count; gi++) {
                    Type *sub = find_generic_substitution(node->generic_args[gi], param_names, concrete, count);
                    if (sub) node->generic_args[gi] = sub;
                }
                // Rebuild mangled name from original name + substituted generic args
                char *new_mangled = mangle_generic_name(node->member_name, node->generic_args, node->generic_arg_count);
                if (node->lhs && node->lhs->name) {
                    free(node->lhs->name);
                    node->lhs->name = new_mangled;
                } else {
                    free(new_mangled);
                }
            }
            substitute_types_in_node(node->lhs, param_names, concrete, count);
            for (Node *a = node->args; a; a = a->next)
                substitute_types_in_node(a, param_names, concrete, count);
            break;
        case NODE_KIND_IF:
            substitute_types_in_node(node->if_stmt.cond, param_names, concrete, count);
            substitute_types_in_node(node->if_stmt.then_b, param_names, concrete, count);
            substitute_types_in_node(node->if_stmt.else_b, param_names, concrete, count);
            break;
        case NODE_KIND_WHILE:
            substitute_types_in_node(node->while_stmt.cond, param_names, concrete, count);
            substitute_types_in_node(node->while_stmt.body, param_names, concrete, count);
            break;
        case NODE_KIND_DO_WHILE:
            substitute_types_in_node(node->do_while_stmt.cond, param_names, concrete, count);
            substitute_types_in_node(node->do_while_stmt.body, param_names, concrete, count);
            break;
        case NODE_KIND_FOR:
            substitute_types_in_node(node->for_stmt.init, param_names, concrete, count);
            substitute_types_in_node(node->for_stmt.cond, param_names, concrete, count);
            substitute_types_in_node(node->for_stmt.step, param_names, concrete, count);
            substitute_types_in_node(node->for_stmt.body, param_names, concrete, count);
            break;
        case NODE_KIND_ASSIGN:
        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
        case NODE_KIND_MOD:
        case NODE_KIND_EQ:
        case NODE_KIND_NE:
        case NODE_KIND_LT:
        case NODE_KIND_LE:
        case NODE_KIND_GT:
        case NODE_KIND_GE:
        case NODE_KIND_INDEX:
        case NODE_KIND_LOGICAL_AND:
        case NODE_KIND_LOGICAL_OR:
        case NODE_KIND_SHL:
        case NODE_KIND_SHR:
        case NODE_KIND_BITWISE_AND:
        case NODE_KIND_BITWISE_OR:
        case NODE_KIND_BITWISE_XOR:
        case NODE_KIND_MEMBER_ACCESS:
        case NODE_KIND_MODULE_ACCESS:
            substitute_types_in_node(node->lhs, param_names, concrete, count);
            substitute_types_in_node(node->rhs, param_names, concrete, count);
            break;
        default:
            break;
    }
}

static void instantiate_generic_func(GenericTemplate *tmpl, Type **concrete, int count, char *mangled_name);
static void instantiate_generic_struct(GenericTemplate *tmpl, Type **concrete, int count, char *mangled_name);

static char *get_module_prefix(const char *path) {
    char *prefix = malloc(strlen(path) + 2);
    prefix[0] = '_';
    int j = 1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/' || path[i] == '.' || path[i] == '\\' || path[i] == '-') {
            prefix[j++] = '_';
        } else {
            prefix[j++] = path[i];
        }
    }
    prefix[j] = '\0';
    return prefix;
}

Type *type_int;
Type *type_i8;
Type *type_i16;
Type *type_i32;
Type *type_i64;
Type *type_u8;
Type *type_u16;
Type *type_u32;
Type *type_u64;
Type *type_f32;
Type *type_f64;
Type *type_bool;
Type *type_char;
Type *type_string;
Type *type_void;

static const char *type_name(Type *ty) {
    if (!ty) return "unknown";
    switch (ty->kind) {
        case TY_INT: return "int";
        case TY_I8: return "i8";
        case TY_I16: return "i16";
        case TY_I32: return "i32";
        case TY_I64: return "i64";
        case TY_U8: return "u8";
        case TY_U16: return "u16";
        case TY_U32: return "u32";
        case TY_U64: return "u64";
        case TY_F32: return "f32";
        case TY_F64: return "f64";
        case TY_BOOL: return "bool";
        case TY_CHAR: return "char";
        case TY_STRING: return "string";
        case TY_VOID: return "void";
        case TY_PTR: return "*ptr";
        case TY_ARRAY: return "array";
        case TY_STRUCT: return ty->name ? ty->name : "struct";
        default: return "unknown";
    }
}

static Type *new_type_full(TypeKind kind, int size, int is_signed) {
    Type *ty = new_type(kind);
    ty->size = size;
    ty->is_signed = is_signed;
    return ty;
}

static void declare_function(char *name, Type *return_type, Type **param_types, int param_count, int is_variadic) {
    for (Function *fs = function_table; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) {
            // Check if in same module
            int same_module = 0;
            if (current_module_path && fs->module_prefix && strcmp(current_module_path, fs->module_prefix) == 0) same_module = 1;
            if (!current_module_path && !fs->module_prefix) same_module = 1;

            if (same_module) {
                if (param_types) free(param_types); // Free unused param_types
                return;
            }
        }
    }

    Function *f = calloc(1, sizeof(Function));
    f->name = strdup(name);
    f->return_type = return_type;
    f->param_types = param_types;
    f->param_count = param_count;
    f->is_variadic = is_variadic;
    f->module_prefix = current_module_path ? strdup(current_module_path) : NULL;

    if (current_module_path) {
        int len = strlen(current_module_path) + 1 + strlen(name) + 1;
        f->asm_name = malloc(len);
        snprintf(f->asm_name, len, "%s_%s", current_module_path, name);
    } else {
        f->asm_name = strdup(name);
    }
    // printf("Declared Function: %s -> ASM: %s (Module: %s)\n", name, f->asm_name, current_module_path ? current_module_path : "NONE");
    
    f->next = function_table;
    function_table = f;
}

static Function *lookup_function_qualified(char *name, char *prefix) {
    for (Function *f = function_table; f; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            if (prefix == NULL && f->module_prefix == NULL) return f;
            if (prefix != NULL && f->module_prefix != NULL && strcmp(prefix, f->module_prefix) == 0) return f;
        }
    }
    return NULL;
}

static Function *lookup_function(char *name) {
    // Try current module first
    if (current_module_path) {
        Function *f = lookup_function_qualified(name, current_module_path);
        if (f) return f;
    }
    // Try global
    return lookup_function_qualified(name, NULL);
}

// Global list of registered structs (using the Type structs themselves)




typedef struct StructDef {
    char *name;
    Type *type;
    struct StructDef *next;
} StructDef;

static StructDef *struct_defs = NULL;

static Type *lookup_struct(char *name) {
    for (StructDef *s = struct_defs; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s->type;
        }
    }
    return NULL;
}

static void resolve_type_ref(Type *ty) {
    if (ty->kind == TY_STRUCT && ty->fields == NULL && ty->name != NULL) {
        Type *def = lookup_struct(ty->name);
        
        // 1. Try mangled name (Internal reference)
        if (!def && current_module_path) {
             int len = strlen(current_module_path) + 1 + strlen(ty->name) + 1;
             char *mangled = malloc(len);
             snprintf(mangled, len, "%s_%s", current_module_path, ty->name);
             def = lookup_struct(mangled);
             if (!def) {
             }
             free(mangled);
        }
        
        // 2. Try resolving alias (External reference: alias.Type)
        if (!def && strchr(ty->name, '.')) {
            char *dot = strchr(ty->name, '.');
            int alias_len = dot - ty->name;
            char *alias = malloc(alias_len + 1);
            strncpy(alias, ty->name, alias_len);
            alias[alias_len] = '\0';
            
            char *type_name = dot + 1;
            
            Variable *var = symtab_lookup(alias);
            if (var) {
                if (var->is_module && var->module_ref) {
                    char *prefix = var->module_ref->path_prefix;
                    int len = strlen(prefix) + 1 + strlen(type_name) + 1;
                    char *mangled = malloc(len);
                    snprintf(mangled, len, "%s_%s", prefix, type_name);
                    def = lookup_struct(mangled);
                    free(mangled);
                }
            }
            free(alias);
        }

        // 3. Try generic instantiation (e.g. Vec_i32 from template Vec gen T)
        if (!def && ty->generic_arg_count > 0 && ty->base_name) {
            GenericTemplate *tmpl = lookup_generic_template(ty->base_name);
            if (tmpl && !tmpl->is_function) {
                instantiate_generic_struct(tmpl, ty->generic_args, ty->generic_arg_count, ty->name);
                def = lookup_struct(ty->name);
            }
        }

        if (!def) {
            fprintf(stderr, "Erro: struct '%s' não definida.\n", ty->name);
            // Print all registered structs for debugging
            fprintf(stderr, "Structs registradas:\n");
            for (StructDef *s = struct_defs; s; s = s->next) {
                fprintf(stderr, "  - %s\n", s->name);
            }
            exit(1);
        }
        // Copy details
        ty->fields = def->fields;
        ty->owns_fields = 0; // We are borrowing fields from def, do not free them
        ty->size = def->size;
        // ty->name is already set
    }
    if (ty->kind == TY_PTR || ty->kind == TY_ARRAY) {
        resolve_type_ref(ty->base);
    }
}

static void declare_struct(char *name, Type *type) {
    for (StructDef *s = struct_defs; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return;
        }
    }
    StructDef *s = calloc(1, sizeof(StructDef));
    s->name = strdup(name);
    s->type = type;
    s->next = struct_defs;
    struct_defs = s;

    // Calculate size and offsets
    int offset = 0;
    for (Field *f = type->fields; f; f = f->next) {
        resolve_type_ref(f->type);
        
        if (f->type->kind == TY_STRUCT && strcmp(f->type->name, type->name) == 0) {
             fprintf(stderr, "Erro: struct '%s' contém a si mesma diretamente no campo '%s'. Use um ponteiro.\n", type->name, f->name);
             exit(1);
        }

        f->offset = offset;
        offset += f->type->size;
    }
    type->size = offset;
}

void types_init() {
    type_i8 = new_type_full(TY_I8, 1, 1); type_i8->is_static = 1;
    type_i16 = new_type_full(TY_I16, 2, 1); type_i16->is_static = 1;
    type_i32 = new_type_full(TY_I32, 4, 1); type_i32->is_static = 1;
    type_i64 = new_type_full(TY_I64, 8, 1); type_i64->is_static = 1;
    type_u8 = new_type_full(TY_U8, 1, 0); type_u8->is_static = 1;
    type_u16 = new_type_full(TY_U16, 2, 0); type_u16->is_static = 1;
    type_u32 = new_type_full(TY_U32, 4, 0); type_u32->is_static = 1;
    type_u64 = new_type_full(TY_U64, 8, 0); type_u64->is_static = 1;
    type_f32 = new_type_full(TY_F32, 4, 1); type_f32->is_static = 1;
    type_f64 = new_type_full(TY_F64, 8, 1); type_f64->is_static = 1;
    type_bool = new_type_full(TY_BOOL, 1, 0); type_bool->is_static = 1;
    type_char = new_type_full(TY_CHAR, 1, 1); type_char->is_static = 1;
    type_string = new_type_full(TY_STRING, 8, 0); type_string->is_static = 1;
    type_void = new_type_full(TY_VOID, 0, 0); type_void->is_static = 1;
    type_int = type_i32;
}

static int is_safe_implicit_conversion(Type *from, Type *to) {
    if (from == to) return 1;

    if (from->size > to->size) return 0;

    if (from->is_signed != to->is_signed) return 0;

    return 1;
}

static void symtab_init() {
    current_scope = NULL;
    global_scope = calloc(1, sizeof(Scope));
    global_scope->next_allocated = all_scopes; all_scopes = global_scope;
    global_scope->is_global_scope = 1;
    current_scope = global_scope;
    stack_offset = 48; // Reserve 48 bytes for saved registers (rbx, r12-r15) + alignment
}

static void symtab_enter_scope() {
    Scope *new_scope = calloc(1, sizeof(Scope));
    new_scope->next_allocated = all_scopes; all_scopes = new_scope;
    new_scope->parent = current_scope;
    current_scope = new_scope;
    current_scope = new_scope;
}

static void symtab_exit_scope() {
    if (!current_scope) return;


    current_scope = current_scope->parent;
}

static Variable *symtab_declare(char *name, Type *type, VarFlags flags) {
    if (!current_scope) {
        fprintf(stderr, "Erro interno: sem escopo ativo\n");
        exit(1);
    }

    for (Variable *v = current_scope->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
        if (strcmp(v->name, name) == 0) {
            return v;
        }
        }
    }

    // Check for shadowing
    for (Scope *s = current_scope->parent; s; s = s->parent) {
        for (Variable *v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                break;
            }
        }
    }

    Variable *var = calloc(1, sizeof(Variable));
    var->name = strdup(name);
    var->type = type;
    var->flags = flags;

    if (current_scope->is_global_scope) {
        var->is_global = 1;
        var->offset = 0; // Globals don't have stack offset
        
        if (current_module_path) {
            int len = strlen(current_module_path) + 1 + strlen(name) + 1;
            char *mangled = malloc(len);
            snprintf(mangled, len, "%s_%s", current_module_path, name);
            var->asm_name = mangled;
        } else {
            var->asm_name = strdup(name);
        }
    } else {
        var->is_global = 0;
        stack_offset += type->size;
        var->offset = stack_offset;
        var->asm_name = NULL;
    }

    var->next = current_scope->vars;
    current_scope->vars = var;
    
    current_scope->vars = var;
    
    return var;
}

static Variable *symtab_lookup(char *name) {
    for (Scope *scope = current_scope; scope; scope = scope->parent) {
        for (Variable *v = scope->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                return v;
            }
        }
    }
    return NULL;
}



static void instantiate_generic_func(GenericTemplate *tmpl, Type **concrete, int count, char *mangled_name) {
    if (is_already_instantiated(mangled_name)) return;
    mark_instantiated(mangled_name);

    // Clone the template AST
    Node *fn_clone = clone_node(tmpl->ast);

    // Substitute type params
    substitute_types_in_node(fn_clone, tmpl->params, concrete, count);

    // Rename function
    free(fn_clone->name);
    fn_clone->name = strdup(mangled_name);

    // Clear generic params so it's treated as a normal function
    if (fn_clone->generic_params) {
        for (int i = 0; i < fn_clone->generic_param_count; i++)
            free(fn_clone->generic_params[i]);
        free(fn_clone->generic_params);
        fn_clone->generic_params = NULL;
        fn_clone->generic_param_count = 0;
    }

    // Register function in the function table
    int param_count = 0;
    for (Node *p = fn_clone->params; p; p = p->next) {
        resolve_type_ref(p->ty);
        param_count++;
    }
    resolve_type_ref(fn_clone->return_type);

    Type **param_types = calloc(param_count, sizeof(Type*));
    int i = 0;
    for (Node *p = fn_clone->params; p; p = p->next) {
        param_types[i++] = p->ty;
    }
    declare_function(mangled_name, fn_clone->return_type, param_types, param_count, fn_clone->is_variadic);

    // Append to AST for IR generation
    if (ast_tail) {
        ast_tail->next = fn_clone;
        ast_tail = fn_clone;
    }

    // Walk the function body for type checking
    // Save and restore state since walk(NODE_KIND_FN) modifies global stack_offset and scope
    int saved_stack_offset = stack_offset;
    Scope *saved_scope = current_scope;
    walk(fn_clone);
    stack_offset = saved_stack_offset;
    current_scope = saved_scope;
}

static void instantiate_generic_struct(GenericTemplate *tmpl, Type **concrete, int count, char *mangled_name) {
    if (is_already_instantiated(mangled_name)) return;
    mark_instantiated(mangled_name);

    // Clone the struct type
    Type *new_struct = clone_type(tmpl->struct_type);
    new_struct->owns_fields = 1;

    // Substitute type params in fields — use direct pointer replacement
    for (Field *f = new_struct->fields; f; f = f->next) {
        Type *sub = find_generic_substitution(f->type, tmpl->params, concrete, count);
        if (sub) {
            f->type = sub;
        } else {
            substitute_type(f->type, tmpl->params, concrete, count);
        }
    }

    // Handle nested generic struct fields (e.g. Inner gen T inside Outer gen T)
    for (Field *f = new_struct->fields; f; f = f->next) {
        if (f->type->generic_arg_count > 0 && f->type->base_name) {
            // Substitute each generic_arg (e.g. T -> i32)
            for (int gi = 0; gi < f->type->generic_arg_count; gi++) {
                Type *sub = find_generic_substitution(f->type->generic_args[gi], tmpl->params, concrete, count);
                if (sub) f->type->generic_args[gi] = sub;
            }
            // Rebuild mangled name (e.g. Inner_i32)
            char *new_name = mangle_generic_name(f->type->base_name, f->type->generic_args, f->type->generic_arg_count);
            free(f->type->name);
            f->type->name = strdup(new_name);

            // Instantiate the inner generic struct if not already done
            GenericTemplate *inner_tmpl = lookup_generic_template(f->type->base_name);
            if (inner_tmpl && !inner_tmpl->is_function) {
                instantiate_generic_struct(inner_tmpl, f->type->generic_args, f->type->generic_arg_count, new_name);
            }
            free(new_name);
        }
    }

    // Rename
    if (new_struct->name) free(new_struct->name);
    new_struct->name = strdup(mangled_name);

    // Clear generic params
    if (new_struct->generic_params) {
        for (int i = 0; i < new_struct->generic_param_count; i++)
            free(new_struct->generic_params[i]);
        free(new_struct->generic_params);
        new_struct->generic_params = NULL;
        new_struct->generic_param_count = 0;
    }

    // Register and calculate sizes
    declare_struct(mangled_name, new_struct);
}

void walk(Node *node) {
    if (!node) return;
    switch (node->kind) {
        case NODE_KIND_ASM: break;
        case NODE_KIND_NUM:
            if (!node->ty) node->ty = type_i64;
            return;

        case NODE_KIND_STRING_LITERAL: {
            Type *ptr_ty = new_type(TY_PTR);
            ptr_ty->base = type_u8;
            ptr_ty->size = 8;
            node->ty = ptr_ty;
            return;
        }

        case NODE_KIND_IDENTIFIER: {
            if (node->var) {
                 node->ty = node->var->type;
                 node->offset = node->var->offset;
                 return;
            }
            Variable *var = symtab_lookup(node->name);
            if (!var) {
                fprintf(stderr, "Erro: variavel '%s' nao declarada\n", node->name);
                exit(1);
            }
            node->var = var;
            node->ty = var->type;
            node->offset = var->offset;
            return;
        }

        case NODE_KIND_RETURN:
            if (node->expr) {
                walk(node->expr);
                // Allow implicit conversion for integer literals that fit in target type
                int allow_literal_narrowing = 0;
                if (node->expr->kind == NODE_KIND_NUM) {
                    long val = node->expr->val;
                    if (current_return_type == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                    else if (current_return_type == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                    else if (current_return_type == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                    else if (current_return_type == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                    else if (current_return_type == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                    else if (current_return_type == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
                    if (allow_literal_narrowing) {
                        node->expr->ty = current_return_type;
                    }
                }
                if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->expr->ty, current_return_type)) {
                    fprintf(stderr, "Erro em %s:%d: tipo de retorno incompatível. Esperado '%s', encontrado '%s'\n",
                            node->tok ? node->tok->filename : "?",
                            node->tok ? node->tok->line : 0,
                            type_name(current_return_type), type_name(node->expr->ty));
                    exit(1);
                }
            } else {
                if (current_return_type != type_void) {
                    fprintf(stderr, "Erro: retorno vazio em função não-void.\n");
                    exit(1);
                }
            }
            return;

        case NODE_KIND_FN:
            // Function scope is a child of global scope (or current scope if nested functions were supported)
            // But we need to reset stack_offset for each function.
            // And we need to ensure we are entering a new scope from global scope.
            // Since we are iterating top-level nodes in analyze(), we might be in global scope.
            // Let's ensure we push a new scope.
            symtab_enter_scope();
            stack_offset = 48; // Reset stack offset for new function
            // Declarar parâmetros no escopo da função
            for (Node *param = node->params; param; param = param->next) {
                Variable *var = symtab_declare(param->name, param->ty, param->flags);
                param->var = var;
                param->offset = var->offset;
            }
            // Update node->name to asm_name for codegen
            // We need to find the function in the table to get asm_name
            // But we are in a new scope? No, function_table is global.
            // But we need to know the prefix?
            // lookup_function uses current_module_path.
            // analyze_bodies sets current_module_path.
            // So lookup_function(node->name) should work.
            Function *fs_def = lookup_function(node->name);
            if (fs_def && fs_def->asm_name) {
                 // Only update if different (to avoid leak if we run multiple times?)
                 if (strcmp(node->name, fs_def->asm_name) != 0) {
                     node->name = strdup(fs_def->asm_name);
                 }
            }

            Type *saved_return_type = current_return_type;
            current_return_type = node->return_type; // Assuming node->return_type is set by parser/declare_functions
            // Wait, node->return_type is in the AST node.
            // We need to make sure it's resolved?
            // Parser sets it to type_i32 or type_void etc.
            // If it's a struct type, it might need resolution?
            // For now assume primitive or already resolved.
            
            walk(node->body);
            
            current_return_type = saved_return_type;
            symtab_exit_scope();
            return;

        case NODE_KIND_EXPR_STMT:
            walk(node->expr);
            return;

        case NODE_KIND_BLOCK:
            symtab_enter_scope();
            for (Node *stmt = node->stmts; stmt; stmt = stmt->next) {
                walk(stmt);
            }
            symtab_exit_scope();
            return;

        case NODE_KIND_LET: {
            resolve_type_ref(node->ty);
            if (node->ty->kind == TY_VOID) {
                fprintf(stderr, "Erro na linha %d: Variável '%s' não pode ser do tipo void.\n", node->tok ? node->tok->line : 0, node->name);
                exit(1);
            }
            if (node->init_expr) {
                walk(node->init_expr);

                int allow_literal_narrowing = 0;
                if (node->init_expr->kind == NODE_KIND_NUM) {
                    long val = node->init_expr->val;
                    if (node->ty == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                    else if (node->ty == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                    else if (node->ty == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                    else if (node->ty == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                    else if (node->ty == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                    else if (node->ty == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
                }

                if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->init_expr->ty, node->ty)) {
                    fprintf(stderr, "Erro: conversão implícita não segura ao inicializar variável. Use cast explícito.\n");
                    exit(1);
                }
            }
            Variable *var = symtab_declare(node->name, node->ty, node->flags);
            node->var = var;
            node->offset = var->offset;
            // Wait, symtab_declare uses type->size. If type was not resolved, size might be 0.
            // We must resolve BEFORE declaring.
            // Let's fix this.
            
            // Undo symtab_declare side effects? No, let's resolve before.
            // But we already called symtab_declare.
            // Let's change the order.
            return;
        }

        case NODE_KIND_ADD:
        case NODE_KIND_SUBTRACTION:
        case NODE_KIND_MULTIPLIER:
        case NODE_KIND_DIVIDER:
        case NODE_KIND_MOD:
        case NODE_KIND_EQ:
        case NODE_KIND_NE:
        case NODE_KIND_LT:
        case NODE_KIND_LE:
        case NODE_KIND_GT:
        case NODE_KIND_GE:
        case NODE_KIND_SHL:
        case NODE_KIND_SHR:
        case NODE_KIND_BITWISE_AND:
        case NODE_KIND_BITWISE_OR:
        case NODE_KIND_BITWISE_XOR:
            walk(node->lhs);
            walk(node->rhs);

            if ((node->kind == NODE_KIND_EQ || node->kind == NODE_KIND_NE) && 
                (node->lhs->ty->kind == TY_STRUCT || node->rhs->ty->kind == TY_STRUCT)) {
                fprintf(stderr, "Erro na linha %d: Comparação direta de structs ('==', '!=') não suportada. Use uma função de comparação.\n", node->tok ? node->tok->line : 0);
                exit(1);
            }

            if (node->lhs->ty->kind == TY_STRUCT || node->rhs->ty->kind == TY_STRUCT) {
                fprintf(stderr, "Erro na linha %d: Operacao invalida com structs.\n", node->tok ? node->tok->line : 0);
                exit(1);
            }

            if (node->lhs->kind == NODE_KIND_NUM && node->rhs->kind != NODE_KIND_NUM) {
                long val = node->lhs->val;
                Type *target = node->rhs->ty;
                if ((target == type_i8 && val >= -128 && val <= 127) ||
                    (target == type_i16 && val >= -32768 && val <= 32767) ||
                    (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) ||
                    (target == type_u8 && val >= 0 && val <= 255) ||
                    (target == type_u16 && val >= 0 && val <= 65535) ||
                    (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL)) {
                    node->lhs->ty = target;
                }
            }

            if (node->rhs->kind == NODE_KIND_NUM && node->lhs->kind != NODE_KIND_NUM) {
                long val = node->rhs->val;
                Type *target = node->lhs->ty;
                if ((target == type_i8 && val >= -128 && val <= 127) ||
                    (target == type_i16 && val >= -32768 && val <= 32767) ||
                    (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) ||
                    (target == type_u8 && val >= 0 && val <= 255) ||
                    (target == type_u16 && val >= 0 && val <= 65535) ||
                    (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL)) {
                    node->rhs->ty = target;
                }
            }

            // Pointer Arithmetic
            if (node->kind == NODE_KIND_ADD || node->kind == NODE_KIND_SUBTRACTION) {
                if (node->lhs->ty->kind == TY_PTR && (node->rhs->ty->kind == TY_I32 || node->rhs->ty->kind == TY_I64)) {
                    node->ty = node->lhs->ty;
                    return;
                }
                if (node->rhs->ty->kind == TY_PTR && (node->lhs->ty->kind == TY_I32 || node->lhs->ty->kind == TY_I64)) {
                    if (node->kind == NODE_KIND_SUBTRACTION) {
                        fprintf(stderr, "Erro: subtracao de ponteiro por inteiro invalida (int - ptr).\n");
                        exit(1);
                    }
                    node->ty = node->rhs->ty;
                    return;
                }
                if (node->lhs->ty->kind == TY_PTR && node->rhs->ty->kind == TY_PTR) {
                    if (node->kind == NODE_KIND_SUBTRACTION) {
                        node->ty = type_i64; // ptrdiff_t
                        return;
                    } else {
                        fprintf(stderr, "Erro: soma de dois ponteiros invalida.\n");
                        exit(1);
                    }
                }
            }

            Type *target_type;
            if (node->lhs->ty->size > node->rhs->ty->size) {
                target_type = node->lhs->ty;
            } else {
                target_type = node->rhs->ty;
            }

            if (!is_safe_implicit_conversion(node->lhs->ty, target_type)) {
                fprintf(stderr, "Erro: conversão implícita não segura de tipo menor/incompatível para maior. Use cast explícito.\n");
                exit(1);
            }
            if (!is_safe_implicit_conversion(node->rhs->ty, target_type)) {
                fprintf(stderr, "Erro: conversão implícita não segura de tipo menor/incompatível para maior. Use cast explícito.\n");
                exit(1);
            }

            node->ty = target_type;
            return;

        case NODE_KIND_LOGICAL_AND:
        case NODE_KIND_LOGICAL_OR:
            walk(node->lhs);
            walk(node->rhs);
            
            if (node->lhs->ty->kind == TY_STRUCT || node->rhs->ty->kind == TY_STRUCT) {
                fprintf(stderr, "Erro na linha %d: Operacao invalida com structs.\n", node->tok ? node->tok->line : 0);
                exit(1);
            }

            // Check if operands are boolean-like (integers or bool)
            // For now, we allow integers as booleans (C-style)
            node->ty = type_bool;
            return;

        case NODE_KIND_BREAK:
            if (loop_depth == 0) {
                fprintf(stderr, "Erro: 'break' fora de loop.\n");
                exit(1);
            }
            return;

        case NODE_KIND_CONTINUE:
            if (loop_depth == 0) {
                fprintf(stderr, "Erro: 'continue' fora de loop.\n");
                exit(1);
            }
            return;

        case NODE_KIND_IF: {
            walk(node->if_stmt.cond);
            TypeKind tk = node->if_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do if deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            walk(node->if_stmt.then_b);
            if (node->if_stmt.else_b) {
                walk(node->if_stmt.else_b);
            }
            return;
        }
        case NODE_KIND_WHILE: {
            walk(node->while_stmt.cond);
            TypeKind tk = node->while_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do while deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            loop_depth++;
            walk(node->while_stmt.body);
            loop_depth--;
            return;
        }

        case NODE_KIND_DO_WHILE: {
            loop_depth++;
            walk(node->do_while_stmt.body);
            loop_depth--;
            walk(node->do_while_stmt.cond);
            TypeKind tk = node->do_while_stmt.cond->ty->kind;
            if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                tk != TY_BOOL && tk != TY_INT) {
                fprintf(stderr, "Erro: condição do do-while deve ser um booleano ou inteiro.\n");
                exit(1);
            }
            return;
        }

        case NODE_KIND_FOR: {
            symtab_enter_scope();
            if (node->for_stmt.init) walk(node->for_stmt.init);
            
            if (node->for_stmt.cond) {
                walk(node->for_stmt.cond);
                TypeKind tk = node->for_stmt.cond->ty->kind;
                if (tk != TY_I8 && tk != TY_I16 && tk != TY_I32 && tk != TY_I64 &&
                    tk != TY_U8 && tk != TY_U16 && tk != TY_U32 && tk != TY_U64 &&
                    tk != TY_BOOL && tk != TY_INT) {
                    fprintf(stderr, "Erro: condição do for deve ser um booleano ou inteiro.\n");
                    exit(1);
                }
            }
            
            if (node->for_stmt.step) walk(node->for_stmt.step);
            
            loop_depth++;
            walk(node->for_stmt.body);
            loop_depth--;
            
            symtab_exit_scope();
            return;
        }
        case NODE_KIND_ADDR:
            walk(node->expr);
            if (node->expr->kind != NODE_KIND_IDENTIFIER && 
                node->expr->kind != NODE_KIND_DEREF && 
                node->expr->kind != NODE_KIND_INDEX) {
                fprintf(stderr, "Erro: operador '&' requer um lvalue (variável, dereferência ou índice).\n");
                exit(1);
            }
            // Create pointer type
            Type *ptr_ty = new_type(TY_PTR);
            ptr_ty->base = node->expr->ty;
            ptr_ty->size = 8;
            node->ty = ptr_ty;
            return;

        case NODE_KIND_DEREF:
            walk(node->expr);
            if (node->expr->ty->kind != TY_PTR) {
                fprintf(stderr, "Erro: operador '*' requer um ponteiro.\n");
                exit(1);
            }
            node->ty = node->expr->ty->base;
            return;

        case NODE_KIND_NEG: {
            walk(node->expr);
            // Unary minus works on integers and floats
            TypeKind neg_tk = node->expr->ty->kind;
            if (neg_tk != TY_I8 && neg_tk != TY_I16 && neg_tk != TY_I32 && neg_tk != TY_I64 &&
                neg_tk != TY_U8 && neg_tk != TY_U16 && neg_tk != TY_U32 && neg_tk != TY_U64 &&
                neg_tk != TY_F32 && neg_tk != TY_F64 && neg_tk != TY_INT) {
                fprintf(stderr, "Erro: operador '-' unário requer um tipo numérico.\n");
                exit(1);
            }
            node->ty = node->expr->ty;
            return;
        }

        case NODE_KIND_BITWISE_NOT: {
            walk(node->expr);
            // Bitwise NOT works only on integers
            TypeKind bnot_tk = node->expr->ty->kind;
            if (bnot_tk != TY_I8 && bnot_tk != TY_I16 && bnot_tk != TY_I32 && bnot_tk != TY_I64 &&
                bnot_tk != TY_U8 && bnot_tk != TY_U16 && bnot_tk != TY_U32 && bnot_tk != TY_U64 &&
                bnot_tk != TY_INT) {
                fprintf(stderr, "Erro: operador '~' requer um tipo inteiro.\n");
                exit(1);
            }
            node->ty = node->expr->ty;
            return;
        }

        case NODE_KIND_LOGICAL_NOT: {
            walk(node->expr);
            // Logical NOT works on integers and booleans, returns bool
            TypeKind lnot_tk = node->expr->ty->kind;
            if (lnot_tk != TY_I8 && lnot_tk != TY_I16 && lnot_tk != TY_I32 && lnot_tk != TY_I64 &&
                lnot_tk != TY_U8 && lnot_tk != TY_U16 && lnot_tk != TY_U32 && lnot_tk != TY_U64 &&
                lnot_tk != TY_BOOL && lnot_tk != TY_INT) {
                fprintf(stderr, "Erro: operador '!' requer um tipo inteiro ou booleano.\n");
                exit(1);
            }
            node->ty = new_type(TY_BOOL);
            node->ty->size = 1;
            return;
        }

        case NODE_KIND_INDEX:
            walk(node->lhs);
            walk(node->rhs);

            if (node->lhs->ty->kind != TY_ARRAY && node->lhs->ty->kind != TY_PTR) {
                fprintf(stderr, "Erro: indexação requer array ou ponteiro.\n");
                exit(1);
            }

            // Check index type (must be integer)
            TypeKind idx_kind = node->rhs->ty->kind;
            if (idx_kind != TY_I32 && idx_kind != TY_I64 && idx_kind != TY_U32 && idx_kind != TY_U64 && idx_kind != TY_INT) {
                 fprintf(stderr, "Erro: índice deve ser um inteiro.\n");
                 exit(1);
            }

            node->ty = node->lhs->ty->base;
            return;

        case NODE_KIND_ASSIGN: {
            walk(node->lhs);
            walk(node->rhs);

            // Check if LHS is a valid lvalue
            if (node->lhs->kind != NODE_KIND_IDENTIFIER && 
                node->lhs->kind != NODE_KIND_DEREF && 
                node->lhs->kind != NODE_KIND_INDEX &&
                node->lhs->kind != NODE_KIND_MEMBER_ACCESS) {
                fprintf(stderr, "Erro: alvo da atribuição inválido (deve ser variável, dereferência, índice ou membro).\n");
                exit(1);
            }

            if (node->lhs->kind == NODE_KIND_IDENTIFIER) {
                Variable *var = node->lhs->var;
                // Variables are mutable by default. Only immutable if explicitly marked with 'imut'.
                if (var->flags & VAR_FLAG_IMUT) {
                    fprintf(stderr, "Erro: não é possível atribuir à variável imutável '%s'.\n", var->name);
                    exit(1);
                }
            }
            // Note: For DEREF and INDEX, we assume memory is mutable for now, 
            // or we'd need to track mutability in the type system (e.g. *mut T vs *T).
            // For now, let's allow it.

            int allow_literal_narrowing = 0;
            if (node->rhs->kind == NODE_KIND_NUM) {
                long val = node->rhs->val;
                Type *target = node->lhs->ty;
                if (target == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                else if (target == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                else if (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                else if (target == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                else if (target == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                else if (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
            }

            if (!allow_literal_narrowing && !is_safe_implicit_conversion(node->rhs->ty, node->lhs->ty)) {
                fprintf(stderr, "Erro em %s:%d: conversão implícita não segura na atribuição (de '%s' para '%s'). Use cast explícito.\n",
                    node->tok ? node->tok->filename : "?",
                    node->tok ? node->tok->line : 0,
                    type_name(node->rhs->ty),
                    type_name(node->lhs->ty));
                exit(1);
            }

            node->ty = node->lhs->ty;
            return;
        }

        case NODE_KIND_CAST:
            walk(node->expr);
            return;

        case NODE_KIND_SIZEOF:
            resolve_type_ref(node->ty);
            node->kind = NODE_KIND_NUM;
            node->val = node->ty->size;
            node->ty = type_i32; // sizeof returns i32 (or size_t which we map to i32/i64)
            return;

        case NODE_KIND_FNCALL: {
            Function *func = NULL;
            // fprintf(stderr, "Debug: Visiting FNCALL. LHS kind: %d\n", node->lhs->kind);
            
            // Case 1: Direct Identifier Call (func())
            if (node->lhs->kind == NODE_KIND_IDENTIFIER) {
                func = lookup_function(node->lhs->name);
                if (!func && node->generic_arg_count > 0 && node->member_name) {
                    // Generic function call: try to instantiate from template
                    GenericTemplate *tmpl = lookup_generic_template(node->member_name);
                    if (tmpl && tmpl->is_function) {
                        char *mangled = mangle_generic_name(tmpl->name, node->generic_args, node->generic_arg_count);
                        // Update the LHS name to match the mangled name
                        free(node->lhs->name);
                        node->lhs->name = strdup(mangled);
                        instantiate_generic_func(tmpl, node->generic_args, node->generic_arg_count, mangled);
                        func = lookup_function(mangled);
                        free(mangled);
                    }
                }
                if (!func) {
                    fprintf(stderr, "Erro: função '%s' não declarada.\n", node->lhs->name);
                    exit(1);
                }
            } 
            // Case 2: Member Access Call (mod.func() or struct.method())
            else if (node->lhs->kind == NODE_KIND_MEMBER_ACCESS) {
                Node *lhs_expr = node->lhs->lhs;
                char *member_name = node->lhs->member_name;
                
                // fprintf(stderr, "Debug: FNCALL Member Access. Member: %s\n", member_name);
                
                // Check if lhs is a Module (Namespace)
                if (lhs_expr->kind == NODE_KIND_IDENTIFIER) {
                    // fprintf(stderr, "Debug: LHS is Identifier: %s\n", lhs_expr->name);
                    Variable *var = symtab_lookup(lhs_expr->name);
                    if (var && var->is_module) {
                        // fprintf(stderr, "Debug: Module found. Var: %p, ModuleRef: %p\n", (void*)var, (void*)var->module_ref);
                        // It is a module call! (e.g. str.free)
                        // Resolve module prefix
                        if (!var->module_ref) {
                             fprintf(stderr, "Erro interno: variavel de modulo sem referencia de modulo.\n");
                             exit(1);
                        }
                        
                        // fprintf(stderr, "Debug: Looking up function '%s' in prefix '%s'\n", member_name, var->module_ref->path_prefix);
                        func = lookup_function_qualified(member_name, var->module_ref->path_prefix);
                        if (!func) {
                            fprintf(stderr, "Erro: função '%s' não encontrada no módulo '%s'.\n", member_name, lhs_expr->name);
                            exit(1);
                        }
                        // fprintf(stderr, "Debug: Function found: %p. Name: %s. AsmName: %s\n", (void*)func, func->name, func->asm_name);
                    } else {
                        // Not a module, assume struct member access (method or function pointer)
                        // For now, we don't support methods or function pointers in structs fully yet in this pass?
                        // Or maybe we do? The user said "Futuro/Erro por enquanto".
                        // Let's emit error for now to be safe, or fall through if we want to support function pointers later.
                        fprintf(stderr, "Erro: métodos ou ponteiros de função em structs ainda não suportados para '%s.%s'.\n", lhs_expr->name, member_name);
                        exit(1);
                    }
                } else {
                    // Complex expression on LHS (e.g. get_struct().method())
                    fprintf(stderr, "Erro: chamadas em expressões complexas ainda não suportadas.\n");
                    exit(1);
                }
            } else {
                fprintf(stderr, "Erro: tipo de chamada de função inválido.\n");
                exit(1);
            }

            node->ty = func->return_type;
            // Store asm_name in the node for codegen - must strdup to avoid shared ownership
            if (node->name) free(node->name);  // Free original parsed name
            node->name = strdup(func->asm_name); 
            
            // fprintf(stderr, "Debug: Processing args for function '%s'\n", func->name);
            
            // Verify arguments
            int arg_count = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                // fprintf(stderr, "Debug: Counting arg %p\n", (void*)arg);
                arg_count++;
            }
            // 3. Verificar número de argumentos
            int param_count = func->param_count;
            if (func->is_variadic) {
                if (arg_count < param_count) {
                    fprintf(stderr, "Erro: função '%s' espera pelo menos %d argumentos, mas recebeu %d.\n", func->name, param_count, arg_count);
                    exit(1);
                }
            } else {
                if (arg_count != param_count) {
                    fprintf(stderr, "Erro: função '%s' espera %d argumentos, mas recebeu %d.\n", func->name, param_count, arg_count);
                    exit(1);
                }
            }

            // 4. Verificar tipos dos argumentos
            Node *arg = node->args;
            int i = 0;
            while (i < param_count) { // Iterate only for fixed parameters
                walk(arg);

                // Allow implicit conversion for integer literals that fit in param type
                int allow_literal_narrowing = 0;
                if (arg->kind == NODE_KIND_NUM) {
                    long val = arg->val;
                    Type *target = func->param_types[i];
                    if (target == type_i8 && val >= -128 && val <= 127) allow_literal_narrowing = 1;
                    else if (target == type_i16 && val >= -32768 && val <= 32767) allow_literal_narrowing = 1;
                    else if (target == type_i32 && val >= -2147483648LL && val <= 2147483647LL) allow_literal_narrowing = 1;
                    else if (target == type_u8 && val >= 0 && val <= 255) allow_literal_narrowing = 1;
                    else if (target == type_u16 && val >= 0 && val <= 65535) allow_literal_narrowing = 1;
                    else if (target == type_u32 && val >= 0 && (unsigned long)val <= 4294967295ULL) allow_literal_narrowing = 1;
                    else if (target == type_i64) allow_literal_narrowing = 1;
                    else if (target == type_u64 && val >= 0) allow_literal_narrowing = 1;
                    if (allow_literal_narrowing) {
                        arg->ty = target;
                    }
                }

                // Check type compatibility
                if (!allow_literal_narrowing && !is_safe_implicit_conversion(arg->ty, func->param_types[i])) {
                     fprintf(stderr, "Erro: tipo incompatível no argumento %d da função '%s' (esperado '%s', encontrado '%s').\n",
                             i+1, func->name, type_name(func->param_types[i]), type_name(arg->ty));
                     exit(1);
                }
                arg = arg->next;
                i++;
            }
            
            // Check remaining args for variadic (optional: check if they are valid types)
            while (arg) {
                walk(arg); // Just walk them, no type checking against parameters for variadic args
                arg = arg->next;
            }
            return;
        }

        case NODE_KIND_SYSCALL:
            node->ty = type_i64; // Syscalls always return i64
            for (Node *arg = node->args; arg; arg = arg->next) {
                walk(arg);
            }
            return;

            return;
            
        case NODE_KIND_USE:
            // Handled in analyze passes
            return;

        case NODE_KIND_MODULE_ACCESS:
        case NODE_KIND_MEMBER_ACCESS:
            // fprintf(stderr, "Debug: Visiting MEMBER_ACCESS. Node: %p\n", (void*)node);
            // Check if LHS is a module
            if (node->lhs->kind == NODE_KIND_IDENTIFIER) {
                Variable *var = symtab_lookup(node->lhs->name);
                if (var && var->is_module) {
                    // Module access
                    Module *mod = var->module_ref;
                    char *member_name = node->member_name;
                    
                    // fprintf(stderr, "Debug: Accessing module '%s'. Member: '%s'\n", mod->name, member_name);

                    // Try to find function in module
                    Function *fn = lookup_function_qualified(member_name, mod->path_prefix);
                    if (fn) {
                        // ... (omitted for brevity) ...
                        node->kind = NODE_KIND_IDENTIFIER;
                        node->name = strdup(fn->asm_name); 
                        node->ty = fn->return_type; 
                        return;
                    }
                    
                    // Try to find variable in module scope
                    // fprintf(stderr, "Debug: Searching variable '%s' in module scope %p\n", member_name, (void*)mod->scope);
                    if (!mod->scope) {
                        fprintf(stderr, "Erro interno: modulo '%s' sem escopo.\n", mod->name);
                        exit(1);
                    }

                    for (Variable *v = mod->scope->vars; v; v = v->next) {
                        if (strcmp(v->name, member_name) == 0) {
                            node->kind = NODE_KIND_IDENTIFIER;
                            if (v->asm_name) {
                                node->name = strdup(v->asm_name);
                            } else {
                                node->name = strdup(v->name);
                            }
                            node->var = v;
                            node->ty = v->type;
                            node->offset = v->offset;
                            return;
                        }
                    }
                    
                    fprintf(stderr, "Erro: membro '%s' não encontrado no módulo '%s'\n", member_name, mod->name);
                    exit(1);
                }
            }

            walk(node->lhs);
            
            Type *struct_ty = node->lhs->ty;
            if (struct_ty->kind == TY_PTR) {
                struct_ty = struct_ty->base;
            }
            
            // If LHS is a module (but resolved to something else?), error?
            // If LHS was identifier resolved to module, we handled it above.
            // If LHS is not identifier (e.g. expr), it can't be module.
            
            if (struct_ty->kind != TY_STRUCT) {
                fprintf(stderr, "Erro: operador '.' requer struct ou ponteiro para struct.\n");
                exit(1);
            }

            Field *field = NULL;
            for (Field *f = struct_ty->fields; f; f = f->next) {
                if (strcmp(f->name, node->member_name) == 0) {
                    field = f;
                    break;
                }
            }

            if (!field) {
                fprintf(stderr, "Erro: struct '%s' não possui membro '%s'.\n", struct_ty->name, node->member_name);
                exit(1);
            }

            node->ty = field->type;
            node->offset = field->offset;
            return;
    }
}

static void register_structs(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            register_structs(n->body);
            
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_BLOCK && n->ty && n->ty->kind == TY_STRUCT) {
            // If generic template, store and skip
            if (n->ty->generic_param_count > 0) {
                store_generic_template(n->ty->name, n->ty->generic_params, n->ty->generic_param_count, NULL, n->ty, 0);
                continue;
            }
            // Mangle struct name if in module
            char *name = n->ty->name;
            char *mangled = NULL;
            if (current_module_path) {
                int len = strlen(current_module_path) + 1 + strlen(name) + 1;
                mangled = malloc(len);
                snprintf(mangled, len, "%s_%s", current_module_path, name);
                name = mangled;
            }
            declare_struct(name, n->ty);
            if (mangled) free(mangled);
        }
    }
}

static Module *find_module_by_prefix(char *prefix) {
    for (Module *m = module_list; m; m = m->next) {
        if (strcmp(m->path_prefix, prefix) == 0) {
            return m;
        }
    }
    return NULL;
}

static void register_aliases(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            // If alias, create module scope. If not, use current scope.
            Scope *outer_scope = current_scope;
            Scope *mod_scope = outer_scope;
            
            Module *existing = NULL;
            if (n->name) {
                existing = find_module_by_prefix(prefix);
                if (existing) {
                    mod_scope = existing->scope;
                } else {
                    mod_scope = calloc(1, sizeof(Scope));
                    mod_scope->next_allocated = all_scopes; all_scopes = mod_scope;
                    mod_scope->parent = global_scope; 
                    mod_scope->is_global_scope = 1;
                }
                current_scope = mod_scope;
            }
            
            register_aliases(n->body);
            
            current_scope = outer_scope;
            
            if (n->name) {
                if (!existing) {
                    // Create Module symbol
                    Module *mod = calloc(1, sizeof(Module));
                    mod->name = strdup(n->name);
                    mod->path_prefix = strdup(prefix);
                    mod->scope = mod_scope;
                    mod->next = module_list;
                    module_list = mod;
                    existing = mod;
                }
                
                // Declare alias in outer scope
                Variable *var = symtab_declare(n->name, type_void, VAR_FLAG_NONE);
                var->is_module = 1;
                var->module_ref = existing;
            }
            
            current_module_path = old_path;
            if (prefix) free(prefix);
        }
    }
}

static void resolve_fields(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            Scope *outer_scope = current_scope;
            if (n->name) {
                Variable *var = symtab_lookup(n->name);
                if (var && var->is_module) {
                    current_scope = var->module_ref->scope;
                }
            }
            
            resolve_fields(n->body);
            
            current_scope = outer_scope;
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_BLOCK && n->ty && n->ty->kind == TY_STRUCT) {
            // Skip generic templates
            if (n->ty->generic_param_count > 0) continue;

            // Resolve fields for this struct
            Type *type = n->ty;
            int offset = 0;
            for (Field *f = type->fields; f; f = f->next) {
                resolve_type_ref(f->type);

                if (f->type->kind == TY_STRUCT && strcmp(f->type->name, type->name) == 0) {
                     fprintf(stderr, "Erro: struct '%s' contém a si mesma diretamente no campo '%s'. Use um ponteiro.\n", type->name, f->name);
                     exit(1);
                }

                f->offset = offset;
                offset += f->type->size;
            }
            type->size = offset;
        }
    }
}

static void register_vars(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            Scope *outer_scope = current_scope;
            if (n->name) {
                Variable *var = symtab_lookup(n->name);
                if (var && var->is_module) {
                    current_scope = var->module_ref->scope;
                }
            }
            
            register_vars(n->body);
            
            current_scope = outer_scope;
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_LET) {
            walk(n); 
        }
    }
}

static void register_funcs(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            Scope *outer_scope = current_scope;
            if (n->name) {
                Variable *var = symtab_lookup(n->name);
                if (var && var->is_module) {
                    current_scope = var->module_ref->scope;
                }
            }
            
            register_funcs(n->body);
            
            current_scope = outer_scope;
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_FN) {
            // If generic template, store and skip
            if (n->generic_param_count > 0) {
                store_generic_template(n->name, n->generic_params, n->generic_param_count, n, NULL, 1);
                continue;
            }

             // Resolve param types and return type
            for (Node *p = n->params; p; p = p->next) {
                resolve_type_ref(p->ty);
                if (p->ty->kind == TY_VOID) {
                    fprintf(stderr, "Erro na linha %d: Parâmetro '%s' não pode ser do tipo void.\n", p->tok ? p->tok->line : 0, p->name);
                    exit(1);
                }
            }
            resolve_type_ref(n->return_type);

            int param_count = 0;
            for (Node *p = n->params; p; p = p->next) param_count++;

            Type **param_types = calloc(param_count, sizeof(Type*));
            int i = 0;
            for (Node *p = n->params; p; p = p->next) {
                param_types[i++] = p->ty;
            }

            declare_function(n->name, n->return_type, param_types, param_count, n->is_variadic);
        }
    }
}

static void analyze_bodies(Node *node) {
    for (Node *n = node; n; n = n->next) {
        if (n->kind == NODE_KIND_USE) {
            char *prefix = NULL;
            if (n->name) prefix = get_module_prefix(n->member_name);
            
            char *old_path = current_module_path;
            if (prefix) current_module_path = prefix;
            
            Scope *outer_scope = current_scope;
            if (n->name) {
                Variable *var = symtab_lookup(n->name);
                if (var && var->is_module) {
                    current_scope = var->module_ref->scope;
                }
            }
            
            analyze_bodies(n->body);
            
            current_scope = outer_scope;
            current_module_path = old_path;
            if (prefix) free(prefix);
        } else if (n->kind == NODE_KIND_FN) {
            // Skip generic templates — they are instantiated on demand
            if (n->generic_param_count > 0) continue;
            walk(n);
        }
    }
}

void analyze(Node *node) {
    symtab_init();
    function_table = NULL;
    struct_defs = NULL;
    module_list = NULL;
    generic_templates = NULL;
    generic_instances = NULL;

    // Set ast_tail to the last node in the AST (for appending generic instantiations)
    ast_tail = node;
    if (ast_tail) {
        while (ast_tail->next) ast_tail = ast_tail->next;
    }

    // Register intrinsics
    // __builtin_va_start(va_list_ptr) -> void
    Type *ptr_type = new_type(TY_PTR);
    ptr_type->base = type_u8;
    ptr_type->size = 8;
    Type **va_params = calloc(1, sizeof(Type*));
    va_params[0] = ptr_type;
    declare_function("__builtin_va_start", type_void, va_params, 1, 0);

    register_structs(node);
    register_aliases(node);
    resolve_fields(node);
    register_vars(node);

    register_funcs(node);
    analyze_bodies(node);
}

void semantic_cleanup() {
    // Free function table
    Function *fs = function_table;
    while (fs) {
        Function *next = fs->next;
        if (fs->name) free(fs->name);
        if (fs->asm_name) free(fs->asm_name);
        if (fs->module_prefix) free(fs->module_prefix);
        if (fs->param_types) free(fs->param_types);
        free(fs);
        fs = next;
    }
    function_table = NULL;
    
    // Free struct definitions
    StructDef *sd = struct_defs;
    while (sd) {
        StructDef *next = sd->next;
        if (sd->name) free(sd->name);
        free(sd);
        sd = next;
    }
    struct_defs = NULL;

    // Free module list (only the module structs, scopes are in all_scopes)
    Module *mod = module_list;
    while (mod) {
        Module *next = mod->next;
        if (mod->name) free(mod->name);
        if (mod->path_prefix) free(mod->path_prefix);
        // mod->scope is freed via all_scopes list
        free(mod);
        mod = next;
    }
    module_list = NULL;

    // Free ALL scopes (global, module, and block scopes)
    Scope *s = all_scopes;
    while (s) {
        Scope *next = s->next_allocated;
        
        Variable *v = s->vars;
        while (v) {
            Variable *vnext = v->next;
            if (v->name) free(v->name);
            if (v->asm_name) free(v->asm_name);
            free(v);
            v = vnext;
        }
        
        free(s);
        s = next;
    }
    all_scopes = NULL;
    global_scope = NULL;
    current_scope = NULL;

    // Free generic templates
    GenericTemplate *gt = generic_templates;
    while (gt) {
        GenericTemplate *next = gt->next;
        if (gt->name) free(gt->name);
        if (gt->module_prefix) free(gt->module_prefix);
        if (gt->params) {
            for (int i = 0; i < gt->param_count; i++) {
                if (gt->params[i]) free(gt->params[i]);
            }
            free(gt->params);
        }
        free(gt);
        gt = next;
    }
    generic_templates = NULL;

    // Free generic instances
    GenericInstance *gi = generic_instances;
    while (gi) {
        GenericInstance *next = gi->next;
        if (gi->mangled_name) free(gi->mangled_name);
        free(gi);
        gi = next;
    }
    generic_instances = NULL;
    ast_tail = NULL;

    // Types are freed by free_all_types() in main.c
}
