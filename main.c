#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("É necessario colocar um arquivo, exemplo: ./caustic main.cst\n");
        return 1;
    }

    const char *filename = argv[1];

    int debug_lexer = 0;
    int debug_parser = 0;
    int debug_ir = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-debuglexer") == 0) {
            debug_lexer = 1;
        } else if (strcmp(argv[i], "-debugparser") == 0) {
            debug_parser = 1;
        } else if (strcmp(argv[i], "-debugir") == 0) {
            debug_ir = 1;
        }
    }

    types_init();

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
        // IMPORTANTE: Não retornar aqui, continuar com o parsing normal
    }

    // Reabrir o arquivo para o parsing real
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Não foi possível abrir o arquivo: %s\n", filename);
        return 1;
    }

    lexer_init(file);
    parser_init();
    Node *ast = parse();

    if (ast) {
        analyze(ast);
        if (debug_parser) {
            printf("=== DEBUG PARSER - AST ===\n");
            int func_num = 0;
            for (Node *fn = ast; fn; fn = fn->next) {
                printf("\n--- Função #%d ---\n", ++func_num);
                ast_print(fn);
            }
            printf("\n==========================\n\n");
        }
        // NÃO chamar consume() aqui

        IRProgram *ir = gen_ir(ast);
        if (!ir) {
            printf("Erro ao gerar IR.\n");
            fclose(file);
            return 1;
        }

        if (debug_ir) {
            ir_print(ir);
        }

        char output[256];
        snprintf(output, sizeof(output), "%s.s", filename);

        codegen(ir, output);

        printf("Compilado com sucesso: %s\n", output);

        ir_free(ir);
    } else {
        printf("Erro ao fazer parsing do arquivo.\n");
    }

    if (ast) free_ast(ast);
    semantic_cleanup();
    free_ast(ast);
    free_all_types();
    return 0;
}
