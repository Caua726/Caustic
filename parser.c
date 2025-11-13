#include "parser.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token current_token;
static Token lookahead_token;

static void consume();
static Node *parse_block();
static Node *parse_fn();
static void expect(TokenType type);
static Node *parse_var_decl();
static Node *parse_expr();
static Node *parse_stmt();
static Node *parse_add();
static Node *parse_mul();
static Node *parse_primary();
static Type *parse_type();


void parser_init() {
    lookahead_token = lexer_next();
    consume();
}

Node *parse() {
    return parse_fn();
}

static void consume() {
    current_token = lookahead_token;
    lookahead_token = lexer_next();
}

static void expect(TokenType type) {
    if (current_token.type == type) {
        consume();
    } else {
        fprintf(stderr, "Erro na linha %d: esperado '%s' mas encontrou '%s'\n",
                current_token.line, TOKEN_NAMES[type], TOKEN_NAMES[current_token.type]);
        exit(1);
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
    return parse_add();
}

static Type *parse_type() {
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        fprintf(stderr, "Erro de sintaxe na linha %d: esperado um tipo\n", current_token.line);
        exit(1);
    }

    Type *ty = calloc(1, sizeof(Type));

    if (strcmp(current_token.text, "int") == 0 || strcmp(current_token.text, "i32") == 0) {
        ty->kind = TY_I32;
    } else if (strcmp(current_token.text, "i8") == 0) {
        ty->kind = TY_I8;
    } else if (strcmp(current_token.text, "i16") == 0) {
        ty->kind = TY_I16;
    } else if (strcmp(current_token.text, "i64") == 0) {
        ty->kind = TY_I64;
    } else if (strcmp(current_token.text, "u8") == 0) {
        ty->kind = TY_U8;
    } else if (strcmp(current_token.text, "u16") == 0) {
        ty->kind = TY_U16;
    } else if (strcmp(current_token.text, "u32") == 0) {
        ty->kind = TY_U32;
    } else if (strcmp(current_token.text, "u64") == 0) {
        ty->kind = TY_U64;
    } else if (strcmp(current_token.text, "float") == 0 || strcmp(current_token.text, "f32") == 0) {
        ty->kind = TY_F32;
    } else if (strcmp(current_token.text, "f64") == 0) {
        ty->kind = TY_F64;
    } else if (strcmp(current_token.text, "bool") == 0) {
        ty->kind = TY_BOOL;
    } else if (strcmp(current_token.text, "char") == 0) {
        ty->kind = TY_CHAR;
    } else if (strcmp(current_token.text, "string") == 0) {
        ty->kind = TY_STRING;
    } else if (strcmp(current_token.text, "void") == 0) {
        ty->kind = TY_VOID;
    } else {
        fprintf(stderr, "Erro: tipo desconhecido '%s'\n", current_token.text);
        exit(1);
    }

    consume();
    return ty;
}


static Node* parse_var_decl() {
    expect(TOKEN_TYPE_LET);
    expect(TOKEN_TYPE_IS);

    Type *var_type = parse_type();

    expect(TOKEN_TYPE_AS);

    if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
        fprintf(stderr, "Erro de sintaxe na linha %d: era esperado um nome de variável, mas encontrou '%s'\n", current_token.line, current_token.text);
        exit(1);
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
    if (current_token.type == TOKEN_TYPE_INTEGER) {
        Node *node = new_node_num(current_token.int_value);
        consume();
        return node;
    }

    if (current_token.type == TOKEN_TYPE_IDENTIFIER) {
        Node *node = new_node(NODE_KIND_IDENTIFIER);
        node->name = strdup(current_token.text);
        consume();
        return node;
    }

    fprintf(stderr, "Erro de sintaxe na linha %d: era esperado um numero ou identificador, mas encontrou '%s'\n", current_token.line, current_token.text);
    exit(1);
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
        fprintf(stderr, "Erro na linha %d: esperado nome da função.\n", current_token.line);
        exit(1);
    }
    fn_node->name = strdup(current_token.text);
    consume();

    expect(TOKEN_TYPE_LPAREN);
    expect(TOKEN_TYPE_RPAREN);

    if (current_token.type == TOKEN_TYPE_ARROW) {
        consume();
        if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Erro na linha %d: esperado tipo de retorno.\n", current_token.line);
            exit(1);
        }
        consume(); // Consome o tipo de retorno
    }
    fn_node->body = parse_block();
    return fn_node;
}

static Node *parse_mul() {
    Node *node = parse_primary();
    while (current_token.type == TOKEN_TYPE_MULTIPLIER || current_token.type == TOKEN_TYPE_DIVIDER) {
        if (current_token.type == TOKEN_TYPE_MULTIPLIER) {
            consume();
            node = new_binary_node(NODE_KIND_MULTIPLIER, node, parse_primary());
        } else if (current_token.type == TOKEN_TYPE_DIVIDER) {
            consume();
            node = new_binary_node(NODE_KIND_DIVIDER, node, parse_primary());
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

static Node *parse_stmt() {
    if (current_token.type == TOKEN_TYPE_LET) {
        return parse_var_decl();
    }

    if (current_token.type == TOKEN_TYPE_RETURN) {
        consume();
        Node *expr_node = parse_expr();

        if (current_token.type != TOKEN_TYPE_SEMICOLON) {
            fprintf(stderr, "Erro de sintaxe na linha %d: era esperado ';' depois da expressao.\n", current_token.line);
            exit(1);
        }
        consume();

        return new_node_return(expr_node);
    }

    if (current_token.type == TOKEN_TYPE_LBRACE) {
        return parse_block();
    }

    fprintf(stderr, "Erro de sintaxe na linha %d: era esperado 'let', 'return' ou bloco, mas encontrou '%s'\n", current_token.line, current_token.text);
    exit(1);
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
        default:
            printf("Nó Desconhecido\n");
            break;
    }
}

void ast_print(Node *node) {
    ast_print_recursive(node, 0);
}
