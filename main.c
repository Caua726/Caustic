#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("É necessario colocar um arquivo, exemplo: ./caustic main.cst\n");
        return 1;
    }

    const char *filename = argv[1];

    // Verificar flags de debug
    int debug_lexer = 0;
    int debug_parser = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-debuglexer") == 0) {
            debug_lexer = 1;
        } else if (strcmp(argv[i], "-debugparser") == 0) {
            debug_parser = 1;
        }
    }

    // Se debug_lexer estiver ativo, mostrar tokens
    if (debug_lexer) {
        FILE *file = fopen(filename, "r");
        if (!file) {
            printf("Não foi possível abrir o arquivo: %s\n", filename);
            return 1;
        }

        printf("=== DEBUG LEXER ===\n");
        lexer_init(file);

        while (1) {
            Token t = lexer_next();
            printf("LINE: %d\n", t.line);
            printf("  TOKEN: %d  (linha %d) \n", t.type, t.line);
            printf("    TOKENTYPE: %s\n", TOKEN_NAMES[t.type]);
            if (t.text[0] != '\0') {
                printf("    TOKENTEXT: %s\n", t.text);
            }
            if (t.type != TOKEN_TYPE_EOF) {
                printf("    TOKENID: %d\n", t.type);
            }
            if (t.type == TOKEN_TYPE_EOF)
                break;
        }
        printf("===================\n\n");
        fclose(file);
    }

    // Parsing (reabre o arquivo para parsing)
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Não foi possível abrir o arquivo: %s\n", filename);
        return 1;
    }

    lexer_init(file);
    parser_init();
    Node *ast = parse();

    if (ast) {
        if (debug_parser) {
            printf("=== DEBUG PARSER - AST ===\n");
            ast_print(ast);
            printf("==========================\n");
        }
        printf("Arquivo parseado com sucesso!\n");
    } else {
        printf("Erro ao fazer parsing do arquivo.\n");
    }

    fclose(file);
    return 0;
}
