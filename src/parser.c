#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "parser.h"
#include "lexer.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static Token current_token;
static Token lookahead_token;
static int in_fn_params = 0; // Set when parsing fn params to disambiguate generic commas

static StringLiteral *strings_head = NULL;
static int string_id_counter = 0;

static int register_string(char *val) {
    StringLiteral *s = calloc(1, sizeof(StringLiteral));
    s->id = string_id_counter++;
    s->value = val;
    s->next = strings_head;
    strings_head = s;
    return s->id;
}

typedef struct IncludedFile {
    char *path;
    Node *body;
    struct IncludedFile *next;
} IncludedFile;

typedef struct SourcePath {
    char *path;
    struct SourcePath *next;
} SourcePath;

static SourcePath *source_paths = NULL;

void register_source_path(char *path) {
    SourcePath *sp = malloc(sizeof(SourcePath));
    sp->path = path;
    sp->next = source_paths;
    source_paths = sp;
}

void free_source_paths() {
    SourcePath *cur = source_paths;
    while (cur) {
        SourcePath *next = cur->next;
        free(cur->path);
        free(cur);
        cur = next;
    }
}

static IncludedFile *visited_files = NULL;


Node *get_cached_module(const char *path) {
    IncludedFile *cur = visited_files;
    while (cur) {
        if (strcmp(cur->path, path) == 0) {
            return cur->body;
        }
        cur = cur->next;
    }
    return NULL;
}

void cache_module(const char *path, Node *body) {
    IncludedFile *new_file = calloc(1, sizeof(IncludedFile));
    new_file->path = strdup(path);
    new_file->body = body;
    new_file->next = visited_files;
    visited_files = new_file;
}

void free_module_cache() {
    IncludedFile *cur = visited_files;
    while (cur) {
        IncludedFile *next = cur->next;
        free(cur->path);
        free_ast(cur->body);  // Now safe to free since we skip USE bodies
        free(cur);
        cur = next;
    }
    visited_files = NULL;
}

StringLiteral *get_strings() {
    return strings_head;
}

void free_strings() {
    StringLiteral *s = strings_head;
    while (s) {
        StringLiteral *next = s->next;
        if (s->value) free(s->value);
        free(s);
        s = next;
    }
    strings_head = NULL;
    string_id_counter = 0;
}

static void consume();
static Node *parse_block();
static Node *parse_fn();
static void expect(TokenType type);
static Node *parse_var_decl();
static Node *parse_expr();
static Node *parse_stmt();
static Node *parse_if_stmt();
static Node *parse_if_stmt();
static Node *parse_logical_or();
static Node *parse_logical_and();
static Node *parse_comparison();
static Node *parse_add();
static Node *parse_mul();
static Node *parse_primary();
static Node *parse_comparison();
static Node *parse_add();
static Node *parse_mul();
static Node *parse_primary();
static Type *parse_type();
static Node *parse_struct_decl();
static Node *parse_enum_decl();
static Node *parse_match_stmt();
static void parse_file_body(Node **cur_ptr);
static void parse_use(Node **cur_ptr);
static Node *parse_extern_fn();
static Node *new_node(NodeKind kind);


void parser_init() {
    lookahead_token = lexer_next();
    consume();
}

static void parse_use(Node **cur_ptr) {
    expect(TOKEN_TYPE_USE);

    if (current_token.type != TOKEN_TYPE_STRING) {
        error_at_token(current_token, "esperado caminho do arquivo apos 'use'");
    }

    char path[256];
    strncpy(path, current_token.text, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    consume(); // Consume string token

    char *alias = NULL;
    if (current_token.type == TOKEN_TYPE_AS) {
        consume();
        if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
            error_at_token(current_token, "esperado identificador apos 'as'");
        }
        alias = strdup(current_token.text);
        consume();
    }

    expect(TOKEN_TYPE_SEMICOLON);

    Node *use_node = new_node(NODE_KIND_USE);
    use_node->name = alias; // Store alias in name

    // Resolve path relative to current file
    char full_path[512];
    if (path[0] == '/') {
        strncpy(full_path, path, sizeof(full_path));
    } else {
        const char *current_file = current_token.filename;
        if (!current_file) current_file = ".";
        
        char *dir = strdup(current_file);
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            dir[0] = '.';
            dir[1] = '\0';
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, path);
        free(dir);
    }

    char canonical_path[4096];
    if (!realpath(full_path, canonical_path)) {
        // If realpath fails (e.g. file not found), use full_path for error reporting later
        strncpy(canonical_path, full_path, sizeof(canonical_path));
    }

    use_node->member_name = strdup(canonical_path);

    // Check cache
    Node *cached_body = get_cached_module(canonical_path);
    if (cached_body) {
        use_node->body = cached_body;
    } else {
        FILE *f = fopen(canonical_path, "r");
        if (!f) {
            error_at_token(current_token, "nao foi possivel abrir o arquivo '%s'", canonical_path);
        }
        
        // We need to keep the filename string alive for tokens
        char *persistent_path = strdup(canonical_path);
        register_source_path(persistent_path);

        LexerState state;
        lexer_get_state(&state);
        Token saved_curr = current_token;
        Token saved_look = lookahead_token;

        lexer_init(f, persistent_path);
        parser_init();

        // Parse into a new list for the module
        Node module_head = {0}; // Dummy head
        Node *module_cur = &module_head;
        
        parse_file_body(&module_cur);
        
        use_node->body = module_head.next;
        cache_module(canonical_path, use_node->body);
        // free dummy_head? (careful with GC/memory)

        lexer_set_state(&state);
        current_token = saved_curr;
        lookahead_token = saved_look;
        
        fclose(f);
    }

    // Append use_node to current AST
    (*cur_ptr)->next = use_node;
    *cur_ptr = use_node;
}

static void parse_file_body(Node **cur_ptr) {
    Node *cur = *cur_ptr;
    while (current_token.type != TOKEN_TYPE_EOF) {
        if (current_token.type == TOKEN_TYPE_USE) {
            parse_use(&cur);
            continue;
        }

        if (current_token.type == TOKEN_TYPE_STRUCT) {
             cur->next = parse_struct_decl();
             cur = cur->next;
             continue;
        }

        if (current_token.type == TOKEN_TYPE_ENUM) {
             cur->next = parse_enum_decl();
             cur = cur->next;
             continue;
        }

        if (current_token.type == TOKEN_TYPE_LET) {
            cur->next = parse_var_decl();
            cur = cur->next;
            continue;
        }

        if (current_token.type == TOKEN_TYPE_EXTERN) {
            cur->next = parse_extern_fn();
            cur = cur->next;
            continue;
        }

        if (current_token.type == TOKEN_TYPE_IMPL) {
            consume(); // impl
            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                error_at_token(current_token, "esperado nome do tipo após 'impl'.");
            }
            char *type_name = strdup(current_token.text);
            consume();

            // Parse optional generic params: impl Vec gen T, U { ... }
            char **impl_generic_params = NULL;
            int impl_generic_count = 0;
            if (current_token.type == TOKEN_TYPE_GEN) {
                consume();
                int cap = 4;
                impl_generic_params = calloc(cap, sizeof(char*));
                while (current_token.type == TOKEN_TYPE_IDENTIFIER) {
                    if (impl_generic_count >= cap) {
                        cap *= 2;
                        impl_generic_params = realloc(impl_generic_params, cap * sizeof(char*));
                    }
                    impl_generic_params[impl_generic_count++] = strdup(current_token.text);
                    consume();
                    if (current_token.type == TOKEN_TYPE_COMMA) {
                        consume();
                    } else {
                        break;
                    }
                }
            }

            expect(TOKEN_TYPE_LBRACE);
            while (current_token.type != TOKEN_TYPE_RBRACE) {
                if (current_token.type != TOKEN_TYPE_FN) {
                    error_at_token(current_token, "esperado 'fn' dentro de bloco impl.");
                }
                Node *fn = parse_fn();
                // Mangle name: Type_method
                char mangled[512];
                snprintf(mangled, sizeof(mangled), "%s_%s", type_name, fn->name);
                free(fn->name);
                fn->name = strdup(mangled);
                fn->impl_type_name = strdup(type_name);

                // Copy generic params from impl to fn if fn doesn't have its own
                if (impl_generic_count > 0 && fn->generic_param_count == 0) {
                    fn->generic_params = calloc(impl_generic_count, sizeof(char*));
                    fn->generic_param_count = impl_generic_count;
                    for (int i = 0; i < impl_generic_count; i++) {
                        fn->generic_params[i] = strdup(impl_generic_params[i]);
                    }
                }

                cur->next = fn;
                cur = cur->next;
            }
            expect(TOKEN_TYPE_RBRACE);
            for (int i = 0; i < impl_generic_count; i++) free(impl_generic_params[i]);
            free(impl_generic_params);
            free(type_name);
            continue;
        }

        if (current_token.type != TOKEN_TYPE_FN) {
            error_at_token(current_token, "era esperado 'fn', 'extern', 'impl', 'let', 'struct', 'enum' ou 'use' mas encontrou '%s'", current_token.text);
        }

        cur->next = parse_fn();
        cur = cur->next;
    }
    *cur_ptr = cur;
}

