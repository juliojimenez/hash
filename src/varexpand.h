#ifndef VAREXPAND_H
#define VAREXPAND_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * Expand environment variables in a string
 *
 * Supports:
 * - $VAR → value of VAR
 * - ${VAR} → value of VAR (safer for concatenation)
 * - $? → exit code of last command
 * - $$ → process ID
 * - $0 → shell name
 *
 * Returns newly allocated string (caller must free)
 * Returns NULL on allocation failure
 *
 * @param str String to expand
 * @param last_exit_code Exit code for $? expansion
 * @return Expanded string (newly allocated) or NULL
 */
char *varexpand_expand(const char *str, int last_exit_code);

/**
 * Expand variables in all arguments
 * Modifies args array in place (replaces with expanded versions)
 *
 * @param args Null-terminated argument array
 * @param last_exit_code Exit code for $? expansion
 * @return 0 on success, -1 on error
 */
int varexpand_args(char **args, int last_exit_code);

/**
 * Check if an unset variable error occurred during expansion
 * Used when set -u is enabled
 *
 * @return true if error occurred, false otherwise
 */
bool varexpand_had_error(void);

/**
 * The process ID that $$ expands to: the shell's own, recorded at startup, so
 * it stays the same inside pipelines, subshells, command substitutions and
 * background jobs, which fork. Before shell_pid_init() it is getpid().
 */
void shell_pid_init(void);
pid_t shell_pid(void);

/**
 * Clear the expansion error flag
 * Should be called before expansion when checking for errors
 */
void varexpand_clear_error(void);

#endif // VAREXPAND_H

