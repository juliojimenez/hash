#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// POSIX SHELL SCRIPTING SUPPORT
// ============================================================================
//
// This module implements POSIX-compatible shell scripting features:
// - Control structures: if/then/elif/else/fi, for/do/done, while/do/done, case/esac
// - Test expressions: [ ] and test builtin
// - Functions
// - Script file execution
//
// ============================================================================

#define MAX_SCRIPT_DEPTH 64      // Maximum nesting depth for control structures
#define MAX_FUNC_NAME 128        // Maximum function name length
#define MAX_FUNC_BODY 8192       // Maximum function body size
#define MAX_FUNCTIONS 256        // Maximum number of defined functions
#define MAX_SCRIPT_LINE 4096     // Maximum line length in scripts

// ============================================================================
// Token Types for Script Parsing
// ============================================================================

typedef enum {
    TOK_WORD,           // Regular word/command
    TOK_NEWLINE,        // Newline (command separator)
    TOK_SEMI,           // ;
    TOK_AND,            // &&
    TOK_OR,             // ||
    TOK_PIPE,           // |
    TOK_AMP,            // & (background)
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_LBRACE,         // {
    TOK_RBRACE,         // }

    // Control structure keywords
    TOK_IF,
    TOK_THEN,
    TOK_ELIF,
    TOK_ELSE,
    TOK_FI,
    TOK_FOR,
    TOK_WHILE,
    TOK_UNTIL,
    TOK_DO,
    TOK_DONE,
    TOK_CASE,
    TOK_ESAC,
    TOK_IN,

    // Other keywords
    TOK_FUNCTION,       // function keyword (bash extension)

    TOK_EOF,
    TOK_ERROR
} TokenType;

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
    bool condition_met;     // For if/elif - was a condition already true?
    bool should_execute;    // Should commands in this block execute?

    // For loops
    char *loop_var;         // Variable name for for loops
    char **loop_values;     // Values to iterate over
    int loop_index;         // Current index in loop
    int loop_count;         // Total number of values
    long loop_body_start;   // File position of loop body start
    char *loop_body;        // Buffered loop body for replay
    size_t loop_body_len;   // Current length of loop body
    size_t loop_body_cap;   // Capacity of loop body buffer
    bool collecting_body;   // Are we currently collecting loop body?
    int body_nesting_depth; // Nesting depth of nested loops during collection
    char *loop_condition;   // Condition for while/until loops

    // For case
    char *case_word;        // Word being matched in case statement
    bool case_matched;      // Has a pattern matched?

    // For function definitions
    char *func_name;        // Function name being defined
    char *func_body;        // Function body being collected
    size_t func_body_len;   // Current length of body
    size_t func_body_cap;   // Capacity of body buffer
    int brace_depth;        // Brace nesting depth

    // For lexical scoping of break/continue
    int function_call_depth; // Function call depth when this context was created
} ScriptContext;

// ============================================================================
// Function Definition
// ============================================================================

typedef struct {
    char name[MAX_FUNC_NAME];
    char *body;             // Function body (commands)
    size_t body_len;
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

/**
 * Process a single line of script
 * This handles control structures and can buffer incomplete statements
 *
 * @param line Line to process
 * @return 0 to continue, negative on error, positive for exit
 */
int script_process_line(const char *line);

// ============================================================================
// Control Structure Helpers
// ============================================================================

/**
 * Check if we're currently inside a control structure
 */
bool script_in_control_structure(void);

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
 * Clear break/continue pending state (call when fully handled)
 */
void script_clear_break_continue(void);

/**
 * Reset script state for subshell (POSIX: break/continue only affect loops
 * within the current execution environment, not parent's loops)
 */
void script_reset_for_subshell(void);

/**
 * Get/set return pending flag (for return in if/while conditions)
 * When return is called in a condition, this flag signals that
 * the function should return immediately
 */
bool script_get_return_pending(void);
void script_set_return_pending(bool pending);

/**
 * Check if commands should currently execute
 * (Based on if/else conditions, etc.)
 */
bool script_should_execute(void);

/**
 * Push a new context onto the stack
 */
int script_push_context(ContextType type);

/**
 * Pop context from the stack
 */
int script_pop_context(void);

/**
 * Get current context type
 */
ContextType script_current_context(void);

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
 * Execute a function
 *
 * @param func Function to execute
 * @param argc Number of arguments
 * @param argv Arguments ($1, $2, ...)
 * @return Exit code of function
 */
int script_execute_function(const ShellFunction *func, int argc, char **argv);

// ============================================================================
// Test/Condition Evaluation
// ============================================================================

/**
 * Evaluate a test expression (for [ ] or test builtin)
 *
 * @param args Arguments to test (excluding [ and ])
 * @param argc Number of arguments
 * @return 0 if true, 1 if false, 2 on error
 */
int script_eval_test(char **args, int argc);

/**
 * Evaluate a condition for if/while/until
 * Runs the command and returns based on exit code
 *
 * @param condition Command to execute
 * @return true if exit code is 0
 */
bool script_eval_condition(const char *condition);

// ============================================================================
// Keyword Detection
// ============================================================================

/**
 * Check if a word is a shell keyword
 */
bool script_is_keyword(const char *word);

/**
 * Get the token type for a word
 */
TokenType script_get_keyword_type(const char *word);

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
 * Classify a line to determine how to handle it
 */
LineType script_classify_line(const char *line);

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
 * Get the pending heredoc content
 * Used during command execution to pass heredoc content to stdin
 *
 * @return Heredoc content string, or NULL if none pending
 */
const char *script_get_pending_heredoc(void);

/**
 * Get whether the pending heredoc delimiter was quoted
 * If quoted, no expansion should happen on the heredoc content
 *
 * @return 1 if delimiter was quoted, 0 otherwise
 */
int script_get_pending_heredoc_quoted(void);

/**
 * Track whether we're in a condition context (if/while/until condition)
 * where errexit should not trigger
 */
void script_set_in_condition(bool in_condition);
bool script_get_in_condition(void);

#endif // SCRIPT_H
