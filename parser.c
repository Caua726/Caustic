#include "parser.h"
#include "lexer.h"
#include "semantic.h"

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
static Node *parse_if_stmt();
static Node *parse_comparison();
static Node *parse_add();
static Node *parse_mul();
static Node *parse_primary();
static Type *parse_type();


void parser_init() {
    lookahead_token = lexer_next();
    consume();
}
Node *parse() {
    Node head = {};
    Node *cur = &head;

    // Parsear todas as funções até encontrar EOF
    while (current_token.type != TOKEN_TYPE_EOF) {
        if (current_token.type != TOKEN_TYPE_FN) {
            fprintf(stderr, "Erro na linha %d: era esperado 'fn' mas encontrou '%s'\n",
                    current_token.line, current_token.text);
            exit(1);
        }

        cur->next = parse_fn();
        if (!cur->next) {
            fprintf(stderr, "Erro crítico: parse_fn() retornou NULL\n");
            exit(1);
        }
        cur = cur->next;
    }

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
    return parse_comparison();
}

static Type *parse_type() {
    // Array: [N]T
    if (current_token.type == TOKEN_TYPE_LBRACKET) {
        consume(); // [
        if (current_token.type != TOKEN_TYPE_INTEGER) {
            fprintf(stderr, "Erro na linha %d: Tamanho do array deve ser um inteiro.\n", current_token.line);
            exit(1);
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
        fprintf(stderr, "Erro de sintaxe na linha %d: esperado um tipo\n", current_token.line);
        exit(1);
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
        fprintf(stderr, "Erro: tipo desconhecido '%s'\n", current_token.text);
        exit(1);
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

    if (current_token.type == TOKEN_TYPE_LPAREN) {
        Token paren_tok = current_token;
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

    // Parse parameters
    if (current_token.type != TOKEN_TYPE_RPAREN) {
        Node head = {};
        Node *cur = &head;
        while (1) {
            if (current_token.type != TOKEN_TYPE_IDENTIFIER) {
                fprintf(stderr, "Erro na linha %d: esperado nome do parâmetro.\n", current_token.line);
                exit(1);
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
        fn_node->return_type = type_void; // Default to void if not specified? Or i32? Let's say void for now or keep i32 if that was previous behavior. Previous was i32 default.
        // Actually, let's stick to previous behavior of i32 default if omitted, or maybe void. 
        // The user example `fn main() as i32` suggests explicit typing. 
        // If omitted, let's assume void to be safe, or i32 if that's what we did.
        fn_node->return_type = type_i32; 
    }
    fn_node->body = parse_block();
    return fn_node;
}

static Node *parse_postfix() {
    Node *node = parse_primary();

    while (current_token.type == TOKEN_TYPE_LBRACKET) {
        consume(); // [
        Node *index = parse_expr();
        expect(TOKEN_TYPE_RBRACKET); // ]
        
        Node *idx_node = new_node(NODE_KIND_INDEX);
        idx_node->lhs = node; // Array/Pointer
        idx_node->rhs = index; // Index
        node = idx_node;
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

static Node *parse_comparison() {
    Node *node = parse_add();
    while (1) {
        if (current_token.type == TOKEN_TYPE_EQEQ) {
            consume();
            node = new_binary_node(NODE_KIND_EQ, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_NE) {
            consume();
            node = new_binary_node(NODE_KIND_NE, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_LT) {
            consume();
            node = new_binary_node(NODE_KIND_LT, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_LE) {
            consume();
            node = new_binary_node(NODE_KIND_LE, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_GT) {
            consume();
            node = new_binary_node(NODE_KIND_GT, node, parse_add());
        } else if (current_token.type == TOKEN_TYPE_GE) {
            consume();
            node = new_binary_node(NODE_KIND_GE, node, parse_add());
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

static Node *parse_asm_stmt() {
    consume();
    expect(TOKEN_TYPE_LPAREN);

    if (current_token.type != TOKEN_TYPE_STRING) {
        fprintf(stderr, "Erro na linha %d: esperado string apos 'asm('\n",
                current_token.line);
        exit(1);
    }

    Node *asm_node = new_node(NODE_KIND_ASM);
    asm_node->name = strdup(current_token.text);
    consume();

    expect(TOKEN_TYPE_RPAREN);
    expect(TOKEN_TYPE_SEMICOLON);

    return asm_node;
}

static Node *parse_stmt() {
    if (current_token.type == TOKEN_TYPE_LET) {
        return parse_var_decl();
    }

    if (current_token.type == TOKEN_TYPE_IF) {
        return parse_if_stmt();
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

    if (current_token.type == TOKEN_TYPE_ASM) {
        return parse_asm_stmt();
    }

    if (current_token.type == TOKEN_TYPE_WHILE) {
        return parse_while_stmt();
    }
    Node *node = parse_expr();
    if (current_token.type == TOKEN_TYPE_EQUAL) {
        consume();
        Node *assign_node = new_node(NODE_KIND_ASSIGN);
        assign_node->lhs = node;
        assign_node->rhs = parse_expr();
        expect(TOKEN_TYPE_SEMICOLON);
        return assign_node;
    }

    expect(TOKEN_TYPE_SEMICOLON);
    Node *expr_stmt_node = new_node(NODE_KIND_EXPR_STMT);
    expr_stmt_node->expr = node;
    return expr_stmt_node;

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
