#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include "expand.h"
#include "safe_string.h"

// Get home directory for a user
static char *get_home_dir(const char *username) {
    static char home[PATH_MAX];

    if (!username || *username == '\0') {
        // Get current user's home
        const char *home_env = getenv("HOME");
        if (home_env) {
            safe_strcpy(home, home_env, sizeof(home));
            return home;
        }

        // Use getpwuid_r for thread safety
        struct passwd pw;
        struct passwd *result = NULL;
        char buf[1024];

        if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
            safe_strcpy(home, pw.pw_dir, sizeof(home));
            return home;
        }

        return NULL;
    }

    // Get specified user's home directory
    struct passwd pw;
    struct passwd *result = NULL;
    char buf[1024];

    if (getpwnam_r(username, &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
        safe_strcpy(home, pw.pw_dir, sizeof(home));
        return home;
    }

    return NULL;
}

// Expand a single path with tilde
char *expand_tilde_path(const char *path) {
    if (!path || path[0] != '~') {
        return NULL;  // No expansion needed
    }

    // Check for special tilde prefixes: ~+ (PWD) and ~- (OLDPWD)
    const char *slash = NULL;
    const char *expansion = NULL;

    if (path[1] == '+' && (path[2] == '/' || path[2] == '\0')) {
        // ~+ expands to PWD
        expansion = getenv("PWD");
        if (!expansion) {
            static char cwd[PATH_MAX];
            expansion = getcwd(cwd, sizeof(cwd));
        }
        slash = path[2] == '/' ? path + 2 : NULL;
    } else if (path[1] == '-' && (path[2] == '/' || path[2] == '\0')) {
        // ~- expands to OLDPWD
        expansion = getenv("OLDPWD");
        if (!expansion) {
            return NULL;  // OLDPWD not set
        }
        slash = path[2] == '/' ? path + 2 : NULL;
    } else {
        // Find where the username ends (at / or end of string)
        slash = strchr(path, '/');
        size_t username_len = slash ? (size_t)(slash - path - 1) : strlen(path) - 1;

        // Extract username (empty string for current user)
        char username[256] = {0};
        if (username_len > 0 && username_len < sizeof(username)) {
            memcpy(username, path + 1, username_len);
            username[username_len] = '\0';
        }

        // Get home directory for user
        expansion = get_home_dir(username[0] != '\0' ? username : NULL);
    }

    if (!expansion) {
        return NULL;  // Couldn't get expansion
    }

    // Build expanded path
    char *expanded = malloc(PATH_MAX);
    if (!expanded) {
        return NULL;
    }

    if (slash) {
        // ~/path, ~user/path, ~+/path, ~-/path
        snprintf(expanded, PATH_MAX, "%s%s", expansion, slash);
    } else {
        // Just ~, ~user, ~+, or ~-
        safe_strcpy(expanded, expansion, PATH_MAX);
    }

    return expanded;
}

