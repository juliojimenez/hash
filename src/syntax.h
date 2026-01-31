#ifndef SYNTAX_H
#define SYNTAX_H

#include <stddef.h>
#include <stdbool.h>

// Token types for syntax highlighting
typedef enum {
    SYN_NONE,           // Plain text / whitespace
    SYN_COMMAND,        // Valid external command
    SYN_BUILTIN,        // Builtin command
    SYN_ALIAS,          // Alias
    SYN_INVALID_CMD,    // Invalid/unknown command
    SYN_ARGUMENT,       // Command argument
    SYN_STRING_SINGLE,  // 'single quoted'
    SYN_STRING_DOUBLE,  // "double quoted"
    SYN_VARIABLE,       // $VAR, ${VAR}, $()
    SYN_OPERATOR,       // |, &&, ||, ;, &
    SYN_REDIRECT,       // >, <, >>, 2>, etc.
    SYN_COMMENT,        // # comment
    SYN_GLOB,           // *, ?, [...]
} SyntaxTokenType;

// A syntax-highlighted segment
typedef struct {
    size_t start;           // Start position in input
    size_t end;             // End position (exclusive)
    SyntaxTokenType type;   // Token type
} SyntaxSegment;

// Result of syntax analysis
typedef struct {
    SyntaxSegment *segments;
    int count;
    int capacity;
} SyntaxResult;

// Analyze input and return syntax segments
// Caller must call syntax_result_free()
SyntaxResult *syntax_analyze(const char *input, size_t len);

// Free syntax result
void syntax_result_free(SyntaxResult *result);

// Render input with syntax highlighting
// Returns newly allocated string with ANSI codes
// Caller must free the returned string
char *syntax_render(const char *input, size_t len);

// Check if a command name is valid (exists in PATH, is builtin, or is alias)
// Returns: 0 = invalid, 1 = external command, 2 = builtin, 3 = alias
int syntax_check_command(const char *cmd);

// Initialize syntax highlighting (call once at startup)
void syntax_init(void);

// Clear command cache (call when PATH changes)
void syntax_cache_clear(void);

#endif // SYNTAX_H
