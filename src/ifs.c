#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ifs.h"
#include "utils.h"
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

// A protected byte: \x01 then the byte itself, which may be a real \x03 or
// \x04 that must not be read as a marker. Every scan here skips the pair whole.
static bool is_protected_pair(const char *p) {
    return p[0] == '\x01' && p[1] != '\0';
}

// Does s hold a real marker m (not a protected byte that happens to equal it)?
static bool has_marker(const char *s, char m) {
    for (const char *p = s; *p; p++) {
        if (is_protected_pair(p)) {
            p++;
            continue;
        }
        if (*p == m) {
            return true;
        }
    }
    return false;
}

// Strip \x03 and \x04 markers from a string in place. A \x04 left in a
// word that is not split (an assignment, say) separated two positional
// parameters of "$@": they are joined with a space, as in x="$@"
static void strip_markers_inplace(char *s) {
    if (!s) return;
    const char *src = s;
    char *dst = s;
    while (*src) {
        if (is_protected_pair(src)) {
            *dst++ = *src++;
            *dst++ = *src++;
            continue;
        }
        if (*src == '\x04') {
            *dst++ = ' ';
        } else if (*src != '\x03') {
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
    if (!has_marker(arg, '\x03')) {
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
        if (is_protected_pair(p)) {
            // A protected byte is quoted content: never a marker, never a split
            // point (a quoted space inside ${1+"$@"} stays in its field). A real
            // control byte travels the same way, so IFS set to such a byte does
            // not split on it; bash does, and nobody has needed that yet.
            if (current_len < MAX_ARG_LENGTH - 2) {
                current_word[current_len++] = *p;
                current_word[current_len++] = *(p + 1);
            }
            p += 2;
            continue;
        }
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

                // POSIX 2.6.5: "Each occurrence of a non-whitespace IFS character,
                // along with any IFS white space adjacent to it, shall delimit a field."
                // If whitespace is followed by non-whitespace IFS, absorb it as one delimiter
                if (*p && *p != '\x03' && is_ifs_char(*p, ifs) && !ifs_is_whitespace(*p, ifs)) {
                    p++;  // Skip the non-whitespace IFS delimiter
                    // Skip any trailing IFS whitespace
                    while (*p && *p != '\x03' && ifs_is_whitespace(*p, ifs)) {
                        p++;
                    }
                }
            } else {
                // Non-whitespace IFS char - delimiter
                // POSIX: Each non-whitespace IFS char delimits a field,
                // including empty fields from consecutive delimiters
                current_word[current_len] = '\0';
                all_words[word_count++] = strdup(current_word);
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
int ifs_split_args(char ***args_ptr, int *arg_count, int assignment_words) {
    if (!args_ptr || !*args_ptr || !arg_count) return -1;

    char **args = *args_ptr;
    const char *ifs = ifs_get();

    // Empty IFS means no IFS splitting, but $@ still splits on argument boundaries
    if (ifs[0] == '\0') {
        // Check if we have \x04 markers (quoted $@) - these still need to be split
        int has_at_split = 0;
        for (int i = 0; i < *arg_count; i++) {
            if (has_marker(args[i], '\x04')) {
                has_at_split = 1;
                break;
            }
        }

        if (!has_at_split) {
            // Empty IFS: no field splitting, but empty unquoted expansions
            // should still produce no field (POSIX)
            // First check if we need to remove any empty expansions
            int has_empty_expansion = 0;
            for (int i = 0; i < *arg_count; i++) {
                const char *s = args[i];
                if (s[0] == '\x03' && s[1] == '\x03' && s[2] == '\0') {
                    has_empty_expansion = 1;
                    break;
                }
            }

            if (has_empty_expansion) {
                // Need to rebuild the array to remove empty expansions
                char **new_args = malloc(MAX_SPLIT_ARGS * sizeof(char *));
                if (!new_args) return -1;
                int new_count = 0;

                for (int i = 0; i < *arg_count && new_count < MAX_SPLIT_ARGS - 1; i++) {
                    const char *s = args[i];
                    // Skip empty expansions (\x03\x03)
                    if (s[0] == '\x03' && s[1] == '\x03' && s[2] == '\0') {
                        continue;
                    }
                    // Copy and strip markers
                    char *copy = strdup(args[i]);
                    if (copy) {
                        strip_markers_inplace(copy);
                        new_args[new_count++] = copy;
                    }
                }
                new_args[new_count] = NULL;
                *args_ptr = new_args;
                *arg_count = new_count;
            } else {
                // No empty expansions - just strip markers in place
                for (int i = 0; i < *arg_count; i++) {
                    if (!has_marker(args[i], '\x03')) {
                        continue;
                    }
                    strip_markers_inplace(args[i]);
                }
            }
            return 0;
        }
        // Fall through to handle \x04 splitting
    }

    // First pass: check if any splitting or removal will occur
    int has_splitting = 0;
    for (int i = 0; i < *arg_count; i++) {
        // Check for \x04 marker (quoted $@ argument separator)
        if (has_marker(args[i], '\x04')) {
            has_splitting = 1;
            break;
        }
        if (has_marker(args[i], '\x03')) {
            // Has expansion markers - check if IFS chars are inside or empty expansion
            const char *p = args[i];
            int in_expansion = 0;
            while (*p) {
                if (is_protected_pair(p)) {
                    p += 2;
                    continue;
                }
                if (*p == '\x03') {
                    if (in_expansion) {
                        // Closing marker - check if expansion was empty
                        // (no chars between markers means empty expansion to remove)
                        in_expansion = 0;
                    } else {
                        // Opening marker - check for adjacent closing marker (empty expansion)
                        if (*(p + 1) == '\x03') {
                            // Empty expansion \x03\x03 - may need to remove argument
                            has_splitting = 1;
                            break;
                        }
                        in_expansion = 1;
                    }
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
            if (!has_marker(args[i], '\x03') && !has_marker(args[i], '\x04')) {
                continue;
            }
            strip_markers_inplace(args[i]);
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
        if (i < assignment_words) {
            char *copy = strdup(args[i]);
            if (copy) {
                strip_markers_inplace(copy);
                new_args[new_count++] = copy;
            }
            continue;
        }

        // Check for \x04 marker (quoted $@ - split into separate args, no IFS splitting)
        if (has_marker(args[i], '\x04')) {
            // Split on \x04 markers
            char *copy = strdup(args[i]);
            if (!copy) continue;

            const char *start = copy;
            char *p = copy;
            while (*p && new_count < MAX_SPLIT_ARGS - 1) {
                if (is_protected_pair(p)) {
                    p += 2;
                    continue;
                }
                if (*p == '\x04') {
                    *p = '\0';  // Terminate current part
                    // Strip \x03 markers from this part; an empty part is an
                    // empty positional parameter and stays a field
                    char *part = strdup(start);
                    if (part) {
                        strip_markers_inplace(part);
                        new_args[new_count++] = part;
                    }
                    start = p + 1;
                }
                p++;
            }
            // Handle last part after final \x04
            if (new_count < MAX_SPLIT_ARGS - 1) {
                char *part = strdup(start);
                if (part) {
                    strip_markers_inplace(part);
                    new_args[new_count++] = part;
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