Node *parse() {
    Node head = {};
    Node *cur = &head;

    parse_file_body(&cur);


    return head.next;
}

// Global list of allocated types for GC
Type *all_types = NULL;

Type *new_type(TypeKind kind) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->next = all_types;
    ty->owns_fields = 1; // Default to owning fields
    all_types = ty;
    return ty;
}

void free_all_types() {
    Type *t = all_types;
    while (t) {
        Type *next = t->next;
        
        if (!t->is_static) {
            // Free type name if allocated
            if (t->name) free(t->name);
            
            // Free field list for struct types ONLY if we own them
            if (t->owns_fields) {
                Field *f = t->fields;
                while (f) {
                    Field *fnext = f->next;
                    if (f->name) free(f->name);
                    free(f);
                    f = fnext;
                }
            }
        }
        
        // Always free the Type struct itself (assuming it was malloc'd in new_type)
        if (!t->is_static) {
             free(t);
        } else {
             free(t); 
        }
        t = next;
    }
    all_types = NULL;
}

static void consume() {
    current_token = lookahead_token;
    lookahead_token = lexer_next();
}

static void expect(TokenType type) {
    if (current_token.type == type) {
        consume();
    } else {
        error_at_token(current_token, "esperado '%s' mas encontrou '%s'", TOKEN_NAMES[type], current_token.text);
    }
}

static Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_node_num(long val) {
    Node *node = new_node(NODE_KIND_NUM);
    node->val = val;
    return node;
}

static Node *new_node_return(Node *expr) {
    Node *node = new_node(NODE_KIND_RETURN);
    node->expr = expr;
    return node;
}

static Node *parse_expr() {
    Node *node = parse_logical_or();
    if (current_token.type == TOKEN_TYPE_EQUAL) {
        consume();
        Node *assign_node = new_node(NODE_KIND_ASSIGN);
        assign_node->lhs = node;
        assign_node->rhs = parse_expr(); // Right-associative
        return assign_node;
    }
    return node;
}

static Type *parse_type() {
    // Array: [N]T
    if (current_token.type == TOKEN_TYPE_LBRACKET) {
        consume(); // [
        if (current_token.type != TOKEN_TYPE_INTEGER) {
            error_at_token(current_token, "Tamanho do array deve ser um inteiro.");
        }
        int len = current_token.int_value;
        consume(); // N
        expect(TOKEN_TYPE_RBRACKET);
        Type *base_type = parse_type();
        Type *ty = new_type(TY_ARRAY);
        ty->base = base_type;
        ty->array_len = len;
        ty->size = len * base_type->size;
        return ty;
    }

    // Pointer: *T
    if (current_token.type == TOKEN_TYPE_MULTIPLIER) {
        consume();
        Type *base_type = parse_type();
        Type *ty = new_type(TY_PTR);
        ty->base = base_type;
        ty->size = 8; // 64-bit pointer
        return ty;
    }

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado um tipo");
    }

    Type *ty = NULL;

    // Check for namespaced type (e.g. string.String)
    // But only if it's NOT a known primitive type
    if (current_token.type == TOKEN_TYPE_IDENTIFIER && lookahead_token.type == TOKEN_TYPE_DOT) {
        // Check if the identifier is a known primitive — if so, skip the namespace path
        int is_primitive = 0;
        if (strcmp(current_token.text, "i8") == 0 || strcmp(current_token.text, "i16") == 0 ||
            strcmp(current_token.text, "i32") == 0 || strcmp(current_token.text, "i64") == 0 ||
            strcmp(current_token.text, "u8") == 0 || strcmp(current_token.text, "u16") == 0 ||
            strcmp(current_token.text, "u32") == 0 || strcmp(current_token.text, "u64") == 0 ||
            strcmp(current_token.text, "f32") == 0 || strcmp(current_token.text, "f64") == 0 ||
            strcmp(current_token.text, "bool") == 0 || strcmp(current_token.text, "char") == 0 ||
            strcmp(current_token.text, "void") == 0) {
            is_primitive = 1;
        }
        if (!is_primitive) {
            char *module_name = strdup(current_token.text);
            consume(); // consume module name
            consume(); // consume dot

            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                error_at_token(current_token, "esperado nome do tipo após '.'");
            }

            char *type_name_str = strdup(current_token.text);

            // Construct full name "module.Type"
            int len = strlen(module_name) + 1 + strlen(type_name_str) + 1;
            char *full_name = calloc(1, len);
            snprintf(full_name, len, "%s.%s", module_name, type_name_str);

            ty = new_type(TY_STRUCT);
            ty->name = full_name;

            free(module_name);
            free(type_name_str);

            consume(); // consume type name
            // Fall through to check for generic args (e.g. t.Result gen i32, i32)
        }
    }

    if (ty == NULL) {
        if (strcmp(current_token.text, "i32") == 0) {
            ty = type_i32;
        } else if (strcmp(current_token.text, "i8") == 0) {
            ty = type_i8;
        } else if (strcmp(current_token.text, "i16") == 0) {
            ty = type_i16;
        } else if (strcmp(current_token.text, "i64") == 0) {
            ty = type_i64;
        } else if (strcmp(current_token.text, "u8") == 0) {
            ty = type_u8;
        } else if (strcmp(current_token.text, "u16") == 0) {
            ty = type_u16;
        } else if (strcmp(current_token.text, "u32") == 0) {
            ty = type_u32;
        } else if (strcmp(current_token.text, "u64") == 0) {
            ty = type_u64;
        } else if (strcmp(current_token.text, "f32") == 0) {
            ty = type_f32;
        } else if (strcmp(current_token.text, "f64") == 0) {
            ty = type_f64;
        } else if (strcmp(current_token.text, "bool") == 0) {
            ty = type_bool;
        } else if (strcmp(current_token.text, "char") == 0) {
            ty = type_char;
        } else if (strcmp(current_token.text, "string") == 0) {
            ty = type_string;
        } else if (strcmp(current_token.text, "void") == 0) {
            ty = type_void;
        } else {
            ty = new_type(TY_STRUCT);
            ty->name = strdup(current_token.text);
        }

        consume();
    }

    // Parse generic type args: Vec gen i32, f64
    if (ty->kind == TY_STRUCT && current_token.type == TOKEN_TYPE_GEN) {
        consume();
        ty->base_name = strdup(ty->name);
        int cap = 4;
        ty->generic_args = calloc(cap, sizeof(Type*));
        ty->generic_arg_count = 0;

        // Build mangled name: Vec__i32_f64
        char mangled[512];
        snprintf(mangled, sizeof(mangled), "%s", ty->base_name);

        while (1) {
            Type *arg_type = parse_type();
            if (ty->generic_arg_count >= cap) {
                cap *= 2;
                ty->generic_args = realloc(ty->generic_args, cap * sizeof(Type*));
            }
            ty->generic_args[ty->generic_arg_count++] = arg_type;

            // Append type name to mangled
            const char *tname = "unknown";
            switch (arg_type->kind) {
                case TY_I8: tname = "i8"; break;
                case TY_I16: tname = "i16"; break;
                case TY_I32: tname = "i32"; break;
                case TY_I64: tname = "i64"; break;
                case TY_U8: tname = "u8"; break;
                case TY_U16: tname = "u16"; break;
                case TY_U32: tname = "u32"; break;
                case TY_U64: tname = "u64"; break;
                case TY_F32: tname = "f32"; break;
                case TY_F64: tname = "f64"; break;
                case TY_BOOL: tname = "bool"; break;
                case TY_CHAR: tname = "char"; break;
                case TY_STRING: tname = "string"; break;
                case TY_VOID: tname = "void"; break;
                case TY_PTR: tname = "ptr"; break;
                case TY_STRUCT: tname = arg_type->name ? arg_type->name : "struct"; break;
                default: break;
            }
            size_t cur_len = strlen(mangled);
            snprintf(mangled + cur_len, sizeof(mangled) - cur_len, "_%s", tname);

            if (current_token.type == TOKEN_TYPE_COMMA) {
                // In fn params, check if comma is a param separator (COMMA IDENTIFIER AS)
                if (in_fn_params && lookahead_token.type == TOKEN_TYPE_IDENTIFIER) {
                    // 2-token lookahead: save full state, peek, restore
                    LexerState lex_save;
                    lexer_get_state(&lex_save);
                    long file_pos = ftell(lex_save.file);
                    Token save_cur = current_token;
                    Token save_la = lookahead_token;

                    consume(); // cur = IDENTIFIER, la = lexer_next()
                    int is_param_boundary = (current_token.type == TOKEN_TYPE_IDENTIFIER &&
                                              lookahead_token.type == TOKEN_TYPE_AS);

                    // Restore everything
                    fseek(lex_save.file, file_pos, SEEK_SET);
                    lexer_set_state(&lex_save);
                    current_token = save_cur;
                    lookahead_token = save_la;

                    if (is_param_boundary) break;
                }
                consume(); // consume the comma — it's a generic arg separator
            } else {
                break;
            }
        }

        free(ty->name);
        ty->name = strdup(mangled);
    }

    return ty;
}


