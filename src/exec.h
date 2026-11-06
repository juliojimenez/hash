#ifndef EXEC_H
#define EXEC_H

#include "script.h"
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
    EXEC_EXIT,      /* The shell must exit: exit builtin, errexit, fatal error */
    EXEC_ABORT      /* The command is abandoned (nesting limit): every level unwinds,
                       the status is 1, an interactive shell shows the next prompt */
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

/**
 * Parse and run src, returning the exit status the way script_execute_string()
 * does. Input that ends inside a construct is a syntax error (status 2).
 */
int exec_string_status(const char *src);

/**
 * Run a shell function: the body is parsed and executed with argv as the
 * positional parameters. Called by execute() for a command that names a
 * function. Returns execute()'s convention (1 continue, 0 exit).
 */
int exec_run_function(const ShellFunction *func, int argc, char **argv);

/* Nesting guard: function calls, `.` and eval share one depth. FUNCNEST sets
   the limit (1000 when unset; 0 or a non-number means no depth limit), and a
   call that would leave too little C stack is refused as well. A refused call
   prints the message, sets status 1 and raises the abort that EXEC_ABORT
   carries to the top level. */
bool exec_nesting_enter(const char *name);
void exec_nesting_leave(void);
int exec_nesting_limit(void);
int exec_nesting_depth(void);
bool exec_abort_pending(void);
void exec_abort_clear(void);
/* Record where the stack starts (an address in main's frame), for the stack guard */
void exec_set_stack_base(void *base);

/**
 * Line feeder: accumulates lines until they form a complete program, then
 * runs it. Used by the prompt (which shows PS2 while input is pending), the
 * stdin loop and script files, so a command typed or read across several
 * lines runs once it is complete, including here-documents.
 */
typedef struct {
    char *text;
    size_t len;
    size_t cap;
    size_t first_line;  /* input line (1-based) where the pending text starts */
    size_t next_line;   /* input line the next fed line will have */
    /* Called with the complete text (lines joined with newlines) once the
       feeder has a whole program, right before it runs or its syntax error is
       reported; the prompt records history from it. NULL when unused. */
    void (*on_program)(const char *text, void *ctx);
    void *on_program_ctx;
} ExecFeeder;

typedef enum {
    FEED_NEED_MORE,     /* The text so far is incomplete; nothing ran */
    FEED_RAN,           /* A complete program ran; see the ExecStatus */
    FEED_SYNTAX_ERROR   /* Reported on stderr, status 2; pending text dropped */
} FeedResult;

void exec_feeder_init(ExecFeeder *feeder);
void exec_feeder_free(ExecFeeder *feeder);
void exec_feeder_reset(ExecFeeder *feeder);
bool exec_feeder_pending(const ExecFeeder *feeder);
FeedResult exec_feed_line(ExecFeeder *feeder, const char *line, ExecStatus *status);

/** Account for an input line that is not fed (the shebang line). */
void exec_feeder_skip_line(ExecFeeder *feeder);

/**
 * Where the text being run comes from, for messages: a script path, "-c",
 * "eval"; NULL while reading standard input or the prompt.
 */
void exec_set_source(const char *name);
const char *exec_source(void);

/** $LINENO: the input line of the command being run, and the same as text. */
size_t exec_current_line(void);
const char *exec_lineno_string(void);

/**
 * Print a syntax error for the current source: "hash: FILE: line N: message",
 * "hash: -c: line N: message", "hash: line N: message" on standard input, and
 * just "hash: message" at the prompt. Quiet while silent_errors is set.
 */
void exec_report_syntax_error(const char *message, size_t line);

#endif
