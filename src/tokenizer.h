#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "string_view.h"

typedef enum {
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_ERROR,
    TOKEN_TYPE_MAX
} TokenType;

typedef struct {
    StringView token;
    int line, col;
    TokenType type;

    const char *err_msg;
} Token;

typedef struct {
    const char *src;

    const char *cur;
    const char *token_start;
    int line_start, col_start;

    int line, col;
} Tokenizer;

void tokenizer_create(Tokenizer *tk, const char *src);

void tokenizer_destroy(Tokenizer *tk);

Token tokenizer_get_next_token(Tokenizer *tk);

#endif
