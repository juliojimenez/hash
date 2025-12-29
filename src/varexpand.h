#ifndef VAREXPAND_H
#define VAREXPAND_H

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

#endif // VAREXPAND_H

