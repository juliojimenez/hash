#include "tokenizer.h"
#include <string.h>
#include <ctype.h>
#include "utils.h"

static char step(Tokenizer *tk);

static void skip_horizontal_whitespace(Tokenizer *tk) {
    while (char_in_string(*tk->cur, " \t")) {
        step(tk);
    }
}

static char step(Tokenizer *tk) {
    if (*tk->cur == '\0') return '\0';
    char c = *tk->cur;
    tk->cur++;
    if (c == '\n') {
        tk->line++;
        tk->col = 0;
    } else {
        tk->col++;
    }
    return c;
}

static char peek(Tokenizer *tk) {
    return *tk->cur;
}

static char peek_next(Tokenizer *tk) {
    if (*(tk->cur + 1) == '\0') return '\0';
    return *(tk->cur + 1);
}

static Token build_token(Tokenizer *tk, TokenType type) {
    Token t;
    t.type = type;
    t.value.str = tk->tok_start;
    t.value.len = (size_t)(tk->cur - tk->tok_start);
    t.line = tk->tok_line;
    t.col = tk->tok_col;
    t.err_msg = NULL;
    return t;
}

static Token build_error(Tokenizer *tk, const char *msg) {
    Token t = build_token(tk, TOKEN_ERROR);
    t.err_msg = msg;
    return t;
}

static int is_operator_char(char c) {
    return char_in_string(c, "|&;<>()");
}

static int is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static void skip_single_quote(Tokenizer *tk) {
    step(tk);
    while (*tk->cur && *tk->cur != '\'') {
        step(tk);
    }
    if (*tk->cur == '\'') step(tk);
}

static void skip_double_quote(Tokenizer *tk) {
    step(tk);
    while (*tk->cur && *tk->cur != '"') {
        if (*tk->cur == '\\') {
            step(tk);
            if (*tk->cur) step(tk);
            continue;
        }
        if (*tk->cur == '$' && peek_next(tk) == '(') {
            step(tk);
            step(tk);
            if (*tk->cur == '(') {
                step(tk);
                int depth = 1;
                while (depth > 0 && *tk->cur) {
                    if (*tk->cur == '(' && peek_next(tk) == '(') {
                        depth++; step(tk); step(tk);
                    } else if (*tk->cur == ')' && peek_next(tk) == ')') {
                        depth--; step(tk); step(tk);
                    } else {
                        step(tk);
                    }
                }
            } else {
                int depth = 1;
                while (depth > 0 && *tk->cur) {
                    if (*tk->cur == '\\' && peek_next(tk)) {
                        step(tk); step(tk);
                    } else if (*tk->cur == '(') depth++;
                    else if (*tk->cur == ')') depth--;
                    if (depth > 0) step(tk);
                }
                if (*tk->cur == ')') step(tk);
            }
            continue;
        }
        if (*tk->cur == '$' && peek_next(tk) == '{') {
            step(tk); step(tk);
            int depth = 1;
            while (depth > 0 && *tk->cur) {
                if (*tk->cur == '\\' && peek_next(tk)) {
                    step(tk); step(tk);
                } else if (*tk->cur == '\'' && *tk->cur != '\'') {
                    step(tk);
                    while (*tk->cur && *tk->cur != '\'') {
                        step(tk);
                    }
                    if (*tk->cur == '\'') step(tk);
                } else if (*tk->cur == '\'' && peek_next(tk) == '(') {
                    step(tk); step(tk);
                    int subdepth = 1;
                    while (subdepth > 0 && *tk->cur) {
                        if (*tk->cur == '(') subdepth++;
                        else if (*tk->cur == ')') subdepth--;
                        if (subdepth > 0) step(tk);
                    }
                    if (*tk->cur == ')') step(tk);
                } else if (*tk->cur == '{') {
                    depth++; step(tk);
                } else if (*tk->cur == '}') {
                    depth--; if (depth > 0) step(tk);
                } else {
                    step(tk);
                }
            }
            if (*tk->cur == '}') step(tk);
            continue;
        }
        if (*tk->cur == '$' && peek_next(tk) == '$') {
            step(tk); step(tk);
            continue;
        }
        if (*tk->cur == '`') {
            step(tk);
            while (*tk->cur && *tk->cur != '`') {
                if (*tk->cur == '\\' && peek_next(tk) == '`') {
                    step(tk); step(tk);
                } else {
                    step(tk);
                }
            }
            if (*tk->cur == '`') step(tk);
            continue;
        }
        step(tk);
    }
    if (*tk->cur == '"') step(tk);
}

