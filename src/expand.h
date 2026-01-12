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
 * - ~+ → $PWD (current working directory)
 * - ~- → $OLDPWD (previous working directory)
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

/**
 * Expand tilde in a variable assignment value
 * Handles both leading tilde and tildes after colons (for PATH-like values)
 *
 * Examples:
 * - ~/path → /home/user/path
 * - ~:foo → /home/user:foo
 * - foo:~ → foo:/home/user
 * - foo:~:bar → foo:/home/user:bar
 *
 * @param value The assignment value to expand
 * @return Newly allocated expanded string, or NULL if no expansion needed
 */
char *expand_tilde_in_assignment(const char *value);

/**
 * Check if a string contains glob characters
 *
 * @param s String to check
 * @return true if contains *, ?, or [...]
 */
int has_glob_chars(const char *s);

/**
 * Expand glob patterns (pathname expansion) in arguments
 * Modifies args array in place - may reallocate to accommodate expanded results
 *
 * Handles:
 * - * matches any string
 * - ? matches any single character
 * - [...] character class
 * - [!...] negated character class
 *
 * @param args Pointer to null-terminated argument array (may be reallocated)
 * @param arg_count Pointer to argument count (updated on expansion)
 * @return 0 on success, -1 on error
 */
int expand_glob(char ***args, int *arg_count);

/**
 * Remove \x01 quote markers from a string (in-place)
 * These markers protect quoted characters from expansion
 *
 * @param s String to strip markers from
 */
void strip_quote_markers(char *s);

/**
 * Remove \x01 quote markers from all arguments
 *
 * @param args Null-terminated argument array
 */
void strip_quote_markers_args(char **args);

#endif // EXPAND_H