static Node* parse_var_decl() {
    expect(TOKEN_TYPE_LET);
    expect(TOKEN_TYPE_IS); // Enforce 'is'

    Type *var_type = parse_type();

    expect(TOKEN_TYPE_AS);

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "era esperado um nome de variável, mas encontrou '%s'", current_token.text);
    }
    char *var_name = strdup(current_token.text);
    consume();

    VarFlags var_flags = VAR_FLAG_NONE;
    if (current_token.type == TOKEN_TYPE_WITH) {
        consume();
        while (current_token.type == TOKEN_TYPE_IDENTIFIER) {
            if (strcmp(current_token.text, "mut") == 0) {
                var_flags |= VAR_FLAG_MUT;
            } else if (strcmp(current_token.text, "imut") == 0) {
                var_flags |= VAR_FLAG_IMUT;
            }
            consume();
        }
    }

    Node *node = new_node(NODE_KIND_LET);
    node->name = var_name;
    node->flags = var_flags;
    node->ty = var_type;

    if (current_token.type == TOKEN_TYPE_EQUAL) {
        consume();
        node->init_expr = parse_expr();
    }

    expect(TOKEN_TYPE_SEMICOLON);
    return node;
}

static Node *new_binary_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *parse_primary() {
    if (current_token.type == TOKEN_TYPE_SIZEOF) {
        consume();
        expect(TOKEN_TYPE_LPAREN);
        Type *ty = parse_type();
        expect(TOKEN_TYPE_RPAREN);
        
        Node *node = new_node(NODE_KIND_SIZEOF);
        node->ty = ty; // Store the type to measure
        return node;
    }

    if (current_token.type == TOKEN_TYPE_CAST) {
        consume();
        expect(TOKEN_TYPE_LPAREN);
        Type *ty = parse_type();
        expect(TOKEN_TYPE_COMMA);
        Node *expr = parse_expr();
        expect(TOKEN_TYPE_RPAREN);
        
        Node *node = new_node(NODE_KIND_CAST);
        node->ty = ty;
        node->expr = expr;
        return node;
    }

    if (current_token.type == TOKEN_TYPE_INTEGER) {
        Node *node = new_node_num(current_token.int_value);
        node->ty = type_i64; // Default integer type
        consume();
        return node;
    }
    
    if (current_token.type == TOKEN_TYPE_FLOAT) {
        Node *node = new_node(NODE_KIND_NUM);
        node->fval = current_token.float_value;
        node->ty = type_f64; // Default float type
        consume();
        return node; // We rely on codegen/semantic to check ty->kind
    }

    if (current_token.type == TOKEN_TYPE_CHAR) {
        Node *node = new_node_num(current_token.int_value);
        node->ty = type_char; // Char type
        consume();
        return node;
    }

    if (current_token.type == TOKEN_TYPE_STRING) {
        int id = register_string(strdup(current_token.text));
        Node *node = new_node(NODE_KIND_STRING_LITERAL);
        node->val = id;
        consume();
        return node;
    }

    if (current_token.type == TOKEN_TYPE_LPAREN) {
    
        consume();

        if (current_token.type == TOKEN_TYPE_IDENTIFIER) {
            Token type_tok = current_token;
            Type *maybe_type = NULL;

            if (strcmp(type_tok.text, "i8") == 0) maybe_type = type_i8;
            else if (strcmp(type_tok.text, "i16") == 0) maybe_type = type_i16;
            else if (strcmp(type_tok.text, "i32") == 0) maybe_type = type_i32;
            else if (strcmp(type_tok.text, "i64") == 0) maybe_type = type_i64;
            else if (strcmp(type_tok.text, "u8") == 0) maybe_type = type_u8;
            else if (strcmp(type_tok.text, "u16") == 0) maybe_type = type_u16;
            else if (strcmp(type_tok.text, "u32") == 0) maybe_type = type_u32;
            else if (strcmp(type_tok.text, "u64") == 0) maybe_type = type_u64;

            if (maybe_type) {
                consume();
                if (current_token.type == TOKEN_TYPE_RPAREN) {
                    consume();
                    Node *expr = parse_primary();
                    Node *cast_node = new_node(NODE_KIND_CAST);
                    cast_node->expr = expr;
                    cast_node->ty = maybe_type;
                    return cast_node;
                }
            }
        }

        Node *expr = parse_expr();
        expect(TOKEN_TYPE_RPAREN);
        return expr;
    }

    if (current_token.type == TOKEN_TYPE_IDENTIFIER) {
        // Generic function call: name gen type1, type2 (args...)
        if (lookahead_token.type == TOKEN_TYPE_GEN) {
            char *fn_name = strdup(current_token.text);
            consume(); // consume identifier
            consume(); // consume 'gen'

            // Parse generic type arguments
            int cap = 4;
            Type **gen_args = calloc(cap, sizeof(Type*));
            int gen_count = 0;

            // Build mangled name
            char mangled[512];
            snprintf(mangled, sizeof(mangled), "%s", fn_name);

            while (1) {
                Type *arg_type = parse_type();
                if (gen_count >= cap) {
                    cap *= 2;
                    gen_args = realloc(gen_args, cap * sizeof(Type*));
                }
                gen_args[gen_count++] = arg_type;

                const char *tname = "unknown";
                switch (arg_type->kind) {
                    case TY_I8: tname = "i8"; break;
                    case TY_I16: tname = "i16"; break;
                    case TY_I32: tname = "i32"; break;
                    case TY_I64: tname = "i64"; break;
                    case TY_U8: tname = "u8"; break;
                    case TY_U16: tname = "u16"; break;
                    case TY_U32: tname = "u32"; break;
                    case TY_U64: tname = "u64"; break;
                    case TY_F32: tname = "f32"; break;
                    case TY_F64: tname = "f64"; break;
                    case TY_BOOL: tname = "bool"; break;
                    case TY_CHAR: tname = "char"; break;
                    case TY_STRING: tname = "string"; break;
                    case TY_VOID: tname = "void"; break;
                    case TY_PTR: tname = "ptr"; break;
                    case TY_STRUCT: tname = arg_type->name ? arg_type->name : "struct"; break;
                    default: break;
                }
                size_t cur_len = strlen(mangled);
                snprintf(mangled + cur_len, sizeof(mangled) - cur_len, "_%s", tname);

                if (current_token.type == TOKEN_TYPE_COMMA) {
                    consume();
                } else {
                    break;
                }
            }

            // If LPAREN follows, this is a generic function call
            // Otherwise, return just the identifier (e.g., for enum constructors like Option gen i32 .Some(42))
            Node *id_node = new_node(NODE_KIND_IDENTIFIER);
            id_node->name = strdup(mangled);
            id_node->generic_args = gen_args;
            id_node->generic_arg_count = gen_count;
            id_node->member_name = fn_name;

            if (current_token.type == TOKEN_TYPE_LPAREN) {
                Node *node = new_node(NODE_KIND_FNCALL);
                node->lhs = id_node;
                node->generic_args = gen_args;
                node->generic_arg_count = gen_count;
                node->member_name = fn_name;

                expect(TOKEN_TYPE_LPAREN);

                if (current_token.type != TOKEN_TYPE_RPAREN) {
                    Node head = {};
                    Node *cur = &head;
                    while (1) {
                        cur->next = parse_expr();
                        cur = cur->next;
                        if (current_token.type == TOKEN_TYPE_COMMA) {
                            consume();
                        } else {
                            break;
                        }
                    }
                    node->args = head.next;
                }

                expect(TOKEN_TYPE_RPAREN);
                return node;
            }

            // Not a function call - return identifier, let parse_postfix handle the rest
            return id_node;
        }
        if (lookahead_token.type == TOKEN_TYPE_LPAREN) {
            Node *id_node = new_node(NODE_KIND_IDENTIFIER);
            id_node->name = strdup(current_token.text);

            Node *node = new_node(NODE_KIND_FNCALL);
            node->lhs = id_node;

            consume();
            expect(TOKEN_TYPE_LPAREN);

            // Parse arguments
            if (current_token.type != TOKEN_TYPE_RPAREN) {
                Node head = {};
                Node *cur = &head;
                while (1) {
                    cur->next = parse_expr();
                    cur = cur->next;
                    if (current_token.type == TOKEN_TYPE_COMMA) {
                        consume();
                    } else {
                        break;
                    }
                }
                node->args = head.next;
            }

            expect(TOKEN_TYPE_RPAREN);
            return node;
        }
        if (strcmp(current_token.text, "true") == 0) {
            Node *node = new_node_num(1);
            node->ty = type_bool;
            consume();
            return node;
        }
        if (strcmp(current_token.text, "false") == 0) {
            Node *node = new_node_num(0);
            node->ty = type_bool;
            consume();
            return node;
        }

        Node *node = new_node(NODE_KIND_IDENTIFIER);
        node->name = strdup(current_token.text);
        consume();
        return node;
    }

    if (current_token.type == TOKEN_TYPE_SYSCALL) {
        consume();
        expect(TOKEN_TYPE_LPAREN);
        
        Node *node = new_node(NODE_KIND_SYSCALL);
        node->ty = type_i64; // Syscalls return i64 (rax)

        if (current_token.type == TOKEN_TYPE_RPAREN) {
             error_at_token(current_token, "syscall requer pelo menos o ID.");
        }

        Node head = {};
        Node *cur = &head;
        
        while (current_token.type != TOKEN_TYPE_RPAREN) {
            cur->next = parse_expr();
            cur = cur->next;
            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
        node->args = head.next;
        
        expect(TOKEN_TYPE_RPAREN);
        return node;
    }

    error_at_token(current_token, "era esperado um numero ou identificador, mas encontrou '%s'", current_token.text);
}

static Node *parse_block() {
    Node *node = new_node(NODE_KIND_BLOCK);
    expect(TOKEN_TYPE_LBRACE);

    Node head = {};
    Node *cur = &head;

    while (current_token.type != TOKEN_TYPE_RBRACE) {
        cur->next = parse_stmt();
        cur = cur->next;
    }
    expect(TOKEN_TYPE_RBRACE);

    node->stmts = head.next;
    return node;
}

static Node *parse_extern_fn() {
    consume(); // extern
    expect(TOKEN_TYPE_FN);

    Node *fn_node = new_node(NODE_KIND_FN);
    fn_node->is_extern = 1;

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado nome da funcao apos 'extern fn'.");
    }
    fn_node->name = strdup(current_token.text);
    consume();

    if (current_token.type == TOKEN_TYPE_GEN) {
        error_at_token(current_token, "funcoes extern nao podem ser genericas.");
    }

    expect(TOKEN_TYPE_LPAREN);

    if (current_token.type != TOKEN_TYPE_RPAREN) {
        in_fn_params = 1;
        Node head = {};
        Node *cur = &head;
        while (1) {
            if (current_token.type == TOKEN_TYPE_ELLIPSIS) {
                fn_node->is_variadic = 1;
                consume();
                if (current_token.type != TOKEN_TYPE_RPAREN) {
                    error_at_token(current_token, "variadic '...' deve ser o ultimo parametro.");
                }
                break;
            }

            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                error_at_token(current_token, "esperado nome do parametro.");
            }
            char *param_name = strdup(current_token.text);
            consume();

            expect(TOKEN_TYPE_AS);
            Type *param_type = parse_type();

            Node *param = new_node(NODE_KIND_LET);
            param->name = param_name;
            param->ty = param_type;
            param->flags = VAR_FLAG_NONE;

            cur->next = param;
            cur = cur->next;

            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
        fn_node->params = head.next;
        in_fn_params = 0;
    }

    expect(TOKEN_TYPE_RPAREN);

    if (current_token.type == TOKEN_TYPE_AS) {
        consume();
        fn_node->return_type = parse_type();
    } else {
        fn_node->return_type = type_i32;
    }

    fn_node->body = NULL;
    expect(TOKEN_TYPE_SEMICOLON);
    return fn_node;
}

