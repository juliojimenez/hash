#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// SHELL SCRIPT STATE
// ============================================================================
//
// Shared state behind the tree executor (exec.c):
// - Shell functions: the table, definition and lookup
// - Positional parameters ($1, $2, ...)
// - The loop context stack and the break/continue/return signals
// - Entry points that run a file or a string (scripts, -c, eval, source)
// - Case word and pattern expansion helpers
//
// ============================================================================

#define MAX_SCRIPT_DEPTH 64      // Maximum nesting depth for control structures
#define MAX_FUNC_NAME 128        // Maximum function name length
#define MAX_FUNC_BODY 8192       // Maximum function body size
#define MAX_FUNCTIONS 256        // Maximum number of defined functions
#define MAX_SCRIPT_LINE 4096     // Maximum line length in scripts

// ============================================================================
// Script Context - Tracks execution state
// ============================================================================

typedef enum {
    CTX_NONE,
    CTX_IF,             // Inside if block
    CTX_ELIF,           // Inside elif block
    CTX_ELSE,           // Inside else block
    CTX_FOR,            // Inside for loop
    CTX_WHILE,          // Inside while loop
    CTX_UNTIL,          // Inside until loop
    CTX_CASE,           // Inside case statement
    CTX_FUNCTION        // Inside function definition
} ContextType;

typedef struct {
    ContextType type;
    int function_call_depth; // Function call depth when this context was created
                             // (break/continue are lexically scoped to it)
} ScriptContext;

// ============================================================================
// Function Definition
// ============================================================================

typedef struct {
    char name[MAX_FUNC_NAME];
    char *body;             // Function body (commands)
    size_t body_len;
    size_t def_line;        // Input line of the definition, for $LINENO in the body (0 = unknown)
} ShellFunction;

// ============================================================================
// Script State - Global state for script execution
// ============================================================================

typedef struct {
    // Context stack for nested control structures
    ScriptContext context_stack[MAX_SCRIPT_DEPTH];
    int context_depth;

    // Function definitions
    ShellFunction functions[MAX_FUNCTIONS];
    int function_count;

    // Script execution state
    bool in_script;         // Are we executing a script file?
    const char *script_path; // Current script path (for error messages)
    int script_line;        // Current line number
    bool silent_errors;     // Suppress errors (for system startup files)

    // Positional parameters ($1, $2, etc.)
    char **positional_params;
    int positional_count;

    // Function call tracking for lexical scoping
    int function_call_depth; // Current depth of function calls (0 = not in function)

    // Exit tracking
    bool exit_requested;     // True if exit was explicitly called

    // History recording: true only while executing the script the shell was
    // started on with -i (hash-shell -i script). Sourced files, startup files,
    // eval and loop/function bodies never record into history.
    bool record_history;
} ScriptState;

extern ScriptState script_state;

// ============================================================================
// Initialization and Cleanup
// ============================================================================

/**
 * Initialize the scripting subsystem
 */
void script_init(void);

/**
 * Clean up the scripting subsystem
 */
void script_cleanup(void);

// ============================================================================
// Script Execution
// ============================================================================

/**
 * Execute a script file
 *
 * @param filepath Path to the script file
 * @param argc Number of arguments passed to script
 * @param argv Arguments passed to script ($0, $1, $2, ...)
 * @return Exit code of the script
 */
int script_execute_file(const char *filepath, int argc, char **argv);

/**
 * Execute a script file with extended options
 *
 * @param filepath Path to the script file
 * @param argc Number of arguments passed to script
 * @param argv Arguments passed to script ($0, $1, $2, ...)
 * @param silent_errors If true, suppress error messages (for system files)
 * @return Exit code of the script
 */
int script_execute_file_ex(const char *filepath, int argc, char **argv, bool silent_errors);

/**
 * Execute a string as a script (for -c option or eval)
 *
 * @param script Script content to execute
 * @return Exit code
 */
int script_execute_string(const char *script);

