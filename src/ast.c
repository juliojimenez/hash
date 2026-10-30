#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

// ============================================================================
// Nodes and vectors
// ============================================================================

Node *ast_node_new(Arena *arena, NodeType type, size_t line, size_t col) {
    Node *node = arena_alloc(arena, sizeof(Node));
    if (!node) {
        return NULL;
    }
    memset(node, 0, sizeof(Node));
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

// Make room for one more element; the arena cannot realloc, so grow by copying
static bool grow(Arena *arena, void **items, size_t *cap, size_t count, size_t elem_size) {
    if (count < *cap) {
        return true;
    }
    size_t new_cap = *cap ? *cap * 2 : 4;
    void *fresh = arena_alloc(arena, new_cap * elem_size);
    if (!fresh) {
        return false;
    }
    if (count > 0) {
        memcpy(fresh, *items, count * elem_size);
    }
    *items = fresh;
    *cap = new_cap;
    return true;
}

bool ast_words_push(Arena *arena, WordVec *vec, char *word) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(char *))) {
        return false;
    }
    vec->items[vec->count++] = word;
    return true;
}

bool ast_nodes_push(Arena *arena, NodeVec *vec, Node *node) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(Node *))) {
        return false;
    }
    vec->items[vec->count++] = node;
    return true;
}

bool ast_list_push(Arena *arena, ListItemVec *vec, Node *node, bool async) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(ListItem))) {
        return false;
    }
    vec->items[vec->count].node = node;
    vec->items[vec->count].async = async;
    vec->count++;
    return true;
}

bool ast_redirs_push(Arena *arena, RedirVec *vec, Redirect redir) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(Redirect))) {
        return false;
    }
    vec->items[vec->count++] = redir;
    return true;
}

bool ast_if_clauses_push(Arena *arena, IfClauseVec *vec, Node *condition, Node *body) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(IfClause))) {
        return false;
    }
    vec->items[vec->count].condition = condition;
    vec->items[vec->count].body = body;
    vec->count++;
    return true;
}

bool ast_case_items_push(Arena *arena, CaseItemVec *vec, CaseItem item) {
    if (!grow(arena, (void **)&vec->items, &vec->cap, vec->count, sizeof(CaseItem))) {
        return false;
    }
    vec->items[vec->count++] = item;
    return true;
}

const char *ast_redirect_op_name(TokenType op) {
    switch (op) {
        case TOKEN_LESS:      return "<";
        case TOKEN_GREAT:     return ">";
        case TOKEN_DGREAT:    return ">>";
        case TOKEN_DLESS:     return "<<";
        case TOKEN_DLESSDASH: return "<<-";
        case TOKEN_LESSAND:   return "<&";
        case TOKEN_GREATAND:  return ">&";
        case TOKEN_LESSGREAT: return "<>";
        default:              return "?";
    }
}

// ============================================================================
// S-expression rendering
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool failed;
} StrBuf;