// Expand tilde in a variable assignment value
// Handles both leading tilde and tildes after colons (for PATH-like values)
// Returns newly allocated string, or NULL if no expansion needed
char *expand_tilde_in_assignment(const char *value) {
    if (!value) return NULL;

    // Quick check: does it contain a tilde?
    if (!strchr(value, '~')) {
        return NULL;  // No expansion needed
    }

    // Allocate result buffer
    char *result = malloc(PATH_MAX);
    if (!result) return NULL;

    size_t result_pos = 0;
    const char *p = value;

    // Check for leading tilde
    if (*p == '~') {
        // Find end of tilde prefix (at / or : or end of string)
        const char *end = p + 1;
        while (*end && *end != '/' && *end != ':') end++;

        // Extract the tilde prefix
        size_t prefix_len = end - p;
        char prefix[256];
        if (prefix_len < sizeof(prefix)) {
            memcpy(prefix, p, prefix_len);
            prefix[prefix_len] = '\0';

            // Expand the prefix
            char *expanded = expand_tilde_path(prefix);
            if (expanded) {
                size_t exp_len = strlen(expanded);
                if (result_pos + exp_len < PATH_MAX) {
                    memcpy(result + result_pos, expanded, exp_len);
                    result_pos += exp_len;
                }
                free(expanded);
            } else {
                // Keep original if expansion failed
                memcpy(result + result_pos, p, prefix_len);
                result_pos += prefix_len;
            }
            p = end;
        }
    }

    // Process rest of string, expanding tildes after colons
    while (*p && result_pos < PATH_MAX - 1) {
        if (*p == ':' && *(p + 1) == '~') {
            // Colon followed by tilde - potential expansion
            result[result_pos++] = *p++;  // Copy the colon

            // Find end of tilde prefix
            const char *end = p + 1;
            while (*end && *end != '/' && *end != ':') end++;

            // Extract the tilde prefix
            size_t prefix_len = end - p;
            char prefix[256];
            if (prefix_len < sizeof(prefix)) {
                memcpy(prefix, p, prefix_len);
                prefix[prefix_len] = '\0';

                // Expand the prefix
                char *expanded = expand_tilde_path(prefix);
                if (expanded) {
                    size_t exp_len = strlen(expanded);
                    if (result_pos + exp_len < PATH_MAX) {
                        memcpy(result + result_pos, expanded, exp_len);
                        result_pos += exp_len;
                    }
                    free(expanded);
                } else {
                    // Keep original if expansion failed
                    if (result_pos + prefix_len < PATH_MAX) {
                        memcpy(result + result_pos, p, prefix_len);
                        result_pos += prefix_len;
                    }
                }
                p = end;
            }
        } else {
            result[result_pos++] = *p++;
        }
    }

    result[result_pos] = '\0';

    // Check if any expansion actually happened
    if (strcmp(result, value) == 0) {
        free(result);
        return NULL;  // No change
    }

    return result;
}

// Expand tilde in all arguments
int expand_tilde(char **args) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        // Check for quoted tilde marker (\x01~)
        if (args[i][0] == '\x01' && args[i][1] == '~') {
            // Remove the marker - tilde was quoted, don't expand
            // Shift the string left by 1 to remove the \x01
            memmove(args[i], args[i] + 1, strlen(args[i]));
            continue;
        }
        if (args[i][0] == '~') {
            char *expanded = expand_tilde_path(args[i]);
            if (expanded) {
                // Replace the argument with expanded version
                args[i] = expanded;
            }
            // If expansion failed, keep original
        }
    }

    return 0;
}

