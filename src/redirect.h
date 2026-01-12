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
    REDIR_ERROR_TO_OUT, // 2>&1 (stderr to stdout)
    REDIR_OUT_TO_ERROR, // >&2 or 1>&2 (stdout to stderr)
    REDIR_HEREDOC,     // << DELIMITER
    REDIR_HEREDOC_NOTAB, // <<- DELIMITER (strip leading tabs)
    REDIR_INPUT_DUP,   // <&N (dup fd N to stdin)
    REDIR_OUTPUT_DUP,  // >&N (dup fd N to stdout) - for N != 2
    REDIR_FD_DUP       // N>&M (dup fd M to fd N)
} RedirType;

// A single redirection
typedef struct {
    RedirType type;
    char *filename;       // File to redirect to/from (or NULL for 2>&1)
    char *heredoc_delim;  // Heredoc delimiter (for <<)
    char *heredoc_content; // Heredoc content (collected after parsing)
    int heredoc_quoted;   // 1 if delimiter was quoted (no expansion)
    int dest_fd;          // Destination FD for REDIR_FD_DUP (e.g., 2 in 2>&9)
    int src_fd;           // Source FD for REDIR_FD_DUP (e.g., 9 in 2>&9)
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

/**
 * Check if a line contains a heredoc operator
 *
 * @param line The command line to check
 * @return 1 if heredoc found, 0 otherwise
 */
int redirect_has_heredoc(const char *line);

/**
 * Extract heredoc delimiter from a line
 *
 * @param line The command line
 * @param strip_tabs Output: set to 1 if <<- was used
 * @param quoted Output: set to 1 if delimiter was quoted (no expansion)
 * @return Allocated string with delimiter, or NULL if none found
 */
char *redirect_get_heredoc_delim(const char *line, int *strip_tabs, int *quoted);

/**
 * Set heredoc content for a redirection info
 *
 * @param info Redirection info
 * @param content The heredoc content
 * @param quoted 1 if delimiter was quoted (no expansion), 0 otherwise
 */
void redirect_set_heredoc_content(RedirInfo *info, const char *content, int quoted);

#endif // REDIRECT_H
