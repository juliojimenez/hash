#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ifs.h"
#include "shellvar.h"

#define MAX_SPLIT_ARGS 1024
#define MAX_ARG_LENGTH 8192

// Get current IFS value
const char *ifs_get(void) {
    const char *ifs = shellvar_get("IFS");
    if (ifs == NULL) {
        return DEFAULT_IFS;  // Unset IFS = default
    }
    return ifs;  // Empty IFS = no splitting
}

// Check if character is an IFS whitespace character
// IFS whitespace means: space, tab, or newline that appears in IFS
int ifs_is_whitespace(char c, const char *ifs) {
    if (c != ' ' && c != '\t' && c != '\n') {
        return 0;  // Not a whitespace character at all
    }
    return strchr(ifs, c) != NULL;
}

// Check if character is in IFS
static int is_ifs_char(char c, const char *ifs) {
    return strchr(ifs, c) != NULL;
}

// Check if argument is a variable assignment (VAR=value)
// POSIX: Assignment values don't undergo word splitting
static int is_var_assignment(const char *arg) {
    if (!arg) return 0;
    // Must start with letter or underscore
    if (!isalpha((unsigned char)*arg) && *arg != '_') return 0;
    const char *p = arg + 1;
    // Find the '=' - variable name must be alphanumeric or underscore
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
        p++;
    }
    return *p == '=';
}

