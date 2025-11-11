#include "parser.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>

static Token current_token;
static Token lookahead_token;

static Node *parse_expr();
static Node *parse_stmt();
static Node *new_node(NodeKind kind);
static Node *new_node_num(long val);
static Node *new_node_return(Node *expr);
static void consume();

void parser_init() {
    consume();
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
    Node *node = new_node(NODE_NUM);
    node->val = val;
    return node;
}

static Node *new_node_return(Node *expr) {
    Node *node = new_node(NODE_RETURN);
    node->expr = expr;
    return node;
}

static Node *parse_expr() {
    if (current_token.type != TOKEN_TYPE_INTEGER) {
        fprintf(stderr, "Erro de sintaxe na linha %d: era esperado um numero, mas encontrou '%s'\n", current_token.line, current_token.text);
        exit(1);
    }
    Node *node = new_node_num(current_token.int_value);
    consume();
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
        case NODE_RETURN:
            printf("ReturnStmt\n");
            ast_print_recursive(node->expr, depth + 1);
            break;
        case NODE_NUM:
            printf("NumberLiteral(%ld)\n", node->val);
            break;
        default:
            printf("Nó Desconhecido\n");
            break;
    }
    ast_print_recursive(node->next, depth);
}

void ast_print(Node *node) {
    ast_print_recursive(node, 0);
}
