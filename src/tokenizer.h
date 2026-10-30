#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h>
#include "string_view.h"

typedef enum {
    TOKEN_WORD,
    TOKEN_IO_NUMBER,
    TOKEN_PIPE,
    TOKEN_AND_IF,
    TOKEN_OR_IF,
    TOKEN_SEMI,
    TOKEN_AMPERSAND,
    TOKEN_DSEMI,
    TOKEN_NEWLINE,
    TOKEN_LESS,
    TOKEN_GREAT,
    TOKEN_DGREAT,
    TOKEN_DLESS,
    TOKEN_DLESSDASH,
    TOKEN_LESSAND,
    TOKEN_GREATAND,
    TOKEN_LESSGREAT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF,
    TOKEN_ERROR,
} TokenType;

typedef struct {
    TokenType type;
    StringView value;
    size_t line, col;
    const char *err_msg;
} Token;

typedef struct {
    const char *src;
    const char *cur;
    size_t line, col;
    const char *tok_start;
    size_t tok_line, tok_col;
} Tokenizer;

void tokenizer_init(Tokenizer *tk, const char *src);
void tokenizer_destroy(Tokenizer *tk);
Token tokenizer_next(Tokenizer *tk);

#endif