static Node *parse_fn() {
    expect(TOKEN_TYPE_FN);

    Node *fn_node = new_node(NODE_KIND_FN);

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado nome da função.");
    }
    fn_node->name = strdup(current_token.text);
    consume();

    // Parse generic params: fn max gen T, U (...)
    if (current_token.type == TOKEN_TYPE_GEN) {
        consume();
        int cap = 4;
        fn_node->generic_params = calloc(cap, sizeof(char*));
        fn_node->generic_param_count = 0;
        while (current_token.type == TOKEN_TYPE_IDENTIFIER) {
            if (fn_node->generic_param_count >= cap) {
                cap *= 2;
                fn_node->generic_params = realloc(fn_node->generic_params, cap * sizeof(char*));
            }
            fn_node->generic_params[fn_node->generic_param_count++] = strdup(current_token.text);
            consume();
            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
    }

    expect(TOKEN_TYPE_LPAREN);

    // Parse parameters
    if (current_token.type != TOKEN_TYPE_RPAREN) {
        in_fn_params = 1;
        Node head = {};
        Node *cur = &head;
        while (1) {
            if (current_token.type == TOKEN_TYPE_ELLIPSIS) {
                fn_node->is_variadic = 1;
                consume();
                if (current_token.type != TOKEN_TYPE_RPAREN) {
                    error_at_token(current_token, "variadic '...' deve ser o ultimo parametro.");
                }
                break;
            }

            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                error_at_token(current_token, "esperado nome do parâmetro.");
            }
            char *param_name = strdup(current_token.text);
            consume();

            expect(TOKEN_TYPE_AS);
            Type *param_type = parse_type();

            Node *param = new_node(NODE_KIND_LET); // Reusing LET node for param declaration
            param->name = param_name;
            param->ty = param_type;
            param->flags = VAR_FLAG_NONE; // Parameters are immutable by default for now?

            cur->next = param;
            cur = cur->next;

            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
        fn_node->params = head.next;
        in_fn_params = 0;
    }

    expect(TOKEN_TYPE_RPAREN);

    if (current_token.type == TOKEN_TYPE_AS) {
        consume();
        fn_node->return_type = parse_type();
    } else {
        fn_node->return_type = type_i32; 
    }
    fn_node->body = parse_block();
    return fn_node;
}