static void sb_put(StrBuf *sb, const char *s, size_t n) {
    if (sb->failed) {
        return;
    }
    if (sb->len + n + 1 > sb->cap) {
        size_t new_cap = sb->cap ? sb->cap * 2 : 256;
        while (new_cap < sb->len + n + 1) {
            new_cap *= 2;
        }
        char *fresh = realloc(sb->data, new_cap);
        if (!fresh) {
            sb->failed = true;
            return;
        }
        sb->data = fresh;
        sb->cap = new_cap;
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_puts(StrBuf *sb, const char *s) {
    sb_put(sb, s, strlen(s));
}

static void sb_putc(StrBuf *sb, char c) {
    sb_put(sb, &c, 1);
}

static void sb_quote(StrBuf *sb, const char *s) {
    sb_putc(sb, '"');
    for (; *s; s++) {
        switch (*s) {
            case '"':  sb_puts(sb, "\\\""); break;
            case '\\': sb_puts(sb, "\\\\"); break;
            case '\n': sb_puts(sb, "\\n"); break;
            case '\t': sb_puts(sb, "\\t"); break;
            default:   sb_putc(sb, *s); break;
        }
    }
    sb_putc(sb, '"');
}

static void sx_node(StrBuf *sb, const Node *node);

static void sx_redirs(StrBuf *sb, const RedirVec *redirs) {
    for (size_t i = 0; i < redirs->count; i++) {
        const Redirect *r = &redirs->items[i];
        char fd[16];
        snprintf(fd, sizeof(fd), "%d", r->fd);
        if (r->op == TOKEN_DLESS || r->op == TOKEN_DLESSDASH) {
            sb_puts(sb, " (heredoc ");
            sb_puts(sb, fd);
            sb_putc(sb, ' ');
            sb_puts(sb, ast_redirect_op_name(r->op));
            if (r->heredoc_quoted) {
                sb_puts(sb, " quoted");
            }
            sb_putc(sb, ' ');
            sb_quote(sb, r->target);
            sb_putc(sb, ' ');
            sb_quote(sb, r->heredoc_body ? r->heredoc_body : "");
            sb_putc(sb, ')');
        } else {
            sb_puts(sb, " (redir ");
            sb_puts(sb, fd);
            sb_putc(sb, ' ');
            sb_puts(sb, ast_redirect_op_name(r->op));
            sb_putc(sb, ' ');
            sb_quote(sb, r->target);
            sb_putc(sb, ')');
        }
    }
}

static void sx_child(StrBuf *sb, const Node *node) {
    sb_putc(sb, ' ');
    sx_node(sb, node);
}

static void sx_list(StrBuf *sb, const Node *node) {
    sb_puts(sb, "(list");
    for (size_t i = 0; i < node->u.list.items.count; i++) {
        const ListItem *item = &node->u.list.items.items[i];
        if (item->async) {
            sb_puts(sb, " (bg ");
            sx_node(sb, item->node);
            sb_putc(sb, ')');
        } else {
            sx_child(sb, item->node);
        }
    }
    sb_putc(sb, ')');
}

static void sx_pipeline(StrBuf *sb, const Node *node) {
    if (node->u.pipeline.negated) {
        sb_puts(sb, "(not ");
    }
    if (node->u.pipeline.commands.count == 1) {
        sx_node(sb, node->u.pipeline.commands.items[0]);
    } else {
        sb_puts(sb, "(pipe");
        for (size_t i = 0; i < node->u.pipeline.commands.count; i++) {
            sx_child(sb, node->u.pipeline.commands.items[i]);
        }
        sb_putc(sb, ')');
    }
    if (node->u.pipeline.negated) {
        sb_putc(sb, ')');
    }
}

static void sx_simple(StrBuf *sb, const Node *node) {
    sb_puts(sb, "(cmd");
    for (size_t i = 0; i < node->u.simple.assignments.count; i++) {
        sb_puts(sb, " (assign ");
        sb_quote(sb, node->u.simple.assignments.items[i]);
        sb_putc(sb, ')');
    }
    for (size_t i = 0; i < node->u.simple.words.count; i++) {
        sb_putc(sb, ' ');
        sb_quote(sb, node->u.simple.words.items[i]);
    }
    sx_redirs(sb, &node->redirs);
    sb_putc(sb, ')');
}

static void sx_if(StrBuf *sb, const Node *node) {
    sb_puts(sb, "(if");
    for (size_t i = 0; i < node->u.if_clause.clauses.count; i++) {
        const IfClause *c = &node->u.if_clause.clauses.items[i];
        if (i > 0) {
            sb_puts(sb, " (elif");
        }
        sx_child(sb, c->condition);
        sx_child(sb, c->body);
        if (i > 0) {
            sb_putc(sb, ')');
        }
    }
    if (node->u.if_clause.else_body) {
        sb_puts(sb, " (else");
        sx_child(sb, node->u.if_clause.else_body);
        sb_putc(sb, ')');
    }
    sx_redirs(sb, &node->redirs);
    sb_putc(sb, ')');
}

static void sx_for(StrBuf *sb, const Node *node) {
    sb_puts(sb, "(for ");
    sb_quote(sb, node->u.for_clause.var);
    if (node->u.for_clause.has_words) {
        sb_puts(sb, " (words");
        for (size_t i = 0; i < node->u.for_clause.words.count; i++) {
            sb_putc(sb, ' ');
            sb_quote(sb, node->u.for_clause.words.items[i]);
        }
        sb_putc(sb, ')');
    }
    sx_child(sb, node->u.for_clause.body);
    sx_redirs(sb, &node->redirs);
    sb_putc(sb, ')');
}

static void sx_case(StrBuf *sb, const Node *node) {
    sb_puts(sb, "(case ");
    sb_quote(sb, node->u.case_clause.word);
    for (size_t i = 0; i < node->u.case_clause.items.count; i++) {
        const CaseItem *item = &node->u.case_clause.items.items[i];
        sb_puts(sb, " (item (pat");
        for (size_t j = 0; j < item->patterns.count; j++) {
            sb_putc(sb, ' ');
            sb_quote(sb, item->patterns.items[j]);
        }
        sb_putc(sb, ')');
        sx_child(sb, item->body);
        sb_putc(sb, ')');
    }
    sx_redirs(sb, &node->redirs);
    sb_putc(sb, ')');
}

static void sx_node(StrBuf *sb, const Node *node) {
    if (!node) {
        sb_puts(sb, "nil");
        return;
    }
    switch (node->type) {
        case NODE_LIST:
            sx_list(sb, node);
            break;
        case NODE_AND_OR:
            sb_puts(sb, node->u.and_or.op == TOKEN_AND_IF ? "(and" : "(or");
            sx_child(sb, node->u.and_or.left);
            sx_child(sb, node->u.and_or.right);
            sb_putc(sb, ')');
            break;
        case NODE_PIPELINE:
            sx_pipeline(sb, node);
            break;
        case NODE_SIMPLE:
            sx_simple(sb, node);
            break;
        case NODE_SUBSHELL:
        case NODE_BRACE:
            sb_puts(sb, node->type == NODE_SUBSHELL ? "(subshell" : "(brace");
            sx_child(sb, node->u.group.body);
            sx_redirs(sb, &node->redirs);
            sb_putc(sb, ')');
            break;
        case NODE_IF:
            sx_if(sb, node);
            break;
        case NODE_FOR:
            sx_for(sb, node);
            break;
        case NODE_WHILE:
        case NODE_UNTIL:
            sb_puts(sb, node->type == NODE_WHILE ? "(while" : "(until");
            sx_child(sb, node->u.loop.condition);
            sx_child(sb, node->u.loop.body);
            sx_redirs(sb, &node->redirs);
            sb_putc(sb, ')');
            break;
        case NODE_CASE:
            sx_case(sb, node);
            break;
        case NODE_FUNCTION:
            sb_puts(sb, "(function ");
            sb_quote(sb, node->u.function.name);
            sx_child(sb, node->u.function.body);
            sb_putc(sb, ')');
            break;
    }
}

char *ast_sexpr(const Node *node, Arena *arena) {
    StrBuf sb = {0};
    sx_node(&sb, node);
    char *result = NULL;
    if (!sb.failed && sb.data) {
        result = arena_strdup(arena, sb.data);
    }
    free(sb.data);
    return result;
}