// Remove \x01 and \x03 markers from a string (in-place)
// \x01 markers protect quoted characters from expansion
// \x03 markers delimit unquoted expansion regions (for IFS splitting)
void strip_quote_markers(char *s) {
    if (!s) return;

    // First check if there are any markers - avoid writing to read-only strings
    const char *check = s;
    bool has_markers = false;
    while (*check) {
        if (*check == '\x01' || *check == '\x03') {
            has_markers = true;
            break;
        }
        check++;
    }
    if (!has_markers) return;

    const char *read = s;
    char *write = s;

    while (*read) {
        if (*read == '\x01' && *(read + 1)) {
            // Skip the marker, copy the protected character
            read++;
            *write++ = *read++;
        } else if (*read == '\x03') {
            // Skip IFS split marker entirely
            read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

// Remove \x01 markers from all arguments
void strip_quote_markers_args(char **args) {
    if (!args) return;

    for (int i = 0; args[i] != NULL; i++) {
        strip_quote_markers(args[i]);
    }
}

// Preprocess bracket expressions to handle POSIX collating elements and
// equivalence classes that macOS glob() doesn't support.
// Converts [.x.] to literal x, [=x=] to literal x within bracket expressions.
// Returns newly allocated string, caller must free.
static char *preprocess_bracket_expr(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s);
    // Allocate enough space (output is always <= input length)
    char *result = malloc(len + 1);
    if (!result) return NULL;

    const char *read = s;
    char *write = result;

    while (*read) {
        if (*read == '[') {
            // Start of bracket expression
            *write++ = *read++;

            // Check for negation
            if (*read == '!' || *read == '^') {
                *write++ = *read++;
            }

            // First char after [ (or [! / [^) can be ] and it's literal
            if (*read == ']') {
                *write++ = *read++;
            }

            // Process contents of bracket expression
            while (*read && *read != ']') {
                // Check for collating element [.x.] or equivalence class [=x=]
                if (*read == '[' && (*(read + 1) == '.' || *(read + 1) == '=')) {
                    char delim = *(read + 1);  // . or =
                    const char *start = read + 2;  // Start of the element

                    // Find the closing delimiter (x.] or x=])
                    const char *end = start;
                    while (*end && !(*end == delim && *(end + 1) == ']')) {
                        end++;
                    }

                    if (*end == delim && *(end + 1) == ']') {
                        // Found a complete collating element or equivalence class
                        // Extract the character(s) between delimiters
                        size_t elem_len = end - start;
                        if (elem_len == 1) {
                            // Single character - just output it directly
                            char c = *start;
                            // If it's ']', we need special handling:
                            // It must be first in the bracket expression
                            // For now, just output it - the pattern will work
                            // because we're in a bracket expression
                            *write++ = c;
                        } else if (elem_len > 1) {
                            // Multi-character collating element - just copy as literal
                            // (This is a simplification; true collating elements are locale-dependent)
                            for (const char *p = start; p < end; p++) {
                                *write++ = *p;
                            }
                        }
                        read = end + 2;  // Skip past x.] or x=]
                        continue;
                    }
                }

                // Check for character class [:class:]
                if (*read == '[' && *(read + 1) == ':') {
                    // Find the closing :]
                    const char *end = read + 2;
                    while (*end && !(*end == ':' && *(end + 1) == ']')) {
                        end++;
                    }
                    if (*end == ':' && *(end + 1) == ']') {
                        // Copy the entire character class as-is (glob supports these)
                        size_t class_len = end + 2 - read;
                        memcpy(write, read, class_len);
                        write += class_len;
                        read = end + 2;
                        continue;
                    }
                }

                // Regular character in bracket expression
                *write++ = *read++;
            }

            // Copy closing ]
            if (*read == ']') {
                *write++ = *read++;
            }
        } else {
            // Outside bracket expression - copy as-is
            *write++ = *read++;
        }
    }

    *write = '\0';
    return result;
}

// Custom glob implementation using fnmatch() for proper POSIX character class support
// macOS's glob() doesn't properly match character classes like [:alpha:]
// This function matches files in a directory using fnmatch()
// Returns array of matching paths (NULL-terminated), or NULL if no matches
// Caller must free the returned array and its strings
static char **fnmatch_glob(const char *pattern, size_t *match_count) {
    if (!pattern || !match_count) return NULL;
    *match_count = 0;

    // Extract directory and filename pattern
    char dir_path[PATH_MAX];
    const char *file_pattern;

    const char *last_slash = strrchr(pattern, '/');
    if (last_slash) {
        size_t dir_len = last_slash - pattern;
        if (dir_len >= PATH_MAX) return NULL;
        memcpy(dir_path, pattern, dir_len);
        dir_path[dir_len] = '\0';
        file_pattern = last_slash + 1;
    } else {
        strcpy(dir_path, ".");
        file_pattern = pattern;
    }

    // Check if the file pattern contains glob characters
    // If not, just check if the file exists
    bool has_glob = false;
    for (const char *p = file_pattern; *p; p++) {
        if (*p == '*' || *p == '?' || *p == '[') {
            has_glob = true;
            break;
        }
    }

    if (!has_glob) {
        // No glob chars - just check if file exists
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s",
                 strcmp(dir_path, ".") == 0 ? "" : dir_path,
                 file_pattern);
        // Remove leading / if dir was empty
        const char *check_path = full_path;
        if (check_path[0] == '/' && pattern[0] != '/') {
            check_path++;
        }
        if (access(check_path[0] ? check_path : file_pattern, F_OK) == 0) {
            char **results = malloc(2 * sizeof(char *));
            if (results) {
                results[0] = strdup(check_path[0] ? check_path : file_pattern);
                results[1] = NULL;
                *match_count = 1;
                return results;
            }
        }
        return NULL;
    }

    // Open directory
    DIR *dir = opendir(dir_path);
    if (!dir) return NULL;

    // Collect matches
    size_t capacity = 16;
    size_t count = 0;
    char **results = malloc(capacity * sizeof(char *));
    if (!results) {
        closedir(dir);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files unless pattern starts with .
        if (entry->d_name[0] == '.' && file_pattern[0] != '.') {
            continue;
        }

        // Match using fnmatch (which properly handles character classes)
        if (fnmatch(file_pattern, entry->d_name, 0) == 0) {
            // Build full path
            char full_path[PATH_MAX];
            if (strcmp(dir_path, ".") == 0) {
                safe_strcpy(full_path, entry->d_name, PATH_MAX);
            } else {
                snprintf(full_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
            }

            // Add to results
            if (count >= capacity - 1) {
                capacity *= 2;
                char **new_results = realloc(results, capacity * sizeof(char *));
                if (!new_results) {
                    for (size_t i = 0; i < count; i++) free(results[i]);
                    free(results);
                    closedir(dir);
                    return NULL;
                }
                results = new_results;
            }
            results[count++] = strdup(full_path);
        }
    }

    closedir(dir);

    if (count == 0) {
        free(results);
        return NULL;
    }

    // Sort results for consistent output
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(results[i], results[j]) > 0) {
                char *tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    results[count] = NULL;
    *match_count = count;
    return results;
}

// Check if a string contains glob characters
// Characters preceded by \x01 marker are protected (from quoted context)
int has_glob_chars(const char *s) {
    if (!s) return 0;

    int in_bracket = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\x01' && *(p + 1)) {
            // Skip marker and the protected character
            p++;
            continue;
        }
        if (*p == '\\' && *(p + 1)) {
            // Skip escaped character
            p++;
            continue;
        }
        if (*p == '[') {
            in_bracket = 1;
        } else if (*p == ']' && in_bracket) {
            return 1;  // Found a complete bracket expression
        } else if (!in_bracket && (*p == '*' || *p == '?')) {
            return 1;
        }
    }
    return 0;
}

