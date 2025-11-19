#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

const char *TOKEN_NAMES[] = {
    "TOKEN_TYPE_EOF",
	"TOKEN_TYPE_IDENTIFIER",
	"TOKEN_TYPE_INTEGER",

	"TOKEN_TYPE_RETURN",

	"TOKEN_TYPE_PLUS",
	"TOKEN_TYPE_MINUS",
	"TOKEN_TYPE_MULTIPLIER",
	"TOKEN_TYPE_DIVIDER",
	"TOKEN_TYPE_MOD",
	"TOKEN_TYPE_EQ",
	"TOKEN_TYPE_EQEQ",
	"TOKEN_TYPE_NE",
	"TOKEN_TYPE_LT",
	"TOKEN_TYPE_LE",
	"TOKEN_TYPE_GT",
	"TOKEN_TYPE_GE",
	"TOKEN_TYPE_LPAREN",
	"TOKEN_TYPE_RPAREN",
	"TOKEN_TYPE_LBRACE",
	"TOKEN_TYPE_RBRACE",
	"TOKEN_TYPE_LBRACKET",
	"TOKEN_TYPE_RBRACKET",
	"TOKEN_TYPE_COMMA",
	"TOKEN_TYPE_DOT",
	"TOKEN_TYPE_SEMICOLON",
	"TOKEN_TYPE_COLON",
	"TOKEN_TYPE_FN",
	"TOKEN_TYPE_ARROW",
	"TOKEN_TYPE_EQUAL",
	"TOKEN_TYPE_NOT",
	"TOKEN_TYPE_AND",
	"TOKEN_TYPE_OR",
	"TOKEN_TYPE_LET",
	"TOKEN_TYPE_IS",
	"TOKEN_TYPE_AS",
	"TOKEN_TYPE_WITH",
	"TOKEN_TYPE_ASSIGN",
	"TOKEN_TYPE_IF",
	"TOKEN_TYPE_ELSE",
	"TOKEN_TYPE_ASM",
	"TOKEN_TYPE_STRING",
	"TOKEN_TYPE_WHILE",
	"TOKEN_TYPE_SYSCALL",
	"TOKEN_TYPE_UNKNOWN",
	"TOKEN_TYPE_AND_AND",
	"TOKEN_TYPE_OR_OR",
	"TOKEN_TYPE_BREAK",
	"TOKEN_TYPE_CONTINUE",
	"TOKEN_TYPE_SIZEOF",
	"TOKEN_TYPE_CAST",
	"TOKEN_TYPE_SHL",
	"TOKEN_TYPE_SHR",
	"TOKEN_TYPE_USE",
};

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

void lexer_get_state(LexerState *state) {
    state->file = src;
    state->line = line;
    state->current_char = current_char;
    state->ahead_char = ahead_char;
}

void lexer_set_state(LexerState *state) {
    src = state->file;
    line = state->line;
    current_char = state->current_char;
    ahead_char = state->ahead_char;
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

    if (isalpha(current_char) || current_char == '_') {
        t.line = line;
        int i = 0;
        while (isalnum(current_char) || current_char == '_') {
            if (i < 254) {
                t.text[i++] = current_char;
            } else {
                printf("Erro: identificador muito longo na linha %d\n", line);
                exit(1);
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
        else if (strcmp(t.text, "break") == 0) {t.type = TOKEN_TYPE_BREAK;}
        else if (strcmp(t.text, "continue") == 0) {t.type = TOKEN_TYPE_CONTINUE;}
        else if (strcmp(t.text, "sizeof") == 0) {t.type = TOKEN_TYPE_SIZEOF;}
        else if (strcmp(t.text, "cast") == 0) {t.type = TOKEN_TYPE_CAST;}
        else if (strcmp(t.text, "use") == 0) {t.type = TOKEN_TYPE_USE;}

        else {t.type = TOKEN_TYPE_IDENTIFIER;}
        return t;
    }

    if (isdigit(current_char)) {
        t.line = line;
        int i = 0;
        
        // Hexadecimal support
        if (current_char == '0' && lookhead_char() == 'x') {
            next_char(); // consume '0'
            next_char(); // consume 'x'
            while (isxdigit(current_char)) {
                if (i < 254) {
                    t.text[i++] = current_char;
                } else {
                    printf("Erro: literal hexadecimal muito longo na linha %d\n", line);
                    exit(1);
                }
                next_char();
            }
            t.text[i] = '\0';
            t.type = TOKEN_TYPE_INTEGER;
            // Safe conversion check could be added here, but strtoll handles overflow by clamping (though we might want to error)
            t.int_value = strtoll(t.text, NULL, 16);
            return t;
        }

        while (isdigit(current_char)) {
            if (i < 254) {
                t.text[i++] = current_char;
            } else {
                printf("Erro: literal inteiro muito longo na linha %d\n", line);
                exit(1);
            }
            next_char();
        }
        t.text[i] = '\0';
        t.type = TOKEN_TYPE_INTEGER;
        // Check for overflow? For now, atoll is standard but unsafe for huge numbers.
        // Let's stick to standard behavior or simple check.
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
            if (lookhead_char() == '&') {
                strcpy(t.text, "&&");
                t.type = TOKEN_TYPE_AND_AND;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '&';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_AND;
                next_char();
                return t;
            }
        case '|':
            if (lookhead_char() == '|') {
                strcpy(t.text, "||");
                t.type = TOKEN_TYPE_OR_OR;
                next_char();
                next_char();
                return t;
            } else {
                t.text[0] = '|';
                t.text[1] = '\0';
                t.type = TOKEN_TYPE_OR;
                next_char();
                return t;
            }
        case '<':
            if (lookhead_char() == '=') {
                strcpy(t.text, "<=");
                t.type = TOKEN_TYPE_LE;
                next_char();
                next_char();
                return t;
            } else if (lookhead_char() == '<') {
                strcpy(t.text, "<<");
                t.type = TOKEN_TYPE_SHL;
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
            } else if (lookhead_char() == '>') {
                strcpy(t.text, ">>");
                t.type = TOKEN_TYPE_SHR;
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
                if (current_char == '\\') {
                    next_char();
                    char escaped = current_char;
                    if (current_char == 'n') escaped = '\n';
                    else if (current_char == 't') escaped = '\t';
                    else if (current_char == 'r') escaped = '\r';
                    else if (current_char == '0') escaped = '\0';
                    else if (current_char == '\\') escaped = '\\';
                    else if (current_char == '"') escaped = '"';
                    
                    if (strlen(t.text) < 255) {
                        int len = strlen(t.text);
                        t.text[len] = escaped;
                        t.text[len + 1] = '\0';
                    }
                    next_char();
                } else {
                    if (strlen(t.text) < 255) {
                        int len = strlen(t.text);
                        t.text[len] = current_char;
                        t.text[len + 1] = '\0';
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
        case '\'':
            next_char();
            char c = current_char;
            if (current_char == '\\') {
                next_char();
                if (current_char == 'n') c = '\n';
                else if (current_char == 't') c = '\t';
                else if (current_char == 'r') c = '\r';
                else if (current_char == '0') c = '\0';
                else if (current_char == '\\') c = '\\';
                else if (current_char == '\'') c = '\'';
            }
            next_char();
            if (current_char != '\'') {
                printf("Erro: char literal nao terminado na linha %d\n", line);
                exit(1);
            }
            next_char();
            t.type = TOKEN_TYPE_INTEGER;
            t.int_value = (long long)c;
            return t;
    }
    printf("Erro: caractere inesperado '%c' na linha %d\n", current_char, line);
    exit(1);
}
