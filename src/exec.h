#ifndef EXEC_H
#define EXEC_H

#include <stdbool.h>
#include "ast.h"

/**
 * Tree executor: runs the AST produced by parse_program() on top of the
 * existing back end (expansion, redirection, builtins, process launch, jobs).
 * Control flow (lists, and-or, pipelines, loops, conditionals, case,
 * functions, subshells, background jobs) is interpreted here; each simple
 * command is handed to execute() once its redirections are in place.
 */

typedef enum {
    EXEC_OK = 0,    /* Keep going; exit status is in last_command_exit_code */
    EXEC_BREAK,     /* break pending (levels in script_get_break_pending()) */
    EXEC_CONTINUE,  /* continue pending */
    EXEC_RETURN,    /* return from the current function */
    EXEC_EXIT       /* The shell must exit: exit builtin, errexit, fatal error */
} ExecStatus;

/**
 * Run a tree. Nodes may be executed many times (loop bodies, functions).
 */
ExecStatus exec_node(const Node *node);

/**
 * Parse src and run it. Syntax errors are reported on stderr with exit
 * status 2. When incomplete is non-NULL it receives whether the input ended
 * inside a construct (nothing runs in that case); when it is NULL an
 * incomplete program is reported as a syntax error.
 */
ExecStatus exec_string(const char *src, bool *incomplete);

#endif
