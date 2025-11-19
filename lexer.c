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
            if (i < 255) {
                t.text[i++] = current_char;
            }
            next_char();
        }
        t.text[i] = '\0';

        if (strcmp(t.text, "return") == 0) {t.type = TOKEN_TYPE_RETURN;}
        else if (strcmp(t.text, "fn") == 0) {t.type = TOKEN_TYPE_FN;}
        else if (strcmp(t.text, "let") == 0) {t.type = TOKEN_TYPE_LET;}
        else if (strcmp(t.text, "is") == 0) {t.type = TOKEN_TYPE_IS;}
        else if (strcmp(t.text, "as") == 0) {t.type = TOKEN_TYPE_AS;}
        else if (strcmp(t.text, "with") == 0) {t.type = TOKEN_TYPE_WITH;}
        else if (strcmp(t.text, "if") == 0) {t.type = TOKEN_TYPE_IF;}
        else if (strcmp(t.text, "else") == 0) {t.type = TOKEN_TYPE_ELSE;}
        else if (strcmp(t.text, "asm") == 0) {t.type = TOKEN_TYPE_ASM;}
        else if (strcmp(t.text, "while") == 0) {t.type = TOKEN_TYPE_WHILE;}
        else if (strcmp(t.text, "syscall") == 0) {t.type = TOKEN_TYPE_SYSCALL;}
        else if (strcmp(t.text, "struct") == 0) {t.type = TOKEN_TYPE_STRUCT;}

        else {t.type = TOKEN_TYPE_IDENTIFIER;}
        return t;
    }

    if (isdigit(current_char)) {
        t.line = line;
        int i = 0;
        while (isdigit(current_char)) {
            if (i < 255) {
                t.text[i++] = current_char;
            }
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
            if (lookhead_char() == '/') {
                while (current_char != '\n' && current_char != EOF) {
                    next_char();
                }
                return lexer_next();
            }
            t.text[0] = '/';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_DIVIDER;
            next_char();
            return t;
        case '%':
            t.text[0] = '%';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_MOD;
            next_char();
            return t;
        case ';':
            t.text[0] = ';';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_SEMICOLON;
            next_char();
            return t;
        case ',':
            t.text[0] = ',';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_COMMA;
            next_char();
            return t;
        case ':':
            if (lookhead_char() == '=') {
                strcpy(t.text, ":=");
                t.type = TOKEN_TYPE_ASSIGN;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = ':';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_COLON;
                next_char();
                return t;
            }
        case '=':
            if (lookhead_char() == '=') {
                strcpy(t.text, "==");
                t.type = TOKEN_TYPE_EQEQ;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '=';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_EQUAL;
                next_char();
                return t;
            }
        case '!':
            if (lookhead_char() == '=') {
                strcpy(t.text, "!=");
                t.type = TOKEN_TYPE_NE;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '!';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_NOT;
                next_char();
                return t;
            }
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
        case '<':
            if (lookhead_char() == '=') {
                strcpy(t.text, "<=");
                t.type = TOKEN_TYPE_LE;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '<';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_LT;
                next_char();
                return t;
            }
        case '>':
            if (lookhead_char() == '=') {
                strcpy(t.text, ">=");
                t.type = TOKEN_TYPE_GE;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '>';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_GT;
                next_char();
                return t;
            }
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
        case '.':
            t.text[0] = '.';
            t.text[1] = '\0';
            t.type = TOKEN_TYPE_DOT;
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
        case '"':
            next_char();
            t.text[0] = '\0';

            while (current_char != '"' && current_char != EOF && current_char != '\n') {
                if (current_char == '\\' && lookhead_char() == '"') {
                    next_char();
                    if (strlen(t.text) < 255) {
                        t.text[strlen(t.text)] = '"';
                        t.text[strlen(t.text) + 1] = '\0';
                    }
                    next_char();
                } else {
                    if (strlen(t.text) < 255) {
                        t.text[strlen(t.text)] = current_char;
                        t.text[strlen(t.text) + 1] = '\0';
                    }
                    next_char();
                }
            }

            if (current_char != '"') {
                printf("Erro: string nao terminada na linha %d\n", line);
                exit(1);
            }

            t.type = TOKEN_TYPE_STRING;
            next_char();
            return t;
    }
    printf("caractere inesperado: %c", current_char);
    next_char();
    return lexer_next();
}
