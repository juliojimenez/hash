#ifndef CMDSUB_H
#define CMDSUB_H

/**
 * Expand command substitutions in a string
 *
 * Supports:
 * - $(command) → output of command
 * - `command` → output of command (backtick syntax)
 *
 * Returns newly allocated string (caller must free)
 * Returns NULL on allocation failure
 * If no substitution needed, returns NULL (caller should use original)
 *
 * @param str String to expand
 * @return Expanded string (newly allocated) or NULL
 */
char *cmdsub_expand(const char *str);

/**
 * Expand command substitutions in all arguments
 * Modifies args array in place (replaces with expanded versions)
 *
 * @param args Null-terminated argument array
 * @return 0 on success, -1 on error
 */
int cmdsub_args(char **args);

/**
 * Get the exit code from the last command substitution
 *
 * @return Exit code from last command substitution
 */
int cmdsub_get_last_exit_code(void);

/**
 * Reset the command substitution exit code tracker
 * Call this before processing a new command line
 */
void cmdsub_reset_exit_code(void);

#endif // CMDSUB_H
