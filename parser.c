#include "parser.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>

void parser_init() {
    lookahead_token = lexer_next();
    consume();
}

Node *parse() {
    return parse_stmt();
}

static void consume() {
    current_token = lookahead_token;
    lookahead_token = lexer_next();
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

    fprintf(stderr, "Erro de sintaxe na linha %d: era esperado um numero, mas encontrou '%s'\n", current_token.line, current_token.text);
    exit(1);
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
    if (current_token.type != TOKEN_TYPE_RETURN) {
        fprintf(stderr, "Erro de sintaxe na linha %d: era esperado 'return', mas encontrou '%s'\n", current_token.line, current_token.text);
        exit(1);
    }
    consume();

    Node *expr_node = parse_expr();

    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
        fprintf(stderr, "Erro de sintaxe na linha %d: era esperado ';' depois da expressao.\n", current_token.line);
        exit(1);
    }
    consume();

    return new_node_return(expr_node);
}

static void ast_print_recursive(Node *node, int depth) {
    if (!node) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    switch (node->kind) {
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
