#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fnmatch.h>
#include "varexpand.h"
#include "safe_string.h"
#include "hash.h"
#include "script.h"
#include "jobs.h"
#include "config.h"
#include "shellvar.h"

#define MAX_EXPANDED_LENGTH 8192

// Track if an unset variable error occurred during expansion
static bool varexpand_error = false;

// Check and report unset variable error when -u is set
// Returns true if error occurred
static bool check_unset_error(const char *var_name) {
    if (shell_option_nounset()) {
        fprintf(stderr, "%s: %s: unbound variable\n", HASH_NAME, var_name);
        varexpand_error = true;
        return true;
    }
    return false;
}

// Get the last expansion error status
bool varexpand_had_error(void) {
    return varexpand_error;
}

// Clear the expansion error flag
void varexpand_clear_error(void) {
    varexpand_error = false;
}

// Check if character is valid in variable name
static int is_varname_char(char c) {
    return isalnum(c) || c == '_';
}

// Get positional parameter value ($1, $2, etc.)
// Returns NULL if not found
static const char *get_positional_param(int n) {
    if (n < 0 || n >= script_state.positional_count) {
        return NULL;
    }
    return script_state.positional_params[n];
}

// Expand environment variables in a string
char *varexpand_expand(const char *str, int last_exit_code) {
    if (!str) return NULL;

    char *result = malloc(MAX_EXPANDED_LENGTH);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    while (*p && out_pos < MAX_EXPANDED_LENGTH - 1) {
        if (*p == '\x01' && *(p + 1) == '\\' && (*(p + 2) == '$' || *(p + 2) == '`')) {
            // Protected backslash from single quotes - output \$ or \` literally
            result[out_pos++] = '\\';
            result[out_pos++] = *(p + 2);
            p += 3;
        } else if (*p == '\x01' && *(p + 1) == '$') {
            // Single-quoted dollar sign - output $ literally (no expansion)
            result[out_pos++] = '$';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '$') {
            // Escaped dollar sign - \$ → $
            result[out_pos++] = '$';
            p += 2;
        } else if (*p == '\x02' && *(p + 1) == '$') {
            // Quoted variable marker - skip marker and process $ with quote flag
            p++;  // Skip \x02
            goto process_var_quoted;
        } else if (*p == '$') {
process_var_quoted:;
            bool is_quoted = (p > str && *(p - 1) == '\x02');  // Check if we came from marker
            p++;  // Skip $

            const char *var_value = NULL;
            char var_name[256] = {0};

            // Check for special variables
            // Note: Parser may add \x01 marker before ?, *, [ when in quotes
            // We need to skip the marker when checking for special variables like $?
            if (*p == '?' || (*p == '\x01' && *(p + 1) == '?')) {
                // $? - exit code
                if (*p == '\x01') p++;  // Skip marker if present
                snprintf(var_name, sizeof(var_name), "%d", last_exit_code);
                var_value = var_name;
                p++;
            } else if (*p == '$') {
                // $$ - process ID
                snprintf(var_name, sizeof(var_name), "%d", getpid());
                var_value = var_name;
                p++;
            } else if (*p == '!') {
                // $! - PID of last background job
                pid_t bg_pid = jobs_get_last_bg_pid();
                if (bg_pid > 0) {
                    snprintf(var_name, sizeof(var_name), "%d", bg_pid);
                    var_value = var_name;
                } else {
                    var_value = "";
                }
                p++;
            } else if (*p == '#') {
                // $# - number of positional parameters (excluding $0)
                int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
                snprintf(var_name, sizeof(var_name), "%d", count);
                var_value = var_name;
                p++;
            } else if (*p == '*' || (*p == '\x01' && *(p + 1) == '*')) {
                // $* - all positional parameters as single string
                if (*p == '\x01') p++;  // Skip marker if present
                static char star_buf[MAX_EXPANDED_LENGTH];
                star_buf[0] = '\0';
                size_t pos = 0;
                for (int i = 1; i < script_state.positional_count && pos < sizeof(star_buf) - 1; i++) {
                    const char *param = get_positional_param(i);
                    if (param) {
                        if (i > 1 && pos < sizeof(star_buf) - 1) {
                            star_buf[pos++] = ' ';
                        }
                        size_t plen = strlen(param);
                        if (pos + plen < sizeof(star_buf)) {
                            memcpy(star_buf + pos, param, plen);
                            pos += plen;
                        }
                    }
                }
                star_buf[pos] = '\0';
                var_value = star_buf;
                p++;
            } else if (*p == '@') {
                // $@ - all positional parameters (same as $* in this context)
                static char at_buf[MAX_EXPANDED_LENGTH];
                at_buf[0] = '\0';
                size_t pos = 0;
                for (int i = 1; i < script_state.positional_count && pos < sizeof(at_buf) - 1; i++) {
                    const char *param = get_positional_param(i);
                    if (param) {
                        if (i > 1 && pos < sizeof(at_buf) - 1) {
                            at_buf[pos++] = ' ';
                        }
                        size_t plen = strlen(param);
                        if (pos + plen < sizeof(at_buf)) {
                            memcpy(at_buf + pos, param, plen);
                            pos += plen;
                        }
                    }
                }
                at_buf[pos] = '\0';
                var_value = at_buf;
                p++;
            } else if (*p == '0') {
                // $0 - script name or shell name
                const char *param0 = get_positional_param(0);
                var_value = param0 ? param0 : HASH_NAME;
                p++;
            } else if (*p == '{') {
                // ${VAR} or ${VAR-default} or ${VAR:-default} or ${#VAR} etc syntax
                p++;  // Skip {

                // Check for ${#VAR} - string length syntax
                bool get_length = false;
                if (*p == '#' && *(p + 1) == '}') {
                    // ${#} - same as $#, number of positional parameters
                    int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
                    snprintf(var_name, sizeof(var_name), "%d", count);
                    var_value = var_name;
                    p += 2;  // Skip # and }
                    goto append_value;
                } else if (*p == '#') {
                    // ${#var} - get length of variable
                    get_length = true;
                    p++;  // Skip #
                }

                // Parse variable name
                size_t name_len = 0;
                while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                    var_name[name_len++] = *p++;
                }
                var_name[name_len] = '\0';

                // Check for modifiers: - + = ? # % (and : prefix)
                char modifier = 0;
                bool check_null = false;
                bool double_modifier = false;  // For ## and %%
                static char word[1024];  // static to persist after scope
                word[0] = '\0';

                if (*p == ':') {
                    check_null = true;
                    p++;
                }

                if (*p == '-' || *p == '+' || *p == '=' || *p == '?' ||
                    *p == '#' || *p == '%') {
                    modifier = *p++;
                    // Check for ## or %%
                    if ((modifier == '#' || modifier == '%') && *p == modifier) {
                        double_modifier = true;
                        p++;
                    }

                    // Parse the word/pattern until closing brace
                    size_t word_len = 0;
                    int brace_depth = 1;
                    while (*p && brace_depth > 0 && word_len < sizeof(word) - 1) {
                        if (*p == '{') brace_depth++;
                        else if (*p == '}') {
                            brace_depth--;
                            if (brace_depth == 0) break;
                        }
                        word[word_len++] = *p++;
                    }
                    word[word_len] = '\0';
                }

                if (*p == '}') {
                    p++;  // Skip }

                    if (name_len > 0) {
                        // Get the variable value
                        const char *val = NULL;

                        // Check if it's a positional parameter
                        int is_positional = 1;
                        for (size_t i = 0; i < name_len; i++) {
                            if (!isdigit(var_name[i])) {
                                is_positional = 0;
                                break;
                            }
                        }
                        if (is_positional) {
                            int param_num = atoi(var_name);
                            val = get_positional_param(param_num);
                        } else {
                            val = shellvar_get(var_name);
                        }

                        // Determine if variable is "unset" or "null"
                        bool is_unset = (val == NULL);
                        bool is_null = (val != NULL && val[0] == '\0');

                        // Apply modifier
                        if (modifier == '-') {
                            // ${var-word}: use word if unset
                            // ${var:-word}: use word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                var_value = word;
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '+') {
                            // ${var+word}: use word if set
                            // ${var:+word}: use word if set and not null
                            if (!is_unset && (!check_null || !is_null)) {
                                var_value = word;
                            } else {
                                var_value = "";
                            }
                        } else if (modifier == '=') {
                            // ${var=word}: assign word if unset
                            // ${var:=word}: assign word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                setenv(var_name, word, 1);
                                var_value = word;
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '?') {
                            // ${var?word}: error if unset
                            // ${var:?word}: error if unset or null
                            if (is_unset || (check_null && is_null)) {
                                if (word[0]) {
                                    fprintf(stderr, "%s: %s\n", var_name, word);
                                } else {
                                    fprintf(stderr, "%s: parameter not set\n", var_name);
                                }
                                varexpand_error = true;
                                var_value = "";
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '#') {
                            // ${var#pattern}: remove smallest prefix matching pattern
                            // ${var##pattern}: remove largest prefix matching pattern
                            if (val) {
                                static char pattern_result[4096];
                                strncpy(pattern_result, val, sizeof(pattern_result) - 1);
                                pattern_result[sizeof(pattern_result) - 1] = '\0';

                                size_t val_len = strlen(val);
                                size_t match_len = 0;

                                // Try matching pattern at start of string
                                // For ##, find longest match; for #, find shortest
                                if (double_modifier) {
                                    // Longest match - try from end backwards
                                    for (size_t i = val_len; i > 0; i--) {
                                        char saved = pattern_result[i];
                                        pattern_result[i] = '\0';
                                        if (fnmatch(word, pattern_result, 0) == 0) {
                                            match_len = i;
                                            pattern_result[i] = saved;
                                            break;
                                        }
                                        pattern_result[i] = saved;
                                    }
                                } else {
                                    // Shortest match - try from start forwards
                                    for (size_t i = 1; i <= val_len; i++) {
                                        char saved = pattern_result[i];
                                        pattern_result[i] = '\0';
                                        if (fnmatch(word, pattern_result, 0) == 0) {
                                            match_len = i;
                                            pattern_result[i] = saved;
                                            break;
                                        }
                                        pattern_result[i] = saved;
                                    }
                                }

                                // Result is the part after the match
                                memmove(pattern_result, val + match_len, val_len - match_len + 1);
                                var_value = pattern_result;
                            } else {
                                var_value = "";
                            }
                        } else if (modifier == '%') {
                            // ${var%pattern}: remove smallest suffix matching pattern
                            // ${var%%pattern}: remove largest suffix matching pattern
                            if (val) {
                                static char pattern_result[4096];
                                strncpy(pattern_result, val, sizeof(pattern_result) - 1);
                                pattern_result[sizeof(pattern_result) - 1] = '\0';

                                size_t val_len = strlen(val);
                                size_t keep_len = val_len;  // How much to keep

                                // Try matching pattern at end of string
                                // For %%, find longest match; for %, find shortest
                                if (double_modifier) {
                                    // Longest match - try from start forwards
                                    for (size_t i = 0; i < val_len; i++) {
                                        if (fnmatch(word, val + i, 0) == 0) {
                                            keep_len = i;
                                            break;
                                        }
                                    }
                                } else {
                                    // Shortest match - try from end backwards
                                    for (size_t i = val_len; i > 0; i--) {
                                        if (fnmatch(word, val + i - 1, 0) == 0) {
                                            // Check if this is a valid suffix match
                                            // (the pattern must match starting at position i-1)
                                            if (fnmatch(word, val + i - 1, 0) == 0) {
                                                keep_len = i - 1;
                                            }
                                        }
                                    }
                                    // Actually for %, we want shortest suffix that matches
                                    keep_len = val_len;
                                    for (size_t i = val_len; i > 0; i--) {
                                        if (fnmatch(word, val + i - 1, 0) == 0) {
                                            keep_len = i - 1;
                                            // Don't break - keep looking for shorter match
                                        }
                                    }
                                }

                                // Result is the part before the match
                                pattern_result[keep_len] = '\0';
                                var_value = pattern_result;
                            } else {
                                var_value = "";
                            }
                        } else {
                            // No modifier, simple expansion
                            if (is_unset && check_unset_error(var_name)) {
                                // Unset variable with -u option
                                var_value = "";
                            } else {
                                var_value = val;
                            }
                        }

                        // Handle ${#var} - return length instead of value
                        if (get_length) {
                            size_t len = var_value ? strlen(var_value) : 0;
                            snprintf(var_name, sizeof(var_name), "%zu", len);
                            var_value = var_name;
                        }
                    } else {
                        // Empty variable name ${} is a syntax error
                        fprintf(stderr, "%s: Bad substitution\n", HASH_NAME);
                        varexpand_error = true;
                        var_value = "";
                    }
                }
            } else if (isdigit(*p)) {
                // $N positional parameter (single digit for POSIX compliance)
                int param_num = *p - '0';
                p++;
                var_value = get_positional_param(param_num);
                // Check for unset positional parameter (except $0)
                if (var_value == NULL && param_num > 0) {
                    char param_name[16];
                    snprintf(param_name, sizeof(param_name), "%d", param_num);
                    check_unset_error(param_name);
                }
            } else if (is_varname_char(*p)) {
                // $VAR syntax
                size_t name_len = 0;
                while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                    var_name[name_len++] = *p++;
                }
                var_name[name_len] = '\0';

                if (name_len > 0) {
                    var_value = shellvar_get(var_name);
                    // Check for unset variable with -u option
                    if (var_value == NULL) {
                        check_unset_error(var_name);
                    }
                }
            } else {
                // Just a $ followed by something else, keep the $
                if (out_pos < MAX_EXPANDED_LENGTH - 1) {
                    result[out_pos++] = '$';
                }
            }

append_value:
            // Append variable value if found
            if (var_value) {
                size_t val_len = strlen(var_value);
                if (val_len > 0) {
                    if (is_quoted) {
                        // Append with markers to prevent glob expansion and quote interpretation
                        for (size_t i = 0; i < val_len && out_pos < MAX_EXPANDED_LENGTH - 2; i++) {
                            char c = var_value[i];
                            if (c == '*' || c == '?' || c == '[' || c == '"' || c == '\'' || c == '\\') {
                                // Add marker before special characters
                                result[out_pos++] = '\x01';
                            }
                            result[out_pos++] = c;
                        }
                    } else {
                        // Unquoted - simple copy
                        size_t space = MAX_EXPANDED_LENGTH - 1 - out_pos;
                        size_t to_copy = (val_len < space) ? val_len : space;
                        if (to_copy > 0) {
                            memcpy(result + out_pos, var_value, to_copy);
                            out_pos += to_copy;
                        }
                    }
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
