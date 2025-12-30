#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h>

#define MAX_COMPLETIONS 256
#define MAX_COMPLETION_LENGTH 1024

// Completion result
typedef struct {
    char **matches;      // Array of matching strings
    int count;           // Number of matches
    char *common_prefix; // Common prefix of all matches
} CompletionResult;

/**
 * Initialize completion system
 */
void completion_init(void);

/**
 * Generate completions for the current input
 *
 * Completes:
 * - Commands (first word, from PATH + built-ins + aliases)
 * - Files/directories (subsequent words)
 * - Aliases
 * - Environment variables (after $)
 *
 * @param line Current input line
 * @param pos Cursor position in line
 * @return CompletionResult (caller must free with completion_free_result)
 */
CompletionResult *completion_generate(const char *line, size_t pos);

/**
 * Free completion result
 *
 * @param result Result to free
 */
void completion_free_result(CompletionResult *result);

/**
 * Find common prefix of all matches
 * Returns newly allocated string (caller must free)
 *
 * @param matches Array of strings
 * @param count Number of strings
 * @return Common prefix, or NULL if none
 */
char *completion_common_prefix(char **matches, int count);

#endif // COMPLETION_H
