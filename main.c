#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("É necessario colocar um arquivo, exemplo: ./caustic main.cst\n");
        return 1;
    }

    const char *filename = argv[1];
    FILE *file = fopen(filename, "r");

    if (!file) {
        printf("Não foi possível abrir o arquivo: %s\n", filename);
        return 1;
    }

    lexer_init(file);

    // loop mínimo para testar o lexer (pode remover depois)
    while (1) {
        Token t = lexer_next();

        printf("TOKEN: %d  (linha %d)\n", t.type, t.line);

        if (t.type == TOKEN_TYPE_IDENTIFIER) {
            printf("IDENT: %s\n", t.text);
        } else if (t.type == TOKEN_TYPE_INTEGER) {
            printf("INT: %lld\n", t.int_value);
        }

        if (t.type == TOKEN_TYPE_EOF)
            break;
    }

    fclose(file);
    return 0;
}
