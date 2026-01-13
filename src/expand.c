#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <glob.h>
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

// Remove \x01 markers from a string (in-place)
// These markers are used to protect quoted characters from expansion
void strip_quote_markers(char *s) {
    if (!s) return;

    // First check if there are any markers - avoid writing to read-only strings
    const char *check = s;
    bool has_markers = false;
    while (*check) {
        if (*check == '\x01') {
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
            glob_t gl;
            int flags = GLOB_NOCHECK | GLOB_TILDE;
            int ret = glob(args[i], flags, NULL, &gl);
            if (ret == 0) {
                total_new_args += (int)gl.gl_pathc;
                if (gl.gl_pathc > 1 || (gl.gl_pathc == 1 && strcmp(gl.gl_pathv[0], args[i]) != 0)) {
                    has_expansion = 1;
                }
            } else {
                total_new_args++;  // Keep original on error
            }
            globfree(&gl);
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
            glob_t gl;
            int flags = GLOB_NOCHECK | GLOB_TILDE;
            int ret = glob(args[i], flags, NULL, &gl);
            if (ret == 0) {
                for (size_t j = 0; j < gl.gl_pathc; j++) {
                    new_args[new_idx++] = strdup(gl.gl_pathv[j]);
                }
                globfree(&gl);
                // Original string NOT freed - caller manages memory
            } else {
                // Keep original on error (strdup for uniform ownership)
                new_args[new_idx++] = strdup(args[i]);
            }
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
