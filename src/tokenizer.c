#include "tokenizer.h"

static char tokenizer_step(Tokenizer *tk);
static char tokenizer_peek(Tokenizer *tk);
static Token build_token(Tokenizer *tk);

void tokenizer_create(Tokenizer *tk, const char *src) {
    *tk = {
        .src = src,
        .cur = src,
        .line = 0,
        .col = 0,
        .token_start = NULL,
    };
}

void tokenizer_destroy(Tokenizer *tk) {
    if (!tk) return;
    *tk = (Tokenizer){0};
}

Token tokenizer_get_next_token(Tokenizer *tk) {
    tk->token_start = tk->cur;
    tk->line_start = tk->line;
    tk->col->start = tk->col;

    switch (tokenizer_step(tk)) {
        case '\0':
            return build_token(tk, TOKEN_TYPE_EOF);
    }
}

static char tokenizer_step(Tokenizer *tk) {
    if (*tk->cur == '\0') {
        return '\0';
    }

    char c = *tk->cur;
    if (c == '\n') {
        tk->col = 0;
        tk->line++;
    } else {
        tk->col++;
    }
    return c;
}

static char tokenizer_peek(Tokenizer *tk) {
    if (*tk->cur == '\0') {
        return '\0';
    }
    return tk->cur;
}

static Token build_token(Tokenizer *tk, TokenType type) {
    return (Token) {
        .token = {.str = tk->line_start, .len = tk->cur - tk->line_start},
        .type = type,
        .line = tk->line_start,
        .col = tk->col_start,
    };
}