static void read_word(Tokenizer *tk) {
    while (*tk->cur) {
        char c = *tk->cur;

        if (char_in_string(c, " \t\n")) break;
        if (is_operator_char(c)) break;

        if (c == '\\') {
            step(tk);
            if (*tk->cur) step(tk);
            continue;
        }

        if (c == '\'') {
            skip_single_quote(tk);
            continue;
        }

        if (c == '"') {
            skip_double_quote(tk);
            continue;
        }

        if (c == '$') {
            step(tk);
            if (*tk->cur == '(' && peek_next(tk) == '(') {
                step(tk); step(tk);
                int depth = 1;
                while (depth > 0 && *tk->cur) {
                    if (*tk->cur == '(' && peek_next(tk) == '(') {
                        depth++; step(tk); step(tk);
                    } else if (*tk->cur == ')' && peek_next(tk) == ')') {
                        depth--; step(tk); step(tk);
                    } else {
                        step(tk);
                    }
                }
            } else if (*tk->cur == '(') {
                step(tk);
                int depth = 1;
                while (depth > 0 && *tk->cur) {
                    if (*tk->cur == '\\' && peek_next(tk)) {
                        step(tk); step(tk);
                    } else if (*tk->cur == '(') depth++;
                    else if (*tk->cur == ')') depth--;
                    if (depth > 0) step(tk);
                }
                if (*tk->cur == ')') step(tk);
            } else if (*tk->cur == '{') {
                step(tk);
                int depth = 1;
                while (depth > 0 && *tk->cur) {
                    if (*tk->cur == '\\' && peek_next(tk)) {
                        step(tk); step(tk);
                    } else if (*tk->cur == '\'') {
                        step(tk);
                        while (*tk->cur && *tk->cur != '\'') {
                            step(tk);
                        }
                        if (*tk->cur == '\'') step(tk);
                    } else if (*tk->cur == '"') {
                        step(tk);
                        while (*tk->cur && *tk->cur != '"') {
                            if (*tk->cur == '\\') {
                                step(tk); if (*tk->cur) step(tk);
                            } else {
                                step(tk);
                            }
                        }
                        if (*tk->cur == '"') step(tk);
                    } else if (*tk->cur == '$' && peek_next(tk) == '(') {
                        step(tk); step(tk);
                        if (*tk->cur == '(') {
                            step(tk);
                            int subdepth = 1;
                            while (subdepth > 0 && *tk->cur) {
                                if (*tk->cur == '(' && peek_next(tk) == '(') {
                                    subdepth++; step(tk); step(tk);
                                } else if (*tk->cur == ')' && peek_next(tk) == ')') {
                                    subdepth--; step(tk); step(tk);
                                } else {
                                    step(tk);
                                }
                            }
                        } else {
                            int subdepth = 1;
                            while (subdepth > 0 && *tk->cur) {
                                if (*tk->cur == '(') subdepth++;
                                else if (*tk->cur == ')') subdepth--;
                                if (subdepth > 0) step(tk);
                            }
                            if (*tk->cur == ')') step(tk);
                        }
                    } else if (*tk->cur == '$' && peek_next(tk) == '{') {
                        step(tk); step(tk); depth++;
                    } else if (*tk->cur == '{') {
                        depth++; step(tk);
                    } else if (*tk->cur == '}') {
                        depth--; if (depth > 0) step(tk);
                    } else {
                        step(tk);
                    }
                }
                if (*tk->cur == '}') step(tk);
            } else if (is_name_char(*tk->cur)) {
                while (*tk->cur && is_name_char(*tk->cur)) {
                    step(tk);
                }
            } else if (*tk->cur == '?') {
                step(tk);
            } else if (*tk->cur == '$') {
                step(tk);
            } else if (*tk->cur == '!') {
                step(tk);
            } else if (*tk->cur == '#') {
                step(tk);
            } else if (*tk->cur == '-') {
                step(tk);
            } else if (*tk->cur == '@') {
                step(tk);
            } else if (*tk->cur == '*') {
                step(tk);
            } else if (*tk->cur == '0') {
                step(tk);
            }
            continue;
        }

        if (c == '`') {
            step(tk);
            while (*tk->cur && *tk->cur != '`') {
                if (*tk->cur == '\\' && peek_next(tk) == '`') {
                    step(tk); step(tk);
                } else {
                    step(tk);
                }
            }
            if (*tk->cur == '`') step(tk);
            continue;
        }

        step(tk);
    }
}

