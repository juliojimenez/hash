#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include "arena.h"
#include "tokenizer.h"

/**
 * Abstract syntax tree for the POSIX shell grammar, produced by
 * parse_program() (ast_parser.h). Every node and string lives in the arena
 * handed to the parser, so a whole tree is released with one arena_reset().
 */

typedef enum {
    NODE_LIST,          /* and_or items separated by ; & or newline */
    NODE_AND_OR,        /* left && right, left || right */
    NODE_PIPELINE,      /* commands joined by |, optionally negated with ! */
    NODE_SIMPLE,        /* assignments, words, redirections */
    NODE_SUBSHELL,      /* ( list ) */
    NODE_BRACE,         /* { list; } */
    NODE_IF,
    NODE_FOR,
    NODE_WHILE,
    NODE_UNTIL,
    NODE_CASE,
    NODE_FUNCTION       /* name() compound_command */
} NodeType;

typedef struct Node Node;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} WordVec;

typedef struct {
    Node **items;
    size_t count;
    size_t cap;
} NodeVec;

typedef struct {
    Node *node;
    bool async;         /* terminated by & */
} ListItem;

typedef struct {
    ListItem *items;
    size_t count;
    size_t cap;
} ListItemVec;

typedef struct {
    int fd;             /* Effective descriptor: defaults applied (0 for input, 1 for output) */
    TokenType op;       /* TOKEN_LESS, TOKEN_GREAT, TOKEN_DGREAT, TOKEN_DLESS, TOKEN_DLESSDASH,
                           TOKEN_LESSAND, TOKEN_GREATAND, TOKEN_LESSGREAT */
    char *target;       /* File name, descriptor word, or heredoc delimiter (quotes removed) */
    char *heredoc_body; /* NULL unless op is << or <<-; tabs already stripped for <<- */
    bool heredoc_quoted;/* Delimiter was quoted: the body is taken literally */
} Redirect;

typedef struct {
    Redirect *items;
    size_t count;
    size_t cap;
} RedirVec;

typedef struct {
    Node *condition;    /* NODE_LIST */
    Node *body;         /* NODE_LIST */
} IfClause;

typedef struct {
    IfClause *items;
    size_t count;
    size_t cap;
} IfClauseVec;

typedef struct {
    WordVec patterns;
    Node *body;         /* NODE_LIST, possibly empty */
} CaseItem;

typedef struct {
    CaseItem *items;
    size_t count;
    size_t cap;
} CaseItemVec;

struct Node {
    NodeType type;
    size_t line;
    size_t col;
    RedirVec redirs;    /* Simple and compound commands */
    union {
        struct { ListItemVec items; } list;
        struct { Node *left; TokenType op; Node *right; } and_or;
        struct { NodeVec commands; bool negated; } pipeline;
        struct { WordVec assignments; WordVec words; } simple;
        struct { Node *body; } group;                          /* subshell, brace */
        struct { IfClauseVec clauses; Node *else_body; } if_clause;
        struct { char *var; bool has_words; WordVec words; Node *body; } for_clause;
        struct { Node *condition; Node *body; } loop;          /* while, until */
        struct { char *word; CaseItemVec items; } case_clause;
        struct { char *name; Node *body; } function;
    } u;
};

Node *ast_node_new(Arena *arena, NodeType type, size_t line, size_t col);

bool ast_words_push(Arena *arena, WordVec *vec, char *word);
bool ast_nodes_push(Arena *arena, NodeVec *vec, Node *node);
bool ast_list_push(Arena *arena, ListItemVec *vec, Node *node, bool async);
bool ast_redirs_push(Arena *arena, RedirVec *vec, Redirect redir);
bool ast_if_clauses_push(Arena *arena, IfClauseVec *vec, Node *condition, Node *body);
bool ast_case_items_push(Arena *arena, CaseItemVec *vec, CaseItem item);

/**
 * Render a tree as a compact S-expression, e.g. (list (cmd "echo" "hi")).
 * For tests and debugging. Returns an arena string, NULL on allocation failure.
 */
char *ast_sexpr(const Node *node, Arena *arena);

/**
 * Operator spelling for a redirection token type, e.g. ">>".
 */
const char *ast_redirect_op_name(TokenType op);

#endif
