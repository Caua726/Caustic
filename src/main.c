#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    const char *output_filename = NULL;
    int debug_lexer = 0;
    int debug_parser = 0;
    int debug_ir = 0;
    int compile_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Uso: ./caustic <arquivo> [opcoes]\n");
            printf("Opcoes:\n");
            printf("  -o <arquivo>    Define o arquivo de saida (assembly)\n");
            printf("  -c              Compile only (no main required)\n");
            printf("  -debuglexer     Mostra tokens do lexer\n");
            printf("  -debugparser    Mostra AST do parser\n");
            printf("  -debugir        Mostra IR gerado\n");
            printf("  --help          Mostra esta mensagem\n");
            return 0;
        } else if (strcmp(argv[i], "-debuglexer") == 0) {
            debug_lexer = 1;
        } else if (strcmp(argv[i], "-debugparser") == 0) {
            debug_parser = 1;
        } else if (strcmp(argv[i], "-debugir") == 0) {
            debug_ir = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_only = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_filename = argv[++i];
            } else {
                fprintf(stderr, "Erro: esperado nome do arquivo apos '-o'\n");
                return 1;
            }
        } else {
            if (filename == NULL) {
                filename = argv[i];
            } else {
                fprintf(stderr, "Erro: multiplos arquivos de entrada fornecidos: '%s' e '%s'\n", filename, argv[i]);
                return 1;
            }
        }
    }

    if (filename == NULL) {
        printf("É necessario colocar um arquivo, exemplo: ./caustic main.cst\n");
        printf("Use --help para ver as opcoes.\n");
        return 1;
    }

    types_init();

    if (debug_lexer) {
        FILE *file = fopen(filename, "r");
        if (!file) {
            printf("Não foi possível abrir o arquivo: %s\n", filename);
            return 1;
        }

        printf("=== DEBUG LEXER ===\n");
        lexer_init(file, filename);

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

    // Reabrir o arquivo para o parsing real
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Não foi possível abrir o arquivo: %s\n", filename);
        return 1;
    }

    lexer_init(file, filename);
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


        ir_no_main_required = compile_only;
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
        if (output_filename) {
            strncpy(output, output_filename, sizeof(output) - 1);
            output[sizeof(output) - 1] = '\0';
        } else {
            snprintf(output, sizeof(output), "%s.s", filename);
        }

        FILE *out_file = fopen(output, "w");
        if (!out_file) {
            printf("Erro ao abrir arquivo de saida: %s\n", output);
            ir_free(ir);
            fclose(file);
            return 1;
        }

        codegen(ir, out_file);
        fclose(out_file);

        printf("Compilado com sucesso: %s\n", output);

    free_ast_reset();

    if (ast) {
        free_ast(ast);
        ast = NULL;
    }
    
    ir_free(ir);
    fclose(file);
    } else {
        printf("Erro ao fazer parsing do arquivo.\n");
    }

    free_module_cache();  
    free_source_paths();
    semantic_cleanup();
    free_strings();
    free_all_types();
    return 0;
}
