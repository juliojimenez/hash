#ifndef EXPAND_H
#define EXPAND_H

/**
 * Expand tilde (~) in arguments
 * Modifies args array in place (frees old strings, allocates new ones)
 *
 * Handles:
 * - ~ → /home/user
 * - ~/path → /home/user/path
 * - ~user → /home/user (if user exists)
 * - ~user/path → /home/user/path
 *
 * @param args Null-terminated argument array
 * @return 0 on success, -1 on error
 */
int expand_tilde(char **args);

/**
 * Expand a single path with tilde
 * Returns newly allocated string (caller must free)
 * Returns NULL on error or if no expansion needed
 *
 * @param path Path to expand
 * @return Expanded path (newly allocated) or NULL
 */
char *expand_tilde_path(const char *path);

#endif // EXPAND_H
