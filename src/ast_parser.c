#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_parser.h"
#include "tokenizer.h"

// A here-document whose body still has to be read from the lines after the
// current command line. The RedirVec lives in an arena node and never moves;
// its items array may, so the entry is addressed by index.
typedef struct {
    RedirVec *vec;
    size_t index;
} PendingHeredoc;

typedef struct {
    Tokenizer tk;
    Arena *arena;

    Token lookahead[2];
    int lookahead_count;
    const char *last_end;   /* End of the most recently consumed token */

    ParseStatus status;
    const char *message;
    size_t err_line;
    size_t err_col;

    PendingHeredoc *heredocs;
    size_t heredoc_count;
    size_t heredoc_cap;
} Parser;

// ============================================================================
// Failure reporting (the first failure wins)
// ============================================================================

static void fail_at(Parser *p, const Token *at, ParseStatus status, const char *message) {
    if (p->status != PARSE_OK) {
        return;
    }
    p->status = status;
    p->message = message;
    p->err_line = at->line;
    p->err_col = at->col;
}

static void fail_oom(Parser *p) {
    if (p->status == PARSE_OK) {
        p->status = PARSE_ERROR;
        p->message = "out of memory";
    }
}

// Unexpected token: end of input means the construct is incomplete
static void unexpected(Parser *p, const Token *t) {
    if (p->status != PARSE_OK) {
        return;
    }
    if (t->type == TOKEN_EOF) {
        fail_at(p, t, PARSE_INCOMPLETE, "unexpected end of input");
        return;
    }
    char buf[96];
    if (t->type == TOKEN_NEWLINE) {
        snprintf(buf, sizeof(buf), "syntax error near unexpected token `newline'");
    } else if (t->type == TOKEN_ERROR) {
        snprintf(buf, sizeof(buf), "syntax error: %s", t->err_msg ? t->err_msg : "bad token");
    } else {
        size_t n = t->value.len > 40 ? 40 : t->value.len;
        snprintf(buf, sizeof(buf), "syntax error near unexpected token `%.*s'", (int)n, t->value.str);
    }
    char *message = arena_strdup(p->arena, buf);
    fail_at(p, t, PARSE_ERROR, message ? message : "syntax error");
}

// ============================================================================
// Token helpers
// ============================================================================

static bool tok_is_word(const Token *t, const char *s) {
    size_t n = strlen(s);
    return t->type == TOKEN_WORD && t->value.len == n && memcmp(t->value.str, s, n) == 0;
}

static bool is_redirect_op(TokenType type) {
    switch (type) {
        case TOKEN_LESS:
        case TOKEN_GREAT:
        case TOKEN_DGREAT:
        case TOKEN_DLESS:
        case TOKEN_DLESSDASH:
        case TOKEN_LESSAND:
        case TOKEN_GREATAND:
        case TOKEN_LESSGREAT:
            return true;
        default:
            return false;
    }
}

static bool starts_redirect(const Token *t) {
    return t->type == TOKEN_IO_NUMBER || is_redirect_op(t->type);
}

// Reserved words that close a compound list
static bool is_closer_word(const Token *t) {
    static const char *const closers[] = {"then", "do", "done", "fi", "elif", "else", "esac", "}", NULL};
    for (int i = 0; closers[i]; i++) {
        if (tok_is_word(t, closers[i])) {
            return true;
        }
    }
    return false;
}

// Tokens at which a compound list stops
static bool ends_compound_list(const Token *t) {
    return t->type == TOKEN_EOF || t->type == TOKEN_RPAREN || t->type == TOKEN_DSEMI || is_closer_word(t);
}

static bool starts_compound_command(const Token *t) {
    if (t->type == TOKEN_LPAREN) {
        return true;
    }
    static const char *const openers[] = {"{", "if", "for", "while", "until", "case", NULL};
    for (int i = 0; openers[i]; i++) {
        if (tok_is_word(t, openers[i])) {
            return true;
        }
    }
    return false;
}