static Node *parse_postfix() {
    Node *node = parse_primary();

    while (1) {
        if (current_token.type == TOKEN_TYPE_LBRACKET) {
            consume(); // [
            Node *index = parse_expr();
            expect(TOKEN_TYPE_RBRACKET); // ]
            
            Node *idx_node = new_node(NODE_KIND_INDEX);
            idx_node->lhs = node; // Array/Pointer
            idx_node->rhs = index; // Index
            node = idx_node;
        } else if (current_token.type == TOKEN_TYPE_DOT) {
            consume(); // .
            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                error_at_token(current_token, "esperado nome do membro após '.'");
            }
            // Check for module-qualified generic: t.Result gen i32, i32
            if (node->kind == NODE_KIND_IDENTIFIER && lookahead_token.type == TOKEN_TYPE_GEN) {
                char combined[512];
                snprintf(combined, sizeof(combined), "%s.%s", node->name, current_token.text);
                char *original_name = strdup(combined);
                consume(); // consume type/fn name
                consume(); // consume 'gen'

                int cap = 4;
                Type **gen_args = calloc(cap, sizeof(Type*));
                int gen_count = 0;
                char mangled[512];
                snprintf(mangled, sizeof(mangled), "%s", combined);

                while (1) {
                    Type *arg_type = parse_type();
                    if (gen_count >= cap) {
                        cap *= 2;
                        gen_args = realloc(gen_args, cap * sizeof(Type*));
                    }
                    gen_args[gen_count++] = arg_type;

                    const char *tname = "unknown";
                    switch (arg_type->kind) {
                        case TY_I8: tname = "i8"; break;
                        case TY_I16: tname = "i16"; break;
                        case TY_I32: tname = "i32"; break;
                        case TY_I64: tname = "i64"; break;
                        case TY_U8: tname = "u8"; break;
                        case TY_U16: tname = "u16"; break;
                        case TY_U32: tname = "u32"; break;
                        case TY_U64: tname = "u64"; break;
                        case TY_F32: tname = "f32"; break;
                        case TY_F64: tname = "f64"; break;
                        case TY_BOOL: tname = "bool"; break;
                        case TY_CHAR: tname = "char"; break;
                        case TY_STRING: tname = "string"; break;
                        case TY_VOID: tname = "void"; break;
                        case TY_PTR: tname = "ptr"; break;
                        case TY_STRUCT: tname = arg_type->name ? arg_type->name : "struct"; break;
                        default: break;
                    }
                    size_t cur_len = strlen(mangled);
                    snprintf(mangled + cur_len, sizeof(mangled) - cur_len, "_%s", tname);

                    if (current_token.type == TOKEN_TYPE_COMMA) {
                        consume();
                    } else {
                        break;
                    }
                }

                free(node->name);
                node->name = strdup(mangled);
                node->generic_args = gen_args;
                node->generic_arg_count = gen_count;
                node->member_name = original_name;
            } else {
                Node *member_node = new_node(NODE_KIND_MEMBER_ACCESS);
                member_node->lhs = node;
                member_node->member_name = strdup(current_token.text);
                consume();
                node = member_node;
            }
        } else if (current_token.type == TOKEN_TYPE_LPAREN) {
            // Generic Function Call (expr(...))
            consume(); // (
            
            Node *call_node = new_node(NODE_KIND_FNCALL);
            call_node->lhs = node; // The function being called (identifier, member access, etc)
            
            // Parse arguments
            if (current_token.type != TOKEN_TYPE_RPAREN) {
                Node head = {};
                Node *cur = &head;
                while (1) {
                    cur->next = parse_expr();
                    cur = cur->next;
                    if (current_token.type == TOKEN_TYPE_COMMA) {
                        consume();
                    } else {
                        break;
                    }
                }
                call_node->args = head.next;
            }
            expect(TOKEN_TYPE_RPAREN);
            node = call_node;
        } else {
            break;
        }
    }
    return node;
}

static Node *parse_unary() {
    if (current_token.type == TOKEN_TYPE_AND) {
        consume();
        Node *node = new_node(NODE_KIND_ADDR);
        node->expr = parse_unary();
        return node;
    }
    if (current_token.type == TOKEN_TYPE_MULTIPLIER) {
        consume();
        Node *node = new_node(NODE_KIND_DEREF);
        node->expr = parse_unary();
        return node;
    }
    if (current_token.type == TOKEN_TYPE_MINUS) {
        consume();
        Node *node = new_node(NODE_KIND_NEG);
        node->expr = parse_unary();
        return node;
    }
    if (current_token.type == TOKEN_TYPE_NOT) {
        consume();
        Node *node = new_node(NODE_KIND_LOGICAL_NOT);
        node->expr = parse_unary();
        return node;
    }
    if (current_token.type == TOKEN_TYPE_BITNOT) {
        consume();
        Node *node = new_node(NODE_KIND_BITWISE_NOT);
        node->expr = parse_unary();
        return node;
    }
    return parse_postfix();
}

static Node *parse_mul() {
    Node *node = parse_unary();
    while (current_token.type == TOKEN_TYPE_MULTIPLIER || current_token.type == TOKEN_TYPE_DIVIDER || current_token.type == TOKEN_TYPE_MOD) {
        if (current_token.type == TOKEN_TYPE_MULTIPLIER) {
            consume();
            node = new_binary_node(NODE_KIND_MULTIPLIER, node, parse_unary());
        } else if (current_token.type == TOKEN_TYPE_DIVIDER) {
            consume();
            node = new_binary_node(NODE_KIND_DIVIDER, node, parse_unary());
        } else if (current_token.type == TOKEN_TYPE_MOD) {
            consume();
            node = new_binary_node(NODE_KIND_MOD, node, parse_unary());
        }
    }
    return node;
}

static Node *parse_add() {
    Node *node = parse_mul();
    while (current_token.type == TOKEN_TYPE_PLUS || current_token.type == TOKEN_TYPE_MINUS) {
        if (current_token.type == TOKEN_TYPE_PLUS) {
            consume();
            node = new_binary_node(NODE_KIND_ADD, node, parse_mul());
        } else if (current_token.type == TOKEN_TYPE_MINUS) {
            consume();
            node = new_binary_node(NODE_KIND_SUBTRACTION, node, parse_mul());
        }
    }
    return node;
}

static Node *parse_bitwise_and() {
    Node *node = parse_comparison();
    while (current_token.type == TOKEN_TYPE_AND) {
        consume();
        node = new_binary_node(NODE_KIND_BITWISE_AND, node, parse_comparison());
    }
    return node;
}

static Node *parse_bitwise_xor() {
    Node *node = parse_bitwise_and();
    while (current_token.type == TOKEN_TYPE_XOR) {
        consume();
        node = new_binary_node(NODE_KIND_BITWISE_XOR, node, parse_bitwise_and());
    }
    return node;
}

static Node *parse_bitwise_or() {
    Node *node = parse_bitwise_xor();
    while (current_token.type == TOKEN_TYPE_OR) {
        consume();
        node = new_binary_node(NODE_KIND_BITWISE_OR, node, parse_bitwise_xor());
    }
    return node;
}

static Node *parse_logical_and() {
    Node *node = parse_bitwise_or();
    while (current_token.type == TOKEN_TYPE_AND_AND) {
        consume();
        node = new_binary_node(NODE_KIND_LOGICAL_AND, node, parse_bitwise_or());
    }
    return node;
}

static Node *parse_logical_or() {
    Node *node = parse_logical_and();
    while (current_token.type == TOKEN_TYPE_OR_OR) {
        consume();
        node = new_binary_node(NODE_KIND_LOGICAL_OR, node, parse_logical_and());
    }
    return node;
}

