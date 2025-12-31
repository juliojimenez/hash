#ifndef REDIRECT_H
#define REDIRECT_H

// Redirection types
typedef enum {
    REDIR_NONE,
    REDIR_INPUT,       // < file
    REDIR_OUTPUT,      // > file
    REDIR_APPEND,      // >> file
    REDIR_ERROR,       // 2> file
    REDIR_ERROR_APPEND,// 2>> file
    REDIR_BOTH,        // &> file (stdout and stderr)
    REDIR_ERROR_TO_OUT // 2>&1 (stderr to stdout)
} RedirType;

// A single redirection
typedef struct {
    RedirType type;
    char *filename;  // File to redirect to/from (or NULL for 2>&1)
} Redirection;

// Redirection info for a command
typedef struct {
    Redirection *redirs;
    int count;
    char **args;  // Command args with redirections removed
} RedirInfo;

/**
 * Parse redirections from command arguments
 * Extracts redirection operators and filenames
 * Returns cleaned args array without redirections
 *
 * @param args Command arguments (will be modified)
 * @return RedirInfo structure (caller must free)
 */
RedirInfo *redirect_parse(char **args);

/**
 * Apply redirections before executing command
 * Opens files and sets up file descriptors
 *
 * @param info Redirection info
 * @return 0 on success, -1 on error
 */
int redirect_apply(const RedirInfo *info);

/**
 * Free redirection info
 *
 * @param info Redirection info to free
 */
void redirect_free(RedirInfo *info);

#endif // REDIRECT_H
