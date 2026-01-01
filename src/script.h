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

    // For case
    char *case_word;        // Word being matched in case statement
    bool case_matched;      // Has a pattern matched?
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

#endif // SCRIPT_H