static Node *parse_shift() {
    Node *node = parse_add();
    while (current_token.type == TOKEN_TYPE_SHL || current_token.type == TOKEN_TYPE_SHR) {
        if (current_token.type == TOKEN_TYPE_SHL) {
            consume();
            node = new_binary_node(NODE_KIND_SHL, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_SHR) {
            consume();
            node = new_binary_node(NODE_KIND_SHR, node, parse_add());
        }
    }
    return node;
}

static Node *parse_relational() {
    Node *node = parse_shift();
    while (1) {
        if (current_token.type == TOKEN_TYPE_LT) {
            consume();
            node = new_binary_node(NODE_KIND_LT, node, parse_shift());
        } else if (current_token.type == TOKEN_TYPE_LE) {
            consume();
            node = new_binary_node(NODE_KIND_LE, node, parse_shift());
        } else if (current_token.type == TOKEN_TYPE_GT) {
            consume();
            node = new_binary_node(NODE_KIND_GT, node, parse_shift());
        } else if (current_token.type == TOKEN_TYPE_GE) {
            consume();
            node = new_binary_node(NODE_KIND_GE, node, parse_shift());
        } else {
            break;
        }
    }
    return node;
}

static Node *parse_comparison() {
    Node *node = parse_relational();
    while (1) {
        if (current_token.type == TOKEN_TYPE_EQEQ) {
            consume();
            node = new_binary_node(NODE_KIND_EQ, node, parse_relational());
        } else if (current_token.type == TOKEN_TYPE_NE) {
            consume();
            node = new_binary_node(NODE_KIND_NE, node, parse_relational());
        } else {
            break;
        }
    }
    return node;
}

static Node *parse_if_stmt() {
    expect(TOKEN_TYPE_IF);
    Node *if_node = new_node(NODE_KIND_IF);
    expect(TOKEN_TYPE_LPAREN);
    if_node->if_stmt.cond = parse_expr();
    expect(TOKEN_TYPE_RPAREN);
    if_node->if_stmt.then_b = parse_block();
    if (current_token.type == TOKEN_TYPE_ELSE) {
        consume();
        if (current_token.type == TOKEN_TYPE_IF) {
            if_node->if_stmt.else_b = parse_if_stmt();
        } else {
            if_node->if_stmt.else_b = parse_block();
        }
    } else {
        if_node->if_stmt.else_b = NULL;
    }
    return if_node;
}

// Em parser.c, adicione esta função no final

static Node *parse_while_stmt() {
    expect(TOKEN_TYPE_WHILE);
    Node *node = new_node(NODE_KIND_WHILE);

    expect(TOKEN_TYPE_LPAREN);
    node->while_stmt.cond = parse_expr();
    expect(TOKEN_TYPE_RPAREN);

    node->while_stmt.body = parse_block();

    return node;
}

static Node *parse_struct_decl() {
    expect(TOKEN_TYPE_STRUCT);
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado nome da struct.");
    }
    char *struct_name = strdup(current_token.text);
    consume();

    Type *struct_type = new_type(TY_STRUCT);
    struct_type->name = struct_name;

    // Parse generic params: struct Vec gen T, U { ... }
    if (current_token.type == TOKEN_TYPE_GEN) {
        consume();
        int cap = 4;
        struct_type->generic_params = calloc(cap, sizeof(char*));
        struct_type->generic_param_count = 0;
        while (current_token.type == TOKEN_TYPE_IDENTIFIER) {
            if (struct_type->generic_param_count >= cap) {
                cap *= 2;
                struct_type->generic_params = realloc(struct_type->generic_params, cap * sizeof(char*));
            }
            struct_type->generic_params[struct_type->generic_param_count++] = strdup(current_token.text);
            consume();
            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
    }

    expect(TOKEN_TYPE_LBRACE);

    Field head = {};
    Field *cur = &head;

    while (current_token.type != TOKEN_TYPE_RBRACE) {
        if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
             error_at_token(current_token, "esperado nome do campo.");
        }
        char *field_name = strdup(current_token.text);
        consume();

        expect(TOKEN_TYPE_AS);
        Type *field_type = parse_type();
        expect(TOKEN_TYPE_SEMICOLON);

        Field *f = calloc(1, sizeof(Field));
        f->name = field_name;
        f->type = field_type;
        cur->next = f;
        cur = f;
    }
    expect(TOKEN_TYPE_RBRACE);
    struct_type->fields = head.next;

    // We return a dummy node to keep the parser loop happy, 
    // but the important part is that the type is created and (conceptually) registered.
    // Actually, we need to register this type so it can be looked up later.
    // Since we don't have a global type table yet (other than all_types list),
    // we rely on the fact that we created it.
    // BUT, when parsing types later, we create NEW TY_STRUCT types with just the name.
    // We will need to resolve them in semantic analysis.
    
    // Let's return a NO-OP node or a special STRUCT_DECL node if we had one.
    // For now, let's reuse EXPR_STMT with a null expr or something, or add NODE_KIND_STRUCT_DECL.
    // Or better, since we are in top-level, we can just return NULL and handle it in the loop?
    // No, the loop expects a node.
    // Let's add NODE_KIND_STRUCT_DECL to parser.h? No, let's avoid changing header again if possible.
    // Let's use a dummy node.
    Node *node = new_node(NODE_KIND_EXPR_STMT); // Dummy
    node->ty = struct_type; // Store the type here so we can register it in semantic analysis
    // Actually, let's use a new node kind to be clean, or just use a hack.
    // Hack: Use NODE_KIND_NUM with a special flag? No.
    // Let's just add NODE_KIND_STRUCT_DECL. It's safer.
    // Wait, I can't edit parser.h in this tool call.
    // I will use NODE_KIND_BLOCK with no statements as a placeholder, but attach the type.
    // Actually, I can just return a node that carries the type info.
    // Let's use NODE_KIND_LET but with a special flag?
    // Let's just use NODE_KIND_EXPR_STMT with NULL expr, but set the 'ty' field to the struct type.
    // Semantic analysis will see this and register the struct.
    node->kind = NODE_KIND_BLOCK;
    node->stmts = NULL;
    node->ty = struct_type; // Pass the type info
    
    return node;
}

static Node *parse_enum_decl() {
    expect(TOKEN_TYPE_ENUM);
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado nome do enum.");
    }
    char *enum_name = strdup(current_token.text);
    consume();

    Type *enum_type = new_type(TY_ENUM);
    enum_type->name = enum_name;

    // Parse generic params: enum Option gen T { ... }
    if (current_token.type == TOKEN_TYPE_GEN) {
        consume();
        int cap = 4;
        enum_type->generic_params = calloc(cap, sizeof(char*));
        enum_type->generic_param_count = 0;
        while (current_token.type == TOKEN_TYPE_IDENTIFIER) {
            if (enum_type->generic_param_count >= cap) {
                cap *= 2;
                enum_type->generic_params = realloc(enum_type->generic_params, cap * sizeof(char*));
            }
            enum_type->generic_params[enum_type->generic_param_count++] = strdup(current_token.text);
            consume();
            if (current_token.type == TOKEN_TYPE_COMMA) {
                consume();
            } else {
                break;
            }
        }
    }

    expect(TOKEN_TYPE_LBRACE);

    Field head = {};
    Field *cur = &head;
    int discriminant = 0;

    while (current_token.type != TOKEN_TYPE_RBRACE) {
        if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
            error_at_token(current_token, "esperado nome da variante do enum.");
        }
        char *variant_name = strdup(current_token.text);
        consume();

        Field *f = calloc(1, sizeof(Field));
        f->name = variant_name;
        f->offset = discriminant++;

        // Check for payload types: Variant as Type or Variant as Type, Type
        if (current_token.type == TOKEN_TYPE_AS) {
            consume();
            // Parse first type
            Type *first_type = parse_type();

            if (current_token.type == TOKEN_TYPE_COMMA) {
                // Multi-field payload: create anonymous struct
                int field_cap = 4;
                Type **payload_types = calloc(field_cap, sizeof(Type*));
                payload_types[0] = first_type;
                int payload_count = 1;

                while (current_token.type == TOKEN_TYPE_COMMA) {
                    consume();
                    if (payload_count >= field_cap) {
                        field_cap *= 2;
                        payload_types = realloc(payload_types, field_cap * sizeof(Type*));
                    }
                    payload_types[payload_count++] = parse_type();
                }

                // Create anonymous struct for multi-field payload
                Type *payload_struct = new_type(TY_STRUCT);
                char anon_name[256];
                snprintf(anon_name, sizeof(anon_name), "__enum_%s_%s_payload", enum_name, variant_name);
                payload_struct->name = strdup(anon_name);
                payload_struct->owns_fields = 1;

                Field *phead = NULL, *ptail = NULL;
                int poffset = 0;
                for (int i = 0; i < payload_count; i++) {
                    Field *pf = calloc(1, sizeof(Field));
                    char fname[16];
                    snprintf(fname, sizeof(fname), "_%d", i);
                    pf->name = strdup(fname);
                    pf->type = payload_types[i];
                    pf->offset = poffset;
                    poffset += payload_types[i]->size;
                    if (!phead) { phead = pf; ptail = pf; }
                    else { ptail->next = pf; ptail = pf; }
                }
                payload_struct->fields = phead;
                payload_struct->size = poffset;
                f->type = payload_struct;
                free(payload_types);
            } else {
                // Single type payload
                f->type = first_type;
            }
        } else {
            f->type = NULL; // No payload
        }

        expect(TOKEN_TYPE_SEMICOLON);
        cur->next = f;
        cur = f;
    }
    expect(TOKEN_TYPE_RBRACE);

    enum_type->fields = head.next;
    enum_type->variant_count = discriminant;

    Node *node = new_node(NODE_KIND_BLOCK);
    node->stmts = NULL;
    node->ty = enum_type;
    return node;
}