void tokenizer_init(Tokenizer *tk, const char *src) {
    tk->src = src;
    tk->cur = src;
    tk->line = 0;
    tk->col = 0;
    tk->tok_start = NULL;
    tk->tok_line = 0;
    tk->tok_col = 0;
}

void tokenizer_destroy(Tokenizer *tk) {
    (void)tk;
}

Token tokenizer_next(Tokenizer *tk) {
    skip_horizontal_whitespace(tk);

    tk->tok_start = tk->cur;
    tk->tok_line = tk->line;
    tk->tok_col = tk->col;

    char c = peek(tk);

    switch (c) {
        case '\0':
            return build_token(tk, TOKEN_EOF);

        case '\n':
            step(tk);
            return build_token(tk, TOKEN_NEWLINE);

        case '#':
            while (*tk->cur && *tk->cur != '\n') {
                step(tk);
            }
            return tokenizer_next(tk);

        case '|':
            step(tk);
            if (peek(tk) == '|') {
                step(tk);
                return build_token(tk, TOKEN_OR_IF);
            }
            return build_token(tk, TOKEN_PIPE);

        case '&':
            step(tk);
            if (peek(tk) == '&') {
                step(tk);
                return build_token(tk, TOKEN_AND_IF);
            }
            return build_token(tk, TOKEN_AMPERSAND);

        case ';':
            step(tk);
            if (peek(tk) == ';') {
                step(tk);
                return build_token(tk, TOKEN_DSEMI);
            }
            return build_token(tk, TOKEN_SEMI);

        case '(':
            step(tk);
            return build_token(tk, TOKEN_LPAREN);

        case ')':
            step(tk);
            return build_token(tk, TOKEN_RPAREN);

        case '<':
            step(tk);
            if (peek(tk) == '<') {
                step(tk);
                if (peek(tk) == '-') {
                    step(tk);
                    return build_token(tk, TOKEN_DLESSDASH);
                }
                return build_token(tk, TOKEN_DLESS);
            }
            if (peek(tk) == '>') {
                step(tk);
                return build_token(tk, TOKEN_LESSGREAT);
            }
            if (peek(tk) == '&') {
                step(tk);
                return build_token(tk, TOKEN_LESSAND);
            }
            return build_token(tk, TOKEN_LESS);

        case '>':
            step(tk);
            if (peek(tk) == '>') {
                step(tk);
                return build_token(tk, TOKEN_DGREAT);
            }
            if (peek(tk) == '&') {
                step(tk);
                return build_token(tk, TOKEN_GREATAND);
            }
            return build_token(tk, TOKEN_GREAT);

        default:
            if (isdigit((unsigned char)c)) {
                const char *saved_cur = tk->cur;
                size_t saved_line = tk->line;
                size_t saved_col = tk->col;
                size_t tok_saved_line = tk->tok_line;
                size_t tok_saved_col = tk->tok_col;
                const char *tok_saved_start = tk->tok_start;

                while (isdigit((unsigned char)peek(tk))) {
                    step(tk);
                }

                if (peek(tk) == '<' || peek(tk) == '>') {
                    return build_token(tk, TOKEN_IO_NUMBER);
                }

                tk->cur = saved_cur;
                tk->line = saved_line;
                tk->col = saved_col;
                tk->tok_start = tok_saved_start;
                tk->tok_line = tok_saved_line;
                tk->tok_col = tok_saved_col;
            }
            read_word(tk);
            if (tk->cur == tk->tok_start) {
                return build_error(tk, "unexpected character");
            }
            return build_token(tk, TOKEN_WORD);
    }
}
