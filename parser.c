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
    struct IncludedFile *next;
} IncludedFile;

static IncludedFile *visited_files = NULL;

// Retorna 1 se já visitou, 0 se não. Adiciona na lista se 0.
int mark_file_visited(const char *path) {
    IncludedFile *cur = visited_files;
    while (cur) {
        if (strcmp(cur->path, path) == 0) {
            return 1;
        }
        cur = cur->next;
    }

    IncludedFile *new_file = calloc(1, sizeof(IncludedFile));
    new_file->path = strdup(path);
    new_file->next = visited_files;
    visited_files = new_file;
    return 0;
}

StringLiteral *get_strings() {
    return strings_head;
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
static void parse_file_body(Node **cur_ptr);
static void parse_use(Node **cur_ptr);
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

    // Create NODE_KIND_USE
    Node *use_node = new_node(NODE_KIND_USE);
    use_node->name = alias; // Store alias in name
    // We can store path in string literal or just use val/member_name? 
    // Let's use member_name for path for now, or create a string literal node?
    // Node struct has 'member_name'. Let's use that for path.
    use_node->member_name = strdup(path);

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

    // Recursive parsing
    if (!mark_file_visited(canonical_path)) {
        FILE *f = fopen(canonical_path, "r");
        if (!f) {
            error_at_token(current_token, "nao foi possivel abrir o arquivo '%s'", canonical_path);
        }
        
        // We need to keep the filename string alive for tokens
        char *persistent_path = strdup(canonical_path);

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

        if (current_token.type == TOKEN_TYPE_LET) {
            cur->next = parse_var_decl();
            cur = cur->next;
            continue;
        }

        if (current_token.type != TOKEN_TYPE_FN) {
            error_at_token(current_token, "era esperado 'fn', 'let', 'struct' ou 'use' mas encontrou '%s'", current_token.text);
        }

        cur->next = parse_fn();
        // The original code had a critical error check here, but the instruction implies removing it.
        // If parse_fn() returns NULL, it means an error occurred and error_at_token should have been called.
        cur = cur->next;
    }
    *cur_ptr = cur;
}

Node *parse() {
    Node head = {};
    Node *cur = &head;

    parse_file_body(&cur);

    // NÃO chamar consume() aqui - já estamos no EOF
    return head.next;
}

// Global list of allocated types for GC
Type *all_types = NULL;

Type *new_type(TypeKind kind) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->next = all_types;
    all_types = ty;
    return ty;
}

void free_all_types() {
    Type *t = all_types;
    while (t) {
        Type *next = t->next;
        free(t);
        t = next;
    }
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
        // Check if it's a struct type
        // For now, we just assume any unknown identifier used as a type is a struct name
        // The semantic analyzer will verify if it exists.
        ty = new_type(TY_STRUCT);
        ty->name = strdup(current_token.text);
    }

    consume();
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
        if (lookahead_token.type == TOKEN_TYPE_LPAREN) {
            Node *node = new_node(NODE_KIND_FNCALL);
            node->name = strdup(current_token.text);
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

static Node *parse_fn() {
    expect(TOKEN_TYPE_FN);

    Node *fn_node = new_node(NODE_KIND_FN);

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        error_at_token(current_token, "esperado nome da função.");
    }
    fn_node->name = strdup(current_token.text);
    consume();

    expect(TOKEN_TYPE_LPAREN);

    // Parse parameters
    if (current_token.type != TOKEN_TYPE_RPAREN) {
        Node head = {};
        Node *cur = &head;
        while (1) {
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
            Node *member_node = new_node(NODE_KIND_MEMBER_ACCESS);
            member_node->lhs = node;
            member_node->member_name = strdup(current_token.text);
            consume();
            node = member_node;
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

static Node *parse_logical_and() {
    Node *node = parse_comparison();
    while (current_token.type == TOKEN_TYPE_AND_AND) {
        consume();
        node = new_binary_node(NODE_KIND_LOGICAL_AND, node, parse_comparison());
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
            printf("Function(%s)\n", node->name);
            ast_print_recursive(node->body, depth + 1);
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
        default:
            printf("Nó Desconhecido\n");
            break;
    }
}

void ast_print(Node *node) {
    ast_print_recursive(node, 0);
}

void free_ast(Node *node) {
    if (!node) return;

    // Se for uma lista (next), liberar o próximo primeiro
    free_ast(node->next);

    if (node->name) free(node->name);
    if (node->member_name) free(node->member_name);
    
    // Liberar filhos baseados no tipo
    switch (node->kind) {
        case NODE_KIND_FN:
            free_ast(node->params);
            free_ast(node->body);
            break;
        case NODE_KIND_BLOCK:
            free_ast(node->stmts);
            break;
        case NODE_KIND_RETURN:
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_CAST:
        case NODE_KIND_ADDR:
        case NODE_KIND_DEREF:
            free_ast(node->expr);
            break;
        case NODE_KIND_FNCALL:
        case NODE_KIND_SYSCALL:
            free_ast(node->args);
            break;
        case NODE_KIND_LET:
            free_ast(node->init_expr);
            break;
        case NODE_KIND_IF:
            free_ast(node->if_stmt.cond);
            free_ast(node->if_stmt.then_b);
            free_ast(node->if_stmt.else_b);
            break;
        case NODE_KIND_WHILE:
            free_ast(node->while_stmt.cond);
            free_ast(node->while_stmt.body);
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
            free_ast(node->lhs);
            free_ast(node->rhs);
            break;
        default:
            break;
    }

    free(node);
}