// Strip \x03 and \x04 markers from a string in place
static void strip_markers_inplace(char *s) {
    if (!s) return;
    const char *src = s;
    char *dst = s;
    while (*src) {
        if (*src != '\x03' && *src != '\x04') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// Process a single argument that may contain \x03 markers
// Returns array of resulting strings (may be more than one if splitting occurred)
// Caller must free each string and the array
static char **process_arg_with_markers(const char *arg, const char *ifs, int *result_count) {
    *result_count = 0;

    // Quick check: does this arg have any \x03 markers?
    if (!strchr(arg, '\x03')) {
        // No markers - return a copy of the original
        char **result = malloc(2 * sizeof(char *));
        if (!result) return NULL;
        result[0] = strdup(arg);
        result[1] = NULL;
        *result_count = 1;
        return result;
    }

    // Build list of parts (both literal and expanded)
    // Each expanded part may split into multiple words
    char **all_words = malloc(MAX_SPLIT_ARGS * sizeof(char *));
    if (!all_words) return NULL;
    int word_count = 0;

    // Buffer for accumulating the current word being built
    char current_word[MAX_ARG_LENGTH];
    size_t current_len = 0;

    const char *p = arg;
    int in_expansion = 0;

    while (*p && word_count < MAX_SPLIT_ARGS - 1) {
        if (*p == '\x03') {
            // Toggle expansion mode
            if (!in_expansion) {
                // Entering expansion region
                // Any accumulated literal content stays as prefix to first split word
                in_expansion = 1;
            } else {
                // Leaving expansion region
                // The current_word now contains literal + expanded content
                // Don't add it yet - more literal content might follow
                in_expansion = 0;
            }
            p++;
            continue;
        }

        if (in_expansion && is_ifs_char(*p, ifs)) {
            // IFS character inside expansion - this is a split point

            // Handle IFS whitespace vs non-whitespace
            if (ifs_is_whitespace(*p, ifs)) {
                // Skip leading whitespace at start of expansion
                if (current_len == 0) {
                    // Skip all consecutive IFS whitespace
                    while (*p && *p != '\x03' && ifs_is_whitespace(*p, ifs)) {
                        p++;
                    }
                    continue;
                }

                // End current word
                current_word[current_len] = '\0';
                if (current_len > 0) {
                    all_words[word_count++] = strdup(current_word);
                }
                current_len = 0;

                // Skip all consecutive IFS whitespace
                while (*p && *p != '\x03' && ifs_is_whitespace(*p, ifs)) {
                    p++;
                }
            } else {
                // Non-whitespace IFS char - delimiter
                current_word[current_len] = '\0';
                if (current_len > 0) {
                    all_words[word_count++] = strdup(current_word);
                }
                current_len = 0;
                p++;

                // Skip any following IFS whitespace
                while (*p && *p != '\x03' && ifs_is_whitespace(*p, ifs)) {
                    p++;
                }
            }
        } else {
            // Regular character - add to current word
            if (current_len < MAX_ARG_LENGTH - 1) {
                current_word[current_len++] = *p;
            }
            p++;
        }
    }

    // Add final word if any
    if (current_len > 0) {
        current_word[current_len] = '\0';
        all_words[word_count++] = strdup(current_word);
    }

    all_words[word_count] = NULL;
    *result_count = word_count;

    if (word_count == 0) {
        free(all_words);
        return NULL;
    }

    return all_words;
}

// Perform IFS word splitting on argument array
int ifs_split_args(char ***args_ptr, int *arg_count) {
    if (!args_ptr || !*args_ptr || !arg_count) return -1;

    char **args = *args_ptr;
    const char *ifs = ifs_get();

    // Empty IFS means no IFS splitting, but $@ still splits on argument boundaries
    if (ifs[0] == '\0') {
        // Check if we have \x04 markers (quoted $@) - these still need to be split
        int has_at_split = 0;
        for (int i = 0; i < *arg_count; i++) {
            if (strchr(args[i], '\x04')) {
                has_at_split = 1;
                break;
            }
        }

        if (!has_at_split) {
            // Just strip \x03 markers but don't split
            for (int i = 0; i < *arg_count; i++) {
                // Only modify strings that actually have markers
                // (avoid writing to read-only string literals)
                if (!strchr(args[i], '\x03')) {
                    continue;
                }
                // Remove \x03 markers in place
                const char *src = args[i];
                char *dst = args[i];
                while (*src) {
                    if (*src != '\x03') {
                        *dst++ = *src;
                    }
                    src++;
                }
                *dst = '\0';
            }
            return 0;
        }
        // Fall through to handle \x04 splitting
    }

    // First pass: check if any splitting will occur
    int has_splitting = 0;
    for (int i = 0; i < *arg_count; i++) {
        // Check for \x04 marker (quoted $@ argument separator)
        if (strchr(args[i], '\x04')) {
            has_splitting = 1;
            break;
        }
        if (strchr(args[i], '\x03')) {
            // Has expansion markers - check if IFS chars are inside
            const char *p = args[i];
            int in_expansion = 0;
            while (*p) {
                if (*p == '\x03') {
                    in_expansion = !in_expansion;
                } else if (in_expansion && is_ifs_char(*p, ifs)) {
                    has_splitting = 1;
                    break;
                }
                p++;
            }
            if (has_splitting) break;
        }
    }

    // If no splitting needed, just strip markers from strings that have them
    if (!has_splitting) {
        for (int i = 0; i < *arg_count; i++) {
            // Only modify strings that actually have markers
            // (avoid writing to read-only string literals)
            if (!strchr(args[i], '\x03') && !strchr(args[i], '\x04')) {
                continue;
            }
            const char *src = args[i];
            char *dst = args[i];
            while (*src) {
                if (*src != '\x03' && *src != '\x04') {
                    *dst++ = *src;
                }
                src++;
            }
            *dst = '\0';
        }
        return 0;
    }

    // Build new argument array
    char **new_args = malloc(MAX_SPLIT_ARGS * sizeof(char *));
    if (!new_args) return -1;
    int new_count = 0;

    for (int i = 0; i < *arg_count && new_count < MAX_SPLIT_ARGS - 1; i++) {
        // POSIX: Variable assignments don't undergo word splitting
        // Just strip markers and keep as single argument
        if (is_var_assignment(args[i])) {
            char *copy = strdup(args[i]);
            if (copy) {
                strip_markers_inplace(copy);
                new_args[new_count++] = copy;
            }
            continue;
        }

        // Check for \x04 marker (quoted $@ - split into separate args, no IFS splitting)
        if (strchr(args[i], '\x04')) {
            // Split on \x04 markers
            char *copy = strdup(args[i]);
            if (!copy) continue;

            const char *start = copy;
            char *p = copy;
            while (*p && new_count < MAX_SPLIT_ARGS - 1) {
                if (*p == '\x04') {
                    *p = '\0';  // Terminate current part
                    // Strip \x03 markers from this part and add if non-empty
                    char *part = strdup(start);
                    if (part) {
                        strip_markers_inplace(part);
                        if (*part) {
                            new_args[new_count++] = part;
                        } else {
                            free(part);
                        }
                    }
                    start = p + 1;
                }
                p++;
            }
            // Handle last part after final \x04 (or entire string if no \x04)
            if (*start && new_count < MAX_SPLIT_ARGS - 1) {
                char *part = strdup(start);
                if (part) {
                    strip_markers_inplace(part);
                    if (*part) {
                        new_args[new_count++] = part;
                    } else {
                        free(part);
                    }
                }
            }
            free(copy);
            continue;
        }

        int part_count = 0;
        char **parts = process_arg_with_markers(args[i], ifs, &part_count);

        if (parts) {
            for (int j = 0; j < part_count && new_count < MAX_SPLIT_ARGS - 1; j++) {
                new_args[new_count++] = parts[j];
            }
            free(parts);  // Free the array, not the strings (they're now in new_args)
        }
        // If parts is NULL (empty expansion), argument is removed
    }

    new_args[new_count] = NULL;

    // Update pointers
    *args_ptr = new_args;
    *arg_count = new_count;

    return 0;
}