// Convert a string with \x01 markers into a glob pattern
// Characters preceded by \x01 are escaped for glob (made literal)
// If do_preprocess is true, also preprocess collating elements/equivalence classes
// Returns newly allocated string, caller must free
static char *make_glob_pattern_ex(const char *s, bool do_preprocess) {
    if (!s) return NULL;

    // Allocate worst case: every char could need escaping
    size_t len = strlen(s);
    char *pattern = malloc(len * 2 + 1);
    if (!pattern) return NULL;

    const char *read = s;
    char *write = pattern;

    while (*read) {
        if (*read == '\x01' && *(read + 1)) {
            // Protected character - escape it for glob
            read++;  // Skip marker
            char c = *read++;
            // Escape glob metacharacters with backslash
            if (c == '*' || c == '?' || c == '[' || c == ']' || c == '\\') {
                *write++ = '\\';
            }
            *write++ = c;
        } else if (*read == '\x03') {
            // Skip IFS markers
            read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';

    // Preprocess POSIX bracket expressions (collating elements, equivalence classes)
    // Only needed for system glob() which may not support them
    // fnmatch() handles these natively, so skip if using fnmatch_glob
    if (do_preprocess) {
        char *preprocessed = preprocess_bracket_expr(pattern);
        if (preprocessed) {
            free(pattern);
            return preprocessed;
        }
    }

    return pattern;
}

// Check if pattern contains POSIX character classes that need fnmatch_glob
// macOS glob() doesn't properly handle character classes like [:alpha:]
// Note: Collating elements [.x.] and equivalence classes [=x=] are handled
// by preprocessing, so we only need fnmatch for character classes
static bool needs_fnmatch_glob(const char *s) {
    if (!s) return false;

    // Look for [: inside a bracket expression (character class)
    for (const char *p = s; *p; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip escaped char
            continue;
        }
        if (*p == '[') {
            // Found opening bracket, look for [: inside
            p++;
            // Skip negation
            if (*p == '!' || *p == '^') p++;
            // First ] is literal
            if (*p == ']') p++;

            // Scan for [:
            while (*p && *p != ']') {
                if (*p == '[' && *(p + 1) == ':') {
                    return true;  // Found character class
                }
                p++;
            }
        }
    }
    return false;
}

// Expand glob patterns in arguments
// NOTE: This function does NOT free original strings - caller manages memory
// If expansion happens, a new array is allocated and returned via args_ptr
// The original array is NOT freed - caller must handle cleanup
int expand_glob(char ***args_ptr, int *arg_count) {
    if (!args_ptr || !*args_ptr || !arg_count) return -1;

    char **args = *args_ptr;

    // First pass: count how many expansions we need
    int total_new_args = 0;
    int has_expansion = 0;

    for (int i = 0; i < *arg_count; i++) {
        if (has_glob_chars(args[i])) {
            // Always preprocess to handle collating elements [.x.] and
            // equivalence classes [=x=] which many systems don't support
            char *pattern = make_glob_pattern_ex(args[i], true);
            if (!pattern) {
                total_new_args++;
                continue;
            }

            // Use fnmatch_glob for patterns with character classes [::]
            // since macOS glob() doesn't handle them properly
            bool use_fnmatch = needs_fnmatch_glob(pattern);

            if (use_fnmatch) {
                size_t match_count = 0;
                char **matches = fnmatch_glob(pattern, &match_count);
                if (matches && match_count > 0) {
                    total_new_args += (int)match_count;
                    has_expansion = 1;
                    for (size_t j = 0; j < match_count; j++) free(matches[j]);
                    free(matches);
                } else {
                    total_new_args++;  // Keep original on no match
                }
            } else {
                glob_t gl;
                int flags = GLOB_NOCHECK | GLOB_TILDE;
                int ret = glob(pattern, flags, NULL, &gl);
                if (ret == 0) {
                    total_new_args += (int)gl.gl_pathc;
                    if (gl.gl_pathc > 1 || (gl.gl_pathc == 1 && strcmp(gl.gl_pathv[0], pattern) != 0)) {
                        has_expansion = 1;
                    }
                } else {
                    total_new_args++;  // Keep original on error
                }
                globfree(&gl);
            }
            free(pattern);
        } else {
            total_new_args++;
        }
    }

    // If no expansion needed, return early
    if (!has_expansion) {
        return 0;
    }

    // Allocate new args array
    char **new_args = malloc((total_new_args + 1) * sizeof(char *));
    if (!new_args) return -1;

    // Second pass: fill in the new array
    // All strings are strdup'd for uniform memory management
    int new_idx = 0;
    for (int i = 0; i < *arg_count; i++) {
        if (has_glob_chars(args[i])) {
            // Always preprocess to handle collating elements and equivalence classes
            char *pattern = make_glob_pattern_ex(args[i], true);
            if (!pattern) {
                // Fallback: strip markers and use as-is
                char *stripped = strdup(args[i]);
                if (stripped) strip_quote_markers(stripped);
                new_args[new_idx++] = stripped ? stripped : strdup(args[i]);
                continue;
            }

            // Use fnmatch_glob for character classes
            bool use_fnmatch = needs_fnmatch_glob(pattern);

            if (use_fnmatch) {
                size_t match_count = 0;
                char **matches = fnmatch_glob(pattern, &match_count);
                if (matches && match_count > 0) {
                    for (size_t j = 0; j < match_count; j++) {
                        new_args[new_idx++] = matches[j];  // Transfer ownership
                    }
                    free(matches);  // Free array but not strings
                } else {
                    // No match - keep original with markers stripped
                    char *stripped = strdup(args[i]);
                    if (stripped) strip_quote_markers(stripped);
                    new_args[new_idx++] = stripped ? stripped : strdup(args[i]);
                }
            } else {
                glob_t gl;
                int flags = GLOB_NOCHECK | GLOB_TILDE;
                int ret = glob(pattern, flags, NULL, &gl);
                if (ret == 0) {
                    for (size_t j = 0; j < gl.gl_pathc; j++) {
                        new_args[new_idx++] = strdup(gl.gl_pathv[j]);
                    }
                    globfree(&gl);
                } else {
                    // Keep original on error (strdup for uniform ownership)
                    char *stripped = strdup(args[i]);
                    if (stripped) strip_quote_markers(stripped);
                    new_args[new_idx++] = stripped ? stripped : strdup(args[i]);
                }
            }
            free(pattern);
        } else {
            // No glob - strdup for uniform ownership
            new_args[new_idx++] = strdup(args[i]);
        }
    }
    new_args[new_idx] = NULL;

    // Original array NOT freed - caller manages it
    // Update pointers
    *args_ptr = new_args;
    *arg_count = new_idx;

    return 0;
}
