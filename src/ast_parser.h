#ifndef AST_PARSER_H
#define AST_PARSER_H

#include <stddef.h>
#include "arena.h"
#include "ast.h"

/**
 * Recursive-descent parser for the POSIX shell grammar. Builds the tree
 * described in ast.h; nothing is expanded or executed here.
 */

typedef enum {
    PARSE_OK,           /* root holds the program (possibly an empty list) */
    PARSE_INCOMPLETE,   /* input ended inside a construct: read more and parse again */
    PARSE_ERROR         /* syntax error at line/col; message says which token */
} ParseStatus;

typedef struct {
    ParseStatus status;
    Node *root;         /* NODE_LIST when status is PARSE_OK, else NULL */
    const char *message;/* Arena string, NULL when PARSE_OK */
    size_t line;        /* Position of the offending token (0-based) */
    size_t col;
} AstParseResult;

/**
 * Parse a complete program. src must be NUL-terminated. Every node and string
 * in the result is allocated from arena; the caller owns the arena.
 *
 * Here-document bodies are captured from the lines that follow the command
 * and stored on the redirection, so the tree is self-contained.
 */
AstParseResult parse_program(const char *src, Arena *arena);

#endif