static Node *parse_match_stmt() {
    Token match_tok = current_token;
    expect(TOKEN_TYPE_MATCH);

    // Parse the type name: match TypeName (expr) { ... }
    Type *match_type = parse_type();

    expect(TOKEN_TYPE_LPAREN);
    Node *match_expr = parse_expr();
    expect(TOKEN_TYPE_RPAREN);

    expect(TOKEN_TYPE_LBRACE);

    Node *match_node = new_node(NODE_KIND_MATCH);
    match_node->tok = calloc(1, sizeof(Token));
    *match_node->tok = match_tok;
    match_node->match_stmt.expr = match_expr;
    match_node->match_stmt.match_type = match_type;

    // Parse cases
    Node case_head = {};
    Node *case_cur = &case_head;

    while (current_token.type != TOKEN_TYPE_RBRACE) {
        if (current_token.type == TOKEN_TYPE_ELSE) {
            consume();
            match_node->match_stmt.else_b = parse_block();
            break;
        }

        expect(TOKEN_TYPE_CASE);

        if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
            error_at_token(current_token, "esperado nome da variante apos 'case'.");
        }

        Node *case_node = new_node(NODE_KIND_BLOCK);
        case_node->name = strdup(current_token.text);
        consume();

        // Parse destructuring bindings: case Variant(x, y) { ... }
        if (current_token.type == TOKEN_TYPE_LPAREN) {
            consume();
            Node param_head = {};
            Node *param_cur = &param_head;
            while (current_token.type != TOKEN_TYPE_RPAREN) {
                if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                    error_at_token(current_token, "esperado nome de binding no case.");
                }
                Node *binding = new_node(NODE_KIND_LET);
                binding->name = strdup(current_token.text);
                consume();
                param_cur->next = binding;
                param_cur = binding;
                if (current_token.type == TOKEN_TYPE_COMMA) {
                    consume();
                } else {
                    break;
                }
            }
            expect(TOKEN_TYPE_RPAREN);
            case_node->params = param_head.next;
        }

        case_node->body = parse_block();
        case_cur->next = case_node;
        case_cur = case_node;
    }

    expect(TOKEN_TYPE_RBRACE);
    match_node->match_stmt.cases = case_head.next;
    return match_node;
}

static Node *parse_asm_stmt() {
    consume();
    expect(TOKEN_TYPE_LPAREN);

    if (current_token.type != TOKEN_TYPE_STRING) {
        error_at_token(current_token, "esperado string apos 'asm('");
    }

    Node *asm_node = new_node(NODE_KIND_ASM);
    asm_node->name = strdup(current_token.text);
    consume();

    expect(TOKEN_TYPE_RPAREN);
    expect(TOKEN_TYPE_SEMICOLON);

    return asm_node;
}

static Node *parse_for_stmt() {
    expect(TOKEN_TYPE_FOR);
    expect(TOKEN_TYPE_LPAREN);

    Node *node = new_node(NODE_KIND_FOR);
    
    // Init (can be let or expr or empty)
    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
        if (current_token.type == TOKEN_TYPE_LET) {
            node->for_stmt.init = parse_var_decl();
        } else {
            node->for_stmt.init = parse_expr();
            // If it's an expression, we might need to consume a semicolon if it's not part of the expression?
            // parse_expr parses until it can't.
            // But wait, parse_var_decl consumes the semicolon?
            // parse_var_decl calls consume() at the end?
            // Let's check parse_var_decl.
            // It ends with consume() after init expr.
        }
    }
    // parse_var_decl consumes the semicolon?
    // parse_var_decl: ... consume(); return node;
    // It consumes the semicolon at the end of let statement?
    // Let's check parse_var_decl again.
    // It expects semicolon at the end.
    
    // If init was expression, we need to consume semicolon.
    if (node->for_stmt.init && node->for_stmt.init->kind != NODE_KIND_LET) {
        expect(TOKEN_TYPE_SEMICOLON);
    } else if (!node->for_stmt.init) {
        expect(TOKEN_TYPE_SEMICOLON);
    }
    // If it was LET, parse_var_decl consumed semicolon.

    // Cond
    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
        node->for_stmt.cond = parse_expr();
    }
    expect(TOKEN_TYPE_SEMICOLON);

    // Step
    if (current_token.type != TOKEN_TYPE_RPAREN) {
        node->for_stmt.step = parse_expr();
    }
    expect(TOKEN_TYPE_RPAREN);

    node->for_stmt.body = parse_block();

    return node;
}

static Node *parse_do_while_stmt() {
    expect(TOKEN_TYPE_DO);
    Node *node = new_node(NODE_KIND_DO_WHILE);
    node->do_while_stmt.body = parse_block();
    expect(TOKEN_TYPE_WHILE);
    expect(TOKEN_TYPE_LPAREN);
    node->do_while_stmt.cond = parse_expr();
    expect(TOKEN_TYPE_RPAREN);
    expect(TOKEN_TYPE_SEMICOLON);
    return node;
}

static Node *parse_stmt() {
    if (current_token.type == TOKEN_TYPE_SEMICOLON) {
        consume();
        // Return a no-op node or just continue?
        // Since parse_stmt returns a Node*, we should return a Block with no stmts or similar.
        // Or just NULL? But callers might expect a node.
        // Let's return an empty block.
        Node *node = new_node(NODE_KIND_BLOCK);
        node->stmts = NULL;
        return node;
    }

    if (current_token.type == TOKEN_TYPE_LET) {
        return parse_var_decl();
    }

    if (current_token.type == TOKEN_TYPE_FOR) {
        return parse_for_stmt();
    }

    if (current_token.type == TOKEN_TYPE_DO) {
        return parse_do_while_stmt();
    }

    if (current_token.type == TOKEN_TYPE_IF) {
        return parse_if_stmt();
    }

    if (current_token.type == TOKEN_TYPE_RETURN) {
        consume();
        Node *expr_node = NULL;
        if (current_token.type != TOKEN_TYPE_SEMICOLON) {
            expr_node = parse_expr();
        }

        if (current_token.type != TOKEN_TYPE_SEMICOLON) {
            error_at_token(current_token, "era esperado ';' depois da expressao.");
        }
        consume();

        return new_node_return(expr_node);
    }

    if (current_token.type == TOKEN_TYPE_LBRACE) {
        return parse_block();
    }

    if (current_token.type == TOKEN_TYPE_ASM) {
        return parse_asm_stmt();
    }

    if (current_token.type == TOKEN_TYPE_WHILE) {
        return parse_while_stmt();
    }

    if (current_token.type == TOKEN_TYPE_MATCH) {
        return parse_match_stmt();
    }

    if (current_token.type == TOKEN_TYPE_BREAK) {
        consume();
        expect(TOKEN_TYPE_SEMICOLON);
        return new_node(NODE_KIND_BREAK);
    }

    if (current_token.type == TOKEN_TYPE_CONTINUE) {
        consume();
        expect(TOKEN_TYPE_SEMICOLON);
        return new_node(NODE_KIND_CONTINUE);
    }

    if (current_token.type == TOKEN_TYPE_DEFER) {
        Token defer_tok = current_token;
        consume();
        Node *node = new_node(NODE_KIND_DEFER);
        node->tok = calloc(1, sizeof(Token));
        *node->tok = defer_tok;
        node->expr = parse_expr();
        expect(TOKEN_TYPE_SEMICOLON);
        return node;
    }
    Node *node = parse_expr();
    // Assignment is now handled in parse_expr

    expect(TOKEN_TYPE_SEMICOLON);
    Node *expr_stmt_node = new_node(NODE_KIND_EXPR_STMT);
    expr_stmt_node->expr = node;
    return expr_stmt_node;

    error_at_token(current_token, "era esperado 'let', 'return' ou bloco, mas encontrou '%s'", current_token.text);
}

