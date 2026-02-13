#pragma once
#include <stdio.h>

typedef enum {
	TOKEN_TYPE_FLOAT,
	TOKEN_TYPE_EOF,
	TOKEN_TYPE_IDENTIFIER,
	TOKEN_TYPE_INTEGER,

	TOKEN_TYPE_RETURN,

	TOKEN_TYPE_PLUS,
	TOKEN_TYPE_MINUS,
	TOKEN_TYPE_MULTIPLIER,
	TOKEN_TYPE_DIVIDER,
	TOKEN_TYPE_MOD,
	TOKEN_TYPE_EQ,
	TOKEN_TYPE_EQEQ,
	TOKEN_TYPE_NE,
	TOKEN_TYPE_LT,
	TOKEN_TYPE_LE,
	TOKEN_TYPE_GT,
	TOKEN_TYPE_GE,
	TOKEN_TYPE_LPAREN,
	TOKEN_TYPE_RPAREN,
	TOKEN_TYPE_LBRACE,
	TOKEN_TYPE_RBRACE,
	TOKEN_TYPE_LBRACKET,
	TOKEN_TYPE_RBRACKET,
	TOKEN_TYPE_COMMA,
	TOKEN_TYPE_DOT,
	TOKEN_TYPE_SEMICOLON,
	TOKEN_TYPE_COLON,
	TOKEN_TYPE_FN,
	TOKEN_TYPE_ARROW,
	TOKEN_TYPE_EQUAL,
	TOKEN_TYPE_NOT,
	TOKEN_TYPE_AND,
	TOKEN_TYPE_OR,
	TOKEN_TYPE_LET,
	TOKEN_TYPE_IS,
	TOKEN_TYPE_AS,
	TOKEN_TYPE_WITH,
	TOKEN_TYPE_ASSIGN,
	TOKEN_TYPE_IF,
	TOKEN_TYPE_ELSE,
	TOKEN_TYPE_ASM,
	TOKEN_TYPE_STRING,
	TOKEN_TYPE_WHILE,
	TOKEN_TYPE_SYSCALL,
	TOKEN_TYPE_STRUCT,
	TOKEN_TYPE_AND_AND,
	TOKEN_TYPE_OR_OR,
	TOKEN_TYPE_BREAK,
	TOKEN_TYPE_CONTINUE,
	TOKEN_TYPE_SIZEOF,
	TOKEN_TYPE_CAST,
	TOKEN_TYPE_SHL,
	TOKEN_TYPE_SHR,
	TOKEN_TYPE_USE,
	TOKEN_TYPE_FOR,
	TOKEN_TYPE_DO,
	TOKEN_TYPE_ELLIPSIS,
	TOKEN_TYPE_XOR,
	TOKEN_TYPE_BITNOT,
	TOKEN_TYPE_CHAR,
	TOKEN_TYPE_GEN,
	TOKEN_TYPE_ENUM,
	TOKEN_TYPE_MATCH,
	TOKEN_TYPE_CASE,
	TOKEN_TYPE_DEFER,
	TOKEN_TYPE_EXTERN,
	TOKEN_TYPE_IMPL,
	TOKEN_TYPE_FN_PTR,
} TokenType;

extern const char *TOKEN_NAMES[];

typedef struct {
	TokenType type;
	const char *start;
	int length;
	int line;
    int col;
    const char *filename;

	long long int_value;
    double float_value;

	char text[256];
} Token;

typedef struct {
    FILE *file;
    int line;
    int col;
    const char *filename;
    int current_char;
    int ahead_char; // Added ahead_char to state
} LexerState;

void lexer_init(FILE *file, const char *filename);
Token lexer_next();
void lexer_get_state(LexerState *state);
void lexer_set_state(LexerState *state);
_Noreturn void error_at_token(Token t, const char *fmt, ...);