// ============================================================================
// Control Structure Helpers
// ============================================================================

/**
 * Count loops in the context stack (for POSIX dynamic scoping)
 * Returns the number of for/while/until loops that can be broken out of
 * POSIX requires break/continue to work across function boundaries
 */
int script_count_loops_at_current_depth(void);

/**
 * Get/set break pending levels (for POSIX dynamic scoping)
 * When break is called from a function, these track how many levels to break
 */
int script_get_break_pending(void);
void script_set_break_pending(int levels);

/**
 * Get/set continue pending levels (for POSIX dynamic scoping)
 */
int script_get_continue_pending(void);
void script_set_continue_pending(int levels);

/**
 * Reset script state for subshell (POSIX: break/continue only affect loops
 * within the current execution environment, not parent's loops)
 */
void script_reset_for_subshell(void);

void script_set_return_pending(bool pending);

/**
 * Push a new context onto the stack
 */
int script_push_context(ContextType type);

/**
 * Pop context from the stack
 */
int script_pop_context(void);

// ============================================================================
// Function Management
// ============================================================================

/**
 * Define a new function
 *
 * @param name Function name
 * @param body Function body (commands)
 * @return 0 on success, -1 on error
 */
int script_define_function(const char *name, const char *body);

/**
 * Look up a function by name
 *
 * @param name Function name
 * @return Pointer to function, or NULL if not found
 */
ShellFunction *script_get_function(const char *name);

/**
 * Remove a function (unset -f). A body still running is kept until the
 * outermost function call returns.
 *
 * @return 0 when removed, 1 when there was no such function
 */
int script_undefine_function(const char *name);

/**
 * Free the bodies of functions removed while a function was running; a
 * no-op while one still is.
 */
void script_free_retired_functions(void);

// ============================================================================
// Test/Condition Evaluation
// ============================================================================

// ============================================================================
// Keyword Detection
// ============================================================================

// ============================================================================
// Line Classification
// ============================================================================

typedef enum {
    LINE_EMPTY,             // Empty or comment-only line
    LINE_SIMPLE,            // Simple command
    LINE_IF_START,          // if ...
    LINE_THEN,              // then
    LINE_ELIF,              // elif ...
    LINE_ELSE,              // else
    LINE_FI,                // fi
    LINE_FOR_START,         // for var in ...
    LINE_WHILE_START,       // while ...
    LINE_UNTIL_START,       // until ...
    LINE_DO,                // do
    LINE_DONE,              // done
    LINE_CASE_START,        // case word in
    LINE_CASE_PATTERN,      // pattern)
    LINE_CASE_END,          // ;;
    LINE_ESAC,              // esac
    LINE_FUNCTION_START,    // name() or function name
    LINE_LBRACE,            // {
    LINE_RBRACE,            // }
    LINE_UNKNOWN
} LineType;

/**
 * Get a positional parameter value
 *
 * @param index Parameter index (0 for $0, 1 for $1, etc.)
 * @return Parameter value or NULL if not set
 */
const char *script_get_positional_param(int index);

/**
 * Set positional parameters ($1, $2, etc.) from set builtin
 * Note: $0 is preserved, only $1 onwards are replaced
 *
 * @param argc Number of parameters to set
 * @param argv Array of parameter values (note: these are copied, not owned)
 */
void script_set_positional_params(int argc, char **argv);

/**
 * Track whether we're in a condition context (if/while/until condition)
 * where errexit should not trigger
 */
void script_set_in_condition(bool in_condition);
bool script_get_in_condition(void);

/**
 * Expand the word of a case statement (parameters, command substitution,
 * quote removal; no field splitting or pathname expansion). Caller frees.
 */
char *script_expand_case_word(const char *word);

/**
 * Expand a case pattern: expansions happen, but expanded characters are
 * treated literally so they do not become pattern operators. Caller frees.
 */
char *script_expand_case_pattern(const char *pattern);

#endif // SCRIPT_H
