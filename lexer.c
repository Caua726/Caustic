#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static FILE *src;
static int current_char;
static int line = 1;

static void next_char(void) {
	current_char = fgetc(src);
	if (current_char == '\n') {
		line++;
	}
}

void lexer_init(FILE *file) {
	src = file;
	next_char();
}

Token lexer_next() {
	Token t = {0};

	while (isspace(current_char)) {
		next_char();
	}

	if (current_char == EOF) {
		t.type = TOKEN_TYPE_EOF;
		return t;
	}

    if (isalpha(current_char)) {
        int i = 0;
        while (isalnum(current_char)) {
            t.text[i++] = current_char;
            next_char();
        }
        t.text[i] = '\0';

        if (strcmp(t.text, "return") == 0) {t.type = TOKEN_TYPE_RETURN;}

        t.type = TOKEN_TYPE_IDENTIFIER;
        return t;
    }

    if (isdigit(current_char)) {
        int i = 0;
        while (isdigit(current_char)) {
            t.text[i++] = current_char;
            next_char();
        }
        t.text[i] = '\0';
        t.type = TOKEN_TYPE_INTEGER;
        t.int_value = atoll(t.text);
        return t;
    }

    if (current_char == ';') {
        t.type = TOKEN_TYPE_SEMICOLON;
        next_char();
        return t;
    }

    printf("caractere inesperado: %c", current_char);
    next_char();
    return lexer_next();
}
