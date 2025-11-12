#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static FILE *src;
static int current_char;
static int ahead_char;
static int line = 1;

static void next_char(void) {
	current_char = fgetc(src);
	if (current_char == '\n') {
		line++;
	}
}

static int lookhead_char() {
    int c = fgetc(src);
    ungetc(c, src);
    return c;
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
		t.line = line;
		t.type = TOKEN_TYPE_EOF;
		return t;
	}

    if (isalpha(current_char)) {
        t.line = line;
        int i = 0;
        while (isalnum(current_char)) {
            t.text[i++] = current_char;
            next_char();
        }
        t.text[i] = '\0';

        if (strcmp(t.text, "return") == 0) {t.type = TOKEN_TYPE_RETURN;}
        else if (strcmp(t.text, "fn") == 0) {t.type = TOKEN_TYPE_FN;}

        else {t.type = TOKEN_TYPE_IDENTIFIER;}
        return t;
    }

    if (isdigit(current_char)) {
        t.line = line;
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

    t.line = line;
    switch (current_char) {
        case '+':
            t.text[0] = '+';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_PLUS;
            next_char();
            return t;
        case '-':
            if (lookhead_char() == '>') {
                strcpy(t.text, "->");
                t.type = TOKEN_TYPE_ARROW;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '-';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_MINUS;
                next_char();
                return t;
            }
        case '*':
            t.text[0] = '*';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_MULTIPLIER;
            next_char();
            return t;
        case '/':
            t.text[0] = '/';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_DIVIDER;
            next_char();
            return t;
        case ';':
            t.text[0] = ';';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_SEMICOLON;
            next_char();
            return t;
        case ':':
            t.text[0] = ':';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_COLON;
            next_char();
            return t;
        case '=':
            t.text[0] = '=';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_EQUAL;
            next_char();
            return t;
        case '!':
            t.text[0] = '!';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_NOT;
            next_char();
            return t;
        case '&':
            t.text[0] = '&';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_AND;
            next_char();
            return t;
        case '|':
            t.text[0] = '|';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_OR;
            next_char();
            return t;
        case '(':
            t.text[0] = '(';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_LPAREN;
            next_char();
            return t;
        case ')':
            t.text[0] = ')';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_RPAREN;
            next_char();
            return t;
        case '[':
            t.text[0] = '[';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_LBRACKET;
            next_char();
            return t;
        case ']':
            t.text[0] = ']';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_RBRACKET;
            next_char();
            return t;
        case '{':
            t.text[0] = '{';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_LBRACE;
            next_char();
            return t;
        case '}':
            t.text[0] = '}';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_RBRACE;
            next_char();
            return t;
    }

    printf("caractere inesperado: %c", current_char);
    next_char();
    return lexer_next();
}