static bool is_name(const char *s, size_t n) {
    if (n == 0 || !(isalpha((unsigned char)s[0]) || s[0] == '_')) {
        return false;
    }
    for (size_t i = 1; i < n; i++) {
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_')) {
            return false;
        }
    }
    return true;
}

static bool is_assignment(const char *word) {
    const char *eq = strchr(word, '=');
    if (!eq || eq == word) {
        return false;
    }
    return is_name(word, (size_t)(eq - word));
}

// A word made only of backslash-newline pairs: a line continuation between
// tokens, which stands for nothing at all
static bool is_continuation_only(const Token *t) {
    if (t->type != TOKEN_WORD || t->value.len == 0 || t->value.len % 2 != 0) {
        return false;
    }
    for (size_t i = 0; i < t->value.len; i += 2) {
        if (t->value.str[i] != '\\' || t->value.str[i + 1] != '\n') {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Here-document bodies
// ============================================================================

// Line [start, eol) with leading tabs dropped when strip is set
static void heredoc_line(const char *start, const char *eol, bool strip, const char **content, size_t *len) {
    const char *c = start;
    size_t n = (size_t)(eol - start);
    if (strip) {
        while (n > 0 && *c == '\t') {
            c++;
            n--;
        }
    }
    *content = c;
    *len = n;
}

static bool read_one_heredoc(Parser *p, Redirect *r) {
    const char *delim = r->target;
    size_t delim_len = strlen(delim);
    bool strip = (r->op == TOKEN_DLESSDASH);
    const char *cursor = tokenizer_cursor(&p->tk);

    // First pass: find the delimiter line and size the body
    const char *line = cursor;
    const char *after = NULL;
    size_t body_len = 0;
    for (;;) {
        const char *eol = strchr(line, '\n');
        const char *end = eol ? eol : line + strlen(line);
        const char *content;
        size_t len;
        heredoc_line(line, end, strip, &content, &len);
        if (len == delim_len && memcmp(content, delim, delim_len) == 0) {
            after = eol ? eol + 1 : end;
            break;
        }
        if (!eol) {
            break;  // Ran out of input before the delimiter
        }
        body_len += len + 1;
        line = eol + 1;
    }
    if (!after) {
        Token eof_tok = {TOKEN_EOF, {NULL, 0}, p->tk.line, p->tk.col, NULL};
        fail_at(p, &eof_tok, PARSE_INCOMPLETE, "here-document delimited by end of input");
        return false;
    }

    // Second pass: copy the body
    char *body = arena_alloc(p->arena, body_len + 1);
    if (!body) {
        fail_oom(p);
        return false;
    }
    size_t pos = 0;
    line = cursor;
    for (;;) {
        const char *eol = strchr(line, '\n');
        const char *end = eol ? eol : line + strlen(line);
        const char *content;
        size_t len;
        heredoc_line(line, end, strip, &content, &len);
        if (len == delim_len && memcmp(content, delim, delim_len) == 0) {
            break;
        }
        memcpy(body + pos, content, len);
        pos += len;
        body[pos++] = '\n';
        line = eol + 1;
    }
    body[pos] = '\0';
    r->heredoc_body = body;

    tokenizer_advance_to(&p->tk, after);
    return true;
}

// Called right after the tokenizer produced a newline (or hit the end of the
// input): every here-document announced on the line that just ended has its
// body here
static void read_pending_heredocs(Parser *p) {
    for (size_t i = 0; i < p->heredoc_count; i++) {
        PendingHeredoc *h = &p->heredocs[i];
        if (!read_one_heredoc(p, &h->vec->items[h->index])) {
            break;
        }
    }
    p->heredoc_count = 0;
}

static bool add_pending_heredoc(Parser *p, RedirVec *vec, size_t index) {
    if (p->heredoc_count == p->heredoc_cap) {
        size_t new_cap = p->heredoc_cap ? p->heredoc_cap * 2 : 4;
        PendingHeredoc *fresh = arena_alloc(p->arena, new_cap * sizeof(PendingHeredoc));
        if (!fresh) {
            fail_oom(p);
            return false;
        }
        if (p->heredoc_count > 0) {
            memcpy(fresh, p->heredocs, p->heredoc_count * sizeof(PendingHeredoc));
        }
        p->heredocs = fresh;
        p->heredoc_cap = new_cap;
    }
    p->heredocs[p->heredoc_count].vec = vec;
    p->heredocs[p->heredoc_count].index = index;
    p->heredoc_count++;
    return true;
}

// ============================================================================
// Lookahead
// ============================================================================

static Token fetch(Parser *p) {
    for (;;) {
        Token t = tokenizer_next(&p->tk);
        if (is_continuation_only(&t)) {
            continue;
        }
        if ((t.type == TOKEN_NEWLINE || t.type == TOKEN_EOF) && p->heredoc_count > 0) {
            read_pending_heredocs(p);
        }
        return t;
    }
}

static Token *peek(Parser *p) {
    if (p->lookahead_count == 0) {
        p->lookahead[0] = fetch(p);
        p->lookahead_count = 1;
    }
    return &p->lookahead[0];
}

static Token *peek2(Parser *p) {
    peek(p);
    if (p->lookahead_count == 1) {
        p->lookahead[1] = fetch(p);
        p->lookahead_count = 2;
    }
    return &p->lookahead[1];
}

static Token advance(Parser *p) {
    Token t = *peek(p);
    p->lookahead[0] = p->lookahead[1];
    p->lookahead_count--;
    p->last_end = t.value.str + t.value.len;
    return t;
}

static void skip_newlines(Parser *p) {
    while (p->status == PARSE_OK && peek(p)->type == TOKEN_NEWLINE) {
        advance(p);
    }
}

static bool expect_word(Parser *p, const char *keyword) {
    Token *t = peek(p);
    if (!tok_is_word(t, keyword)) {
        unexpected(p, t);
        return false;
    }
    advance(p);
    return true;
}

static bool expect_type(Parser *p, TokenType type) {
    Token *t = peek(p);
    if (t->type != type) {
        unexpected(p, t);
        return false;
    }
    advance(p);
    return true;
}

// ============================================================================
// Words
// ============================================================================

// Copy a word into the arena, dropping backslash-newline pairs (outside
// single quotes). Quotes, escapes and expansions stay raw for the expander.
static char *materialize(Parser *p, const Token *t) {
    const char *s = t->value.str;
    size_t n = t->value.len;
    char *out = arena_alloc(p->arena, n + 1);
    if (!out) {
        fail_oom(p);
        return NULL;
    }
    size_t o = 0;
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!in_single && c == '\\' && i + 1 < n) {
            if (s[i + 1] == '\n') {
                i++;
                continue;
            }
            out[o++] = c;
            out[o++] = s[++i];
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

// Here-document delimiter: quotes removed; *quoted says whether any were there
static char *unquote_delimiter(Parser *p, const char *word, bool *quoted) {
    size_t n = strlen(word);
    char *out = arena_alloc(p->arena, n + 1);
    if (!out) {
        fail_oom(p);
        return NULL;
    }
    *quoted = false;
    size_t o = 0;
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < n; i++) {
        char c = word[i];
        if (in_single) {
            if (c == '\'') {
                in_single = false;
            } else {
                out[o++] = c;
            }
            continue;
        }
        if (c == '\\' && i + 1 < n) {
            *quoted = true;
            out[o++] = word[++i];
            continue;
        }
        if (c == '\'') {
            in_single = true;
            *quoted = true;
            continue;
        }
        if (c == '"') {
            in_double = !in_double;
            *quoted = true;
            continue;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

static int io_number_value(const Token *t) {
    int fd = 0;
    for (size_t i = 0; i < t->value.len && fd < 100000; i++) {
        fd = fd * 10 + (t->value.str[i] - '0');
    }
    return fd;
}

// ============================================================================
// Grammar
// ============================================================================

static Node *parse_command(Parser *p);
static Node *parse_compound_list(Parser *p);

static Node *new_node(Parser *p, NodeType type, const Token *at) {
    Node *node = ast_node_new(p->arena, type, at->line, at->col);
    if (!node) {
        fail_oom(p);
        return NULL;
    }
    node->src = at->value.str;
    return node;
}

// Close the node's source span at the last consumed token
static Node *finish(Parser *p, Node *node) {
    if (node && p->last_end >= node->src) {
        node->src_len = (size_t)(p->last_end - node->src);
    }
    return node;
}

static bool parse_redirect(Parser *p, RedirVec *vec) {
    Redirect r;
    memset(&r, 0, sizeof(r));
    r.fd = -1;

    Token t = *peek(p);
    if (t.type == TOKEN_IO_NUMBER) {
        r.fd = io_number_value(&t);
        advance(p);
        t = *peek(p);
    }
    if (!is_redirect_op(t.type)) {
        unexpected(p, &t);
        return false;
    }
    r.op = t.type;
    advance(p);

    Token target = *peek(p);
    if (target.type != TOKEN_WORD) {
        unexpected(p, &target);
        return false;
    }
    advance(p);
    char *word = materialize(p, &target);
    if (!word) {
        return false;
    }

    if (r.fd < 0) {
        bool input = (r.op == TOKEN_LESS || r.op == TOKEN_DLESS || r.op == TOKEN_DLESSDASH ||
                      r.op == TOKEN_LESSAND || r.op == TOKEN_LESSGREAT);
        r.fd = input ? 0 : 1;
    }

    if (r.op == TOKEN_DLESS || r.op == TOKEN_DLESSDASH) {
        r.target = unquote_delimiter(p, word, &r.heredoc_quoted);
        if (!r.target) {
            return false;
        }
        if (!ast_redirs_push(p->arena, vec, r)) {
            fail_oom(p);
            return false;
        }
        return add_pending_heredoc(p, vec, vec->count - 1);
    }

    r.target = word;
    if (!ast_redirs_push(p->arena, vec, r)) {
        fail_oom(p);
        return false;
    }
    return true;
}

static bool parse_redirect_list(Parser *p, Node *node) {
    while (p->status == PARSE_OK && starts_redirect(peek(p))) {
        if (!parse_redirect(p, &node->redirs)) {
            return false;
        }
    }
    return p->status == PARSE_OK;
}

static Node *parse_simple(Parser *p) {
    Token first = *peek(p);
    Node *node = new_node(p, NODE_SIMPLE, &first);
    if (!node) {
        return NULL;
    }

    bool seen_command = false;
    while (p->status == PARSE_OK) {
        Token *t = peek(p);
        if (starts_redirect(t)) {
            if (!parse_redirect(p, &node->redirs)) {
                return NULL;
            }
            continue;
        }
        if (t->type != TOKEN_WORD) {
            break;
        }
        char *word = materialize(p, t);
        if (!word) {
            return NULL;
        }
        advance(p);
        bool ok;
        if (!seen_command && is_assignment(word)) {
            ok = ast_words_push(p->arena, &node->u.simple.assignments, word);
        } else {
            seen_command = true;
            ok = ast_words_push(p->arena, &node->u.simple.words, word);
        }
        if (!ok) {
            fail_oom(p);
            return NULL;
        }
    }
    if (p->status != PARSE_OK) {
        return NULL;
    }
    if (node->u.simple.words.count == 0 && node->u.simple.assignments.count == 0 &&
        node->redirs.count == 0) {
        unexpected(p, peek(p));
        return NULL;
    }
    return finish(p, node);
}

static Node *parse_group(Parser *p, NodeType type) {
    Token open = advance(p);
    Node *body = parse_compound_list(p);
    if (!body) {
        return NULL;
    }
    if (type == NODE_SUBSHELL) {
        if (!expect_type(p, TOKEN_RPAREN)) {
            return NULL;
        }
    } else if (!expect_word(p, "}")) {
        return NULL;
    }
    Node *node = new_node(p, type, &open);
    if (!node) {
        return NULL;
    }
    node->u.group.body = body;
    if (!parse_redirect_list(p, node)) {
        return NULL;
    }
    return finish(p, node);
}

static Node *parse_if(Parser *p) {
    Token kw = advance(p);
    Node *node = new_node(p, NODE_IF, &kw);
    if (!node) {
        return NULL;
    }

    for (;;) {
        Node *condition = parse_compound_list(p);
        if (!condition || !expect_word(p, "then")) {
            return NULL;
        }
        Node *body = parse_compound_list(p);
        if (!body) {
            return NULL;
        }
        if (!ast_if_clauses_push(p->arena, &node->u.if_clause.clauses, condition, body)) {
            fail_oom(p);
            return NULL;
        }
        if (!tok_is_word(peek(p), "elif")) {
            break;
        }
        advance(p);
    }

    if (tok_is_word(peek(p), "else")) {
        advance(p);
        node->u.if_clause.else_body = parse_compound_list(p);
        if (!node->u.if_clause.else_body) {
            return NULL;
        }
    }
    if (!expect_word(p, "fi") || !parse_redirect_list(p, node)) {
        return NULL;
    }
    return finish(p, node);
}

static Node *parse_loop(Parser *p, NodeType type) {
    Token kw = advance(p);
    Node *node = new_node(p, type, &kw);
    if (!node) {
        return NULL;
    }
    node->u.loop.condition = parse_compound_list(p);
    if (!node->u.loop.condition || !expect_word(p, "do")) {
        return NULL;
    }
    node->u.loop.body = parse_compound_list(p);
    if (!node->u.loop.body || !expect_word(p, "done") || !parse_redirect_list(p, node)) {
        return NULL;
    }
    return finish(p, node);
}

static Node *parse_for(Parser *p) {
    Token kw = advance(p);
    Node *node = new_node(p, NODE_FOR, &kw);
    if (!node) {
        return NULL;
    }

    Token *name = peek(p);
    if (name->type != TOKEN_WORD || !is_name(name->value.str, name->value.len)) {
        unexpected(p, name);
        return NULL;
    }
    node->u.for_clause.var = materialize(p, name);
    if (!node->u.for_clause.var) {
        return NULL;
    }
    advance(p);
    skip_newlines(p);

    if (tok_is_word(peek(p), "in")) {
        advance(p);
        node->u.for_clause.has_words = true;
        while (p->status == PARSE_OK && peek(p)->type == TOKEN_WORD) {
            char *word = materialize(p, peek(p));
            if (!word || !ast_words_push(p->arena, &node->u.for_clause.words, word)) {
                fail_oom(p);
                return NULL;
            }
            advance(p);
        }
        Token *sep = peek(p);
        if (sep->type == TOKEN_SEMI) {
            advance(p);
            skip_newlines(p);
        } else if (sep->type == TOKEN_NEWLINE) {
            skip_newlines(p);
        } else {
            unexpected(p, sep);
            return NULL;
        }
    } else if (peek(p)->type == TOKEN_SEMI) {
        advance(p);
        skip_newlines(p);
    }

    if (!expect_word(p, "do")) {
        return NULL;
    }
    node->u.for_clause.body = parse_compound_list(p);
    if (!node->u.for_clause.body || !expect_word(p, "done") || !parse_redirect_list(p, node)) {
        return NULL;
    }
    return finish(p, node);
}

static bool parse_case_patterns(Parser *p, CaseItem *item) {
    for (;;) {
        Token *t = peek(p);
        if (t->type != TOKEN_WORD) {
            unexpected(p, t);
            return false;
        }
        char *pattern = materialize(p, t);
        if (!pattern || !ast_words_push(p->arena, &item->patterns, pattern)) {
            fail_oom(p);
            return false;
        }
        advance(p);
        if (peek(p)->type != TOKEN_PIPE) {
            return true;
        }
        advance(p);
    }
}

static Node *parse_case(Parser *p) {
    Token kw = advance(p);
    Node *node = new_node(p, NODE_CASE, &kw);
    if (!node) {
        return NULL;
    }

    Token *word = peek(p);
    if (word->type != TOKEN_WORD) {
        unexpected(p, word);
        return NULL;
    }
    node->u.case_clause.word = materialize(p, word);
    if (!node->u.case_clause.word) {
        return NULL;
    }
    advance(p);
    skip_newlines(p);
    if (!expect_word(p, "in")) {
        return NULL;
    }
    skip_newlines(p);

    while (p->status == PARSE_OK && !tok_is_word(peek(p), "esac")) {
        if (peek(p)->type == TOKEN_LPAREN) {
            advance(p);
        }
        CaseItem item;
        memset(&item, 0, sizeof(item));
        if (!parse_case_patterns(p, &item) || !expect_type(p, TOKEN_RPAREN)) {
            return NULL;
        }
        skip_newlines(p);

        Token *body_start = peek(p);
        if (body_start->type == TOKEN_DSEMI || tok_is_word(body_start, "esac")) {
            item.body = new_node(p, NODE_LIST, body_start);
        } else {
            item.body = parse_compound_list(p);
        }
        if (!item.body) {
            return NULL;
        }
        if (!ast_case_items_push(p->arena, &node->u.case_clause.items, item)) {
            fail_oom(p);
            return NULL;
        }

        Token *term = peek(p);
        if (term->type == TOKEN_DSEMI) {
            advance(p);
            skip_newlines(p);
        } else if (!tok_is_word(term, "esac")) {
            unexpected(p, term);
            return NULL;
        }
    }
    if (!expect_word(p, "esac") || !parse_redirect_list(p, node)) {
        return NULL;
    }
    return finish(p, node);
}

static Node *parse_function(Parser *p) {
    Token name = advance(p);
    if (!expect_type(p, TOKEN_LPAREN) || !expect_type(p, TOKEN_RPAREN)) {
        return NULL;
    }
    skip_newlines(p);
    if (!starts_compound_command(peek(p))) {
        unexpected(p, peek(p));
        return NULL;
    }
    Node *body = parse_command(p);
    if (!body) {
        return NULL;
    }
    Node *node = new_node(p, NODE_FUNCTION, &name);
    if (!node) {
        return NULL;
    }
    node->u.function.name = materialize(p, &name);
    node->u.function.body = body;
    return node->u.function.name ? finish(p, node) : NULL;
}

static Node *parse_command(Parser *p) {
    Token *t = peek(p);
    if (p->status != PARSE_OK) {
        return NULL;
    }
    if (t->type == TOKEN_LPAREN) {
        return parse_group(p, NODE_SUBSHELL);
    }
    if (t->type == TOKEN_WORD) {
        if (tok_is_word(t, "{")) {
            return parse_group(p, NODE_BRACE);
        }
        if (tok_is_word(t, "if")) {
            return parse_if(p);
        }
        if (tok_is_word(t, "for")) {
            return parse_for(p);
        }
        if (tok_is_word(t, "while")) {
            return parse_loop(p, NODE_WHILE);
        }
        if (tok_is_word(t, "until")) {
            return parse_loop(p, NODE_UNTIL);
        }
        if (tok_is_word(t, "case")) {
            return parse_case(p);
        }
        if (is_closer_word(t) || tok_is_word(t, "in") || tok_is_word(t, "!")) {
            unexpected(p, t);
            return NULL;
        }
        if (is_name(t->value.str, t->value.len) && peek2(p)->type == TOKEN_LPAREN) {
            return parse_function(p);
        }
        return parse_simple(p);
    }
    if (starts_redirect(t)) {
        return parse_simple(p);
    }
    unexpected(p, t);
    return NULL;
}

static Node *parse_pipeline(Parser *p) {
    Token first = *peek(p);
    bool negated = false;
    if (tok_is_word(&first, "!")) {
        advance(p);
        negated = true;
    }
    Node *command = parse_command(p);
    if (!command) {
        return NULL;
    }
    Node *node = new_node(p, NODE_PIPELINE, &first);
    if (!node || !ast_nodes_push(p->arena, &node->u.pipeline.commands, command)) {
        fail_oom(p);
        return NULL;
    }
    node->u.pipeline.negated = negated;
    while (p->status == PARSE_OK && peek(p)->type == TOKEN_PIPE) {
        advance(p);
        skip_newlines(p);
        Node *next = parse_command(p);
        if (!next) {
            return NULL;
        }
        if (!ast_nodes_push(p->arena, &node->u.pipeline.commands, next)) {
            fail_oom(p);
            return NULL;
        }
    }
    return p->status == PARSE_OK ? finish(p, node) : NULL;
}

static Node *parse_and_or(Parser *p) {
    Node *left = parse_pipeline(p);
    if (!left) {
        return NULL;
    }
    while (p->status == PARSE_OK) {
        Token *t = peek(p);
        if (t->type != TOKEN_AND_IF && t->type != TOKEN_OR_IF) {
            break;
        }
        Token op = advance(p);
        skip_newlines(p);
        Node *right = parse_pipeline(p);
        if (!right) {
            return NULL;
        }
        Node *node = new_node(p, NODE_AND_OR, &op);
        if (!node) {
            return NULL;
        }
        node->u.and_or.left = left;
        node->u.and_or.op = op.type;
        node->u.and_or.right = right;
        node->src = left->src;
        left = finish(p, node);
    }
    return p->status == PARSE_OK ? left : NULL;
}

// and_or items separated by ; & or newlines. At top level the list runs to
// the end of input; inside a compound command it stops at the closing word.
static Node *parse_list(Parser *p, bool top_level) {
    Token at = *peek(p);
    Node *list = new_node(p, NODE_LIST, &at);
    if (!list) {
        return NULL;
    }
    skip_newlines(p);
    while (p->status == PARSE_OK) {
        Token *t = peek(p);
        if (t->type == TOKEN_EOF || (!top_level && ends_compound_list(t))) {
            break;
        }
        Node *item = parse_and_or(p);
        if (!item) {
            return NULL;
        }
        bool async = false;
        Token *sep = peek(p);
        if (sep->type == TOKEN_SEMI) {
            advance(p);
        } else if (sep->type == TOKEN_AMPERSAND) {
            advance(p);
            async = true;
        } else if (sep->type != TOKEN_NEWLINE && sep->type != TOKEN_EOF &&
                   !(!top_level && ends_compound_list(sep))) {
            unexpected(p, sep);
            return NULL;
        }
        if (!ast_list_push(p->arena, &list->u.list.items, item, async)) {
            fail_oom(p);
            return NULL;
        }
        skip_newlines(p);
    }
    return p->status == PARSE_OK ? finish(p, list) : NULL;
}

// A compound list must contain at least one command
static Node *parse_compound_list(Parser *p) {
    Node *list = parse_list(p, false);
    if (!list) {
        return NULL;
    }
    if (list->u.list.items.count == 0) {
        unexpected(p, peek(p));
        return NULL;
    }
    return list;
}

// ============================================================================
// Entry point
// ============================================================================

AstParseResult parse_program(const char *src, Arena *arena) {
    Parser p;
    memset(&p, 0, sizeof(p));
    tokenizer_init(&p.tk, src);
    p.arena = arena;
    p.status = PARSE_OK;
    p.last_end = src;

    Node *root = parse_list(&p, true);

    if (p.status == PARSE_OK && p.tk.unterminated) {
        Token eof_tok = {TOKEN_EOF, {NULL, 0}, p.tk.line, p.tk.col, NULL};
        fail_at(&p, &eof_tok, PARSE_INCOMPLETE, "unterminated quoted string");
    }

    AstParseResult result;
    memset(&result, 0, sizeof(result));
    result.status = p.status;
    result.root = (p.status == PARSE_OK) ? root : NULL;
    result.message = p.message;
    result.line = p.err_line;
    result.col = p.err_col;
    tokenizer_destroy(&p.tk);
    return result;
}