static void ast_print_recursive(Node *node, int depth) {
    if (!node) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    switch (node->kind) {
        case NODE_KIND_FN:
            printf("Function(%s%s)\n", node->name, node->is_extern ? " [extern]" : "");
            if (node->body) ast_print_recursive(node->body, depth + 1);
            break;
        case NODE_KIND_BLOCK:
            printf("Block\n");
            for (Node *stmt = node->stmts; stmt; stmt = stmt->next) {
                ast_print_recursive(stmt, depth + 1);
            }
            break;
        case NODE_KIND_RETURN:
            printf("ReturnStmt");
            if (node->ty) {
                printf(" [tipo: ");
                switch (node->ty->kind) {
                    case TY_INT: printf("int"); break;
                    default: printf("desconhecido"); break;
                }
                printf("]");
            }
            printf("\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_KIND_LET:
            printf("LetStmt(name='%s', offset=%d", node->name ? node->name : "NULL", node->offset);
            if (node->flags & VAR_FLAG_MUT) {
                printf(", mut");
            }
            if (node->flags & VAR_FLAG_IMUT) {
                printf(", imut");
            }
            if (node->ty) {
                printf(", tipo=");
                switch (node->ty->kind) {
                    case TY_I32: printf("i32"); break;
                    case TY_I64: printf("i64"); break;
                    case TY_ARRAY: 
                        printf("Array(len=%d, size=%d)", node->ty->array_len, node->ty->size); 
                        break;
                    case TY_PTR: 
                        printf("Pointer(size=%d)", node->ty->size); 
                        break;
                    default: printf("?"); break;
                }
            }
            printf(")\n");
            if (node->init_expr) {
                for (int i = 0; i < depth + 1; i++) {
                    printf("  ");
                }
                printf("InitExpr:\n");
                ast_print_recursive(node->init_expr, depth + 2);
            }
            break;
        case NODE_KIND_IDENTIFIER:
            printf("Identifier(%s, offset=%d)\n", node->name, node->offset);
            break;
        case NODE_KIND_NUM:
            printf("NumberLiteral(%ld)", node->val);
            if (node->ty) {
                printf(" [tipo: ");
                switch (node->ty->kind) {
                    case TY_INT: printf("int"); break;
                    default: printf("desconhecido"); break;
                }
                printf("]");
            }
            printf("\n");
            break;
        case NODE_KIND_ADD:
            printf("BinaryOp(+)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_SUBTRACTION:
            printf("BinaryOp(-)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_MULTIPLIER:
            printf("BinaryOp(*)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_DIVIDER:
            printf("BinaryOp(/)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_MOD:
            printf("BinaryOp(%%)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_EQ:
            printf("BinaryOp(==)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_NE:
            printf("BinaryOp(!=)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_LT:
            printf("BinaryOp(<)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_LE:
            printf("BinaryOp(<=)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_GT:
            printf("BinaryOp(>)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_GE:
            printf("BinaryOp(>=)\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_IF:
            printf("IfStmt\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Cond:\n");
            ast_print_recursive(node->if_stmt.cond, depth + 2);
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Then:\n");
            ast_print_recursive(node->if_stmt.then_b, depth + 2);
            if (node->if_stmt.else_b) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Else:\n");
                ast_print_recursive(node->if_stmt.else_b, depth + 2);
            }
            break;
        case NODE_KIND_ASM:
            printf("AsmStmt(\"%s\")\n", node->name ? node->name : "");
            break;
        case NODE_KIND_WHILE:
            printf("WhileStmt\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Cond:\n");
            ast_print_recursive(node->while_stmt.cond, depth + 2);
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Body:\n");
            ast_print_recursive(node->while_stmt.body, depth + 2);
            break;
        case NODE_KIND_EXPR_STMT:
            printf("ExprStmt\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_KIND_ASSIGN:
            printf("AssignStmt\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("LHS:\n");
            ast_print_recursive(node->lhs, depth + 2);
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("RHS:\n");
            ast_print_recursive(node->rhs, depth + 2);
            break;
        case NODE_KIND_CAST:
            printf("CastExpr(");
            if (node->ty) {
                switch (node->ty->kind) {
                    case TY_I8: printf("i8"); break;
                    case TY_I16: printf("i16"); break;
                    case TY_I32: printf("i32"); break;
                    case TY_I64: printf("i64"); break;
                    case TY_U8: printf("u8"); break;
                    case TY_U16: printf("u16"); break;
                    case TY_U32: printf("u32"); break;
                    case TY_U64: printf("u64"); break;
                    default: printf("?"); break;
                }
            }
            printf(")\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_KIND_FNCALL:
            printf("FunctionCall(%s)\n", node->name);
            break;
        case NODE_KIND_ADDR:
            printf("AddrOf(&)\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_KIND_DEREF:
            printf("Deref(*)\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_KIND_INDEX:
            printf("Index([])\n");
            ast_print_recursive(node->lhs, depth + 1);
            ast_print_recursive(node->rhs, depth + 1);
            break;
        case NODE_KIND_ENUM_LITERAL:
            printf("EnumLiteral(disc=%d)\n", node->enum_literal.discriminant);
            break;
        case NODE_KIND_MATCH:
            printf("MatchStmt\n");
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Expr:\n");
            ast_print_recursive(node->match_stmt.expr, depth + 2);
            for (Node *c = node->match_stmt.cases; c; c = c->next) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Case(%s, disc=%ld):\n", c->name ? c->name : "?", c->val);
                ast_print_recursive(c->body, depth + 2);
            }
            if (node->match_stmt.else_b) {
                for (int i = 0; i < depth + 1; i++) printf("  ");
                printf("Else:\n");
                ast_print_recursive(node->match_stmt.else_b, depth + 2);
            }
            break;
        case NODE_KIND_DEFER:
            printf("Defer\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        default:
            printf("Nó Desconhecido\n");
            break;
    }
}

void ast_print(Node *node) {
    ast_print_recursive(node, 0);
}

// Simple visited set for free_ast (uses linear probing)
#define FREE_AST_SET_SIZE 65536
static Node *freed_nodes[FREE_AST_SET_SIZE];
static int freed_count = 0;

static int free_ast_visited(Node *node) {
    if (!node) return 1;
    unsigned long hash = ((unsigned long)node) % FREE_AST_SET_SIZE;
    for (int i = 0; i < FREE_AST_SET_SIZE; i++) {
        int idx = (hash + i) % FREE_AST_SET_SIZE;
        if (freed_nodes[idx] == node) return 1;  // Already freed
        if (freed_nodes[idx] == NULL) {
            freed_nodes[idx] = node;
            freed_count++;
            return 0;  // Not freed yet
        }
    }
    return 0;  // Set full, assume not freed (rare edge case)
}

static void free_ast_impl(Node *node) {
    if (!node) return;
    if (free_ast_visited(node)) return;  // Already freed

    // Free next pointer first (list traversal)
    free_ast_impl(node->next);

    if (node->name) free(node->name);
    if (node->member_name) free(node->member_name);
    if (node->generic_params) {
        for (int i = 0; i < node->generic_param_count; i++) {
            if (node->generic_params[i]) free(node->generic_params[i]);
        }
        free(node->generic_params);
    }
    if (node->generic_args) free(node->generic_args);

    // Free children based on node type
    switch (node->kind) {
        case NODE_KIND_FN:
            free_ast_impl(node->params);
            free_ast_impl(node->body);
            break;
        case NODE_KIND_BLOCK:
            free_ast_impl(node->stmts);
            break;
        case NODE_KIND_RETURN:
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_CAST:
        case NODE_KIND_ADDR:
        case NODE_KIND_DEREF:
        case NODE_KIND_DEFER:
            free_ast_impl(node->expr);
            break;
        case NODE_KIND_FNCALL:
        case NODE_KIND_SYSCALL:
            free_ast_impl(node->lhs); // Free the function expression (identifier, member access, etc)
            free_ast_impl(node->args);
            break;
        case NODE_KIND_LET:
            free_ast_impl(node->init_expr);
            break;
        case NODE_KIND_IF:
            free_ast_impl(node->if_stmt.cond);
            free_ast_impl(node->if_stmt.then_b);
            free_ast_impl(node->if_stmt.else_b);
            break;
        case NODE_KIND_WHILE:
            free_ast_impl(node->while_stmt.cond);
            free_ast_impl(node->while_stmt.body);
            break;
        case NODE_KIND_USE:
            // USE body is shared with module cache - still free it but visited set prevents double-free
            free_ast_impl(node->body);
            break;
        case NODE_KIND_FOR:
            free_ast_impl(node->for_stmt.init);
            free_ast_impl(node->for_stmt.cond);
            free_ast_impl(node->for_stmt.step);
            free_ast_impl(node->for_stmt.body);
            break;
        case NODE_KIND_DO_WHILE:
            free_ast_impl(node->do_while_stmt.cond);
            free_ast_impl(node->do_while_stmt.body);
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
        case NODE_KIND_MODULE_ACCESS:
        case NODE_KIND_MEMBER_ACCESS:
            free_ast_impl(node->lhs);
            free_ast_impl(node->rhs);
            break;
        case NODE_KIND_ENUM_LITERAL:
            free_ast_impl(node->enum_literal.payload_args);
            break;
        case NODE_KIND_MATCH:
            free_ast_impl(node->match_stmt.expr);
            free_ast_impl(node->match_stmt.cases);
            free_ast_impl(node->match_stmt.else_b);
            break;
        default:
            break;
    }

    free(node);
}

void free_ast_reset() {
    memset(freed_nodes, 0, sizeof(freed_nodes));
    freed_count = 0;
}

void free_ast(Node *node) {
    // Do NOT reset visited set here - it must persist across calls (e.g. module cache)
    // Call free_ast_reset() manually at start of cleanup phase if needed
    free_ast_impl(node);
}
