#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "varexpand.h"
#include "safe_string.h"
#include "hash.h"

#define MAX_EXPANDED_LENGTH 8192

// Check if character is valid in variable name
static int is_varname_char(char c) {
    return isalnum(c) || c == '_';
}

// Expand environment variables in a string
char *varexpand_expand(const char *str, int last_exit_code) {
    if (!str) return NULL;

    char *result = malloc(MAX_EXPANDED_LENGTH);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    while (*p && out_pos < MAX_EXPANDED_LENGTH - 1) {
        if (*p == '\\' && *(p + 1) == '$') {
            // Escaped dollar sign - \$ → $
            result[out_pos++] = '$';
            p += 2;
        } else if (*p == '$') {
            p++;  // Skip $

            const char *var_value = NULL;
            char var_name[256] = {0};

            // Check for special variables
            if (*p == '?') {
                // $? - exit code
                snprintf(var_name, sizeof(var_name), "%d", last_exit_code);
                var_value = var_name;
                p++;
            } else if (*p == '$') {
                // $$ - process ID
                snprintf(var_name, sizeof(var_name), "%d", getpid());
                var_value = var_name;
                p++;
            } else if (*p == '0') {
                // $0 - shell name
                var_value = HASH_NAME;
                p++;
            } else if (*p == '{') {
                // ${VAR} syntax
                p++;  // Skip {

                size_t name_len = 0;
                while (*p && *p != '}' && name_len < sizeof(var_name) - 1) {
                    if (is_varname_char(*p)) {
                        var_name[name_len++] = *p++;
                    } else {
                        // Invalid character in variable name
                        break;
                    }
                }

                if (*p == '}') {
                    p++;  // Skip }
                    var_name[name_len] = '\0';

                    if (name_len > 0) {
                        var_value = getenv(var_name);
                    }
                }
            } else if (is_varname_char(*p)) {
                // $VAR syntax
                size_t name_len = 0;
                while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                    var_name[name_len++] = *p++;
                }
                var_name[name_len] = '\0';

                if (name_len > 0) {
                    var_value = getenv(var_name);
                }
            } else {
                // Just a $ followed by something else, keep the $
                if (out_pos < MAX_EXPANDED_LENGTH - 1) {
                    result[out_pos++] = '$';
                }
            }

            // Append variable value if found
            if (var_value) {
                size_t val_len = strlen(var_value);
                size_t space = MAX_EXPANDED_LENGTH - 1 - out_pos;
                size_t to_copy = (val_len < space) ? val_len : space;

                if (to_copy > 0) {
                    memcpy(result + out_pos, var_value, to_copy);
                    out_pos += to_copy;
                }
            }
            // If variable doesn't exist, it expands to empty string (like bash)

        } else {
            // Regular character
            result[out_pos++] = *p++;
        }
    }

    result[out_pos] = '\0';
    return result;
}

// Expand variables in all arguments
int varexpand_args(char **args, int last_exit_code) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        // Check if this argument contains $
        if (strchr(args[i], '$') != NULL) {
            char *expanded = varexpand_expand(args[i], last_exit_code);
            if (expanded) {
                // Replace argument with expanded version
                // Note: args[i] points into line buffer, don't free it
                args[i] = expanded;
            }
        }
    }

    return 0;
}
