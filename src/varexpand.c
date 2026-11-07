#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fnmatch.h>
#include "exec.h"
#include "varexpand.h"
#include "safe_string.h"
#include "hash.h"
#include "script.h"
#include "jobs.h"
#include "config.h"
#include "shellvar.h"
#include "ifs.h"
#include "utils.h"

#define MAX_EXPANDED_LENGTH 8192

// Track if an unset variable error occurred during expansion
static bool varexpand_error = false;

// Convert \x01 markers in pattern to backslash escapes for fnmatch
// Returns a static buffer - not thread safe
static const char *convert_pattern_for_fnmatch(const char *pattern) {
    static char converted[2048];
    size_t out = 0;
    const char *p = pattern;

    while (*p && out < sizeof(converted) - 2) {
        if (*p == '\x01' && *(p + 1)) {
            // Marker followed by character - convert to backslash escape
            converted[out++] = '\\';
            p++;  // Skip marker
            converted[out++] = *p++;
        } else {
            converted[out++] = *p++;
        }
    }
    converted[out] = '\0';
    return converted;
}

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
static pid_t shell_pid_value = 0;

void shell_pid_init(void) {
    shell_pid_value = getpid();
}

pid_t shell_pid(void) {
    return shell_pid_value > 0 ? shell_pid_value : getpid();
}

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

// Add \x02 marker before each $ in word to indicate quoted context
// Returns a static buffer - caller should use immediately or copy
static const char *mark_dollars_as_quoted(const char *word) {
    static char marked[4096];
    size_t out = 0;
    const char *p = word;

    while (*p && out < sizeof(marked) - 2) {
        if (*p == '$' && (out == 0 || marked[out-1] != '\x02')) {
            // Add \x02 marker before $ if not already marked
            marked[out++] = '\x02';
        }
        marked[out++] = *p++;
    }
    marked[out] = '\0';
    return marked;
}

// The special parameters that are not positional: $ ? ! - # @ *. They expand
// the same with and without braces, in and out of double quotes.
static bool is_special_param(char c) {
    return c != '\0' && strchr("$?!-#@*", c) != NULL;
}

// Copy a positional parameter into a "$*" / "$@" buffer with its marker-range
// bytes protected (\x01 + byte), so they are not read as the \x04 field
// separators the buffer uses. Returns the bytes written; stops when out of room.
static size_t copy_param_protected(char *dst, size_t room, const char *param) {
    size_t out = 0;
    for (const char *p = param; *p; p++) {
        size_t need = is_marker_byte(*p) ? 2 : 1;
        if (out + need > room) {
            break;
        }
        if (need == 2) {
            dst[out++] = '\x01';
        }
        dst[out++] = *p;
    }
    return out;
}

// Value of the special parameter c. Numbers are written to buf; "$*" and "$@"
// come back in static buffers with the field separators the append step
// expects: \x04 between fields, or the first character of IFS for a quoted
// "$*" (none when IFS is empty).
static const char *special_param_value(char c, bool is_quoted, int last_exit_code,
                                       char *buf, size_t bufsz) {
    switch (c) {
        case '?':   // exit status of the last command
            snprintf(buf, bufsz, "%d", last_exit_code);
            return buf;
        case '$':   // process ID of the shell, the same inside its subshells
            snprintf(buf, bufsz, "%d", (int)shell_pid());
            return buf;
        case '!': {  // process ID of the last background job, empty before any
            pid_t bg_pid = jobs_get_last_bg_pid();
            if (bg_pid > 0) {
                snprintf(buf, bufsz, "%d", bg_pid);
                return buf;
            }
            return "";
        }
        case '-':   // the option letters in effect
            shell_options_flags(buf, bufsz);
            return buf;
        case '#': {  // number of positional parameters (excluding $0)
            int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
            snprintf(buf, bufsz, "%d", count);
            return buf;
        }
        case '*': {
            // POSIX: quoted "$*" joins with the first character of IFS into one
            // field; unquoted, each parameter is its own field (\x04 separator)
            static char star_buf[MAX_EXPANDED_LENGTH];
            size_t pos = 0;
            char sep = '\x04';
            bool use_sep = true;
            if (is_quoted) {
                const char *ifs = ifs_get();
                if (ifs[0] == '\0') {
                    use_sep = false;
                } else {
                    sep = ifs[0];
                }
            }
            for (int i = 1; i < script_state.positional_count && pos < sizeof(star_buf) - 1; i++) {
                const char *param = get_positional_param(i);
                if (param) {
                    if (i > 1 && use_sep && pos < sizeof(star_buf) - 1) {
                        star_buf[pos++] = sep;
                    }
                    pos += copy_param_protected(star_buf + pos, sizeof(star_buf) - 1 - pos, param);
                }
            }
            star_buf[pos] = '\0';
            return star_buf;
        }
        case '@': {
            // Quoted or not, each parameter is its own field (\x04 separator)
            static char at_buf[MAX_EXPANDED_LENGTH];
            size_t pos = 0;
            for (int i = 1; i < script_state.positional_count && pos < sizeof(at_buf) - 1; i++) {
                const char *param = get_positional_param(i);
                if (param) {
                    if (i > 1 && pos < sizeof(at_buf) - 1) {
                        at_buf[pos++] = '\x04';
                    }
                    pos += copy_param_protected(at_buf + pos, sizeof(at_buf) - 1 - pos, param);
                }
            }
            at_buf[pos] = '\0';
            return at_buf;
        }
        default:
            return NULL;
    }
}

// Expand environment variables in a string
char *varexpand_expand(const char *str, int last_exit_code) {
    if (!str) return NULL;

    // Allocate proportionally to input size (expansion can grow the string)
    size_t input_len = strlen(str);
    size_t result_size = input_len * 2 + 4096;
    if (result_size < MAX_EXPANDED_LENGTH) {
        result_size = MAX_EXPANDED_LENGTH;
    }

    char *result = malloc(result_size);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    while (*p && out_pos < result_size - 1) {
        if (*p == '\x01' && *(p + 1) == '\\' && *(p + 2) == '\\') {
            // Protected double backslash from single quotes - output both literally
            result[out_pos++] = '\\';
            result[out_pos++] = '\\';
            p += 3;
            continue;
        } else if (*p == '\x01' && *(p + 1) == '\\' && char_in_string(*(p + 2), "$`")) {
            // Protected backslash from single quotes - output \$ or \` literally
            result[out_pos++] = '\\';
            result[out_pos++] = *(p + 2);
            p += 3;
            continue;
        } else if (*p == '\\' && *(p + 1) == '\\') {
            // Double backslash outside single quotes - output one backslash and continue
            // This allows \\$ to become \ followed by expanded $var
            result[out_pos++] = '\\';
            p += 2;
            continue;
        } else if (char_in_string(*p, "\x01\\") && *(p + 1) == '$') {
            // Single-quoted or escaped dollar sign - output $ literally (no expansion)
            result[out_pos++] = '$';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '`') {
            // Escaped backtick - output ` literally (for heredocs)
            result[out_pos++] = '`';
            p += 2;
        } else if (*p == '\x01' && *(p + 1) && out_pos < result_size - 2) {
            // Any other protected byte: keep the pair whole so the byte after
            // the marker (a real \x02, say) is not read as a marker itself
            result[out_pos++] = *p++;
            result[out_pos++] = *p++;
        } else if ((*p == '\x02' && *(p + 1) == '$') || *p == '$') {
            // Variable expansion - check for quoted marker first
            bool is_quoted = (*p == '\x02');
            if (is_quoted) p++;  // Skip \x02 marker if present
            p++;  // Skip $

            const char *var_value = NULL;
            char var_name[256] = {0};
            bool is_literal_dollar = false;  // Track if this is just a literal $, not an expansion
            const char *premarked = NULL;    // a "$*"/"$@" buffer: its bytes are already protected

            // Special parameters ($ ? ! - # @ *). Inside double quotes the parser
            // puts \x02 before $ and \x01 before ? and *; skip that marker.
            const char *special = p;
            if ((*special == '\x01' || *special == '\x02') && *(special + 1)) {
                special++;
            }
            if (is_special_param(*special)) {
                var_value = special_param_value(*special, is_quoted, last_exit_code, var_name, sizeof(var_name));
                if (*special == '@' || *special == '*') {
                    premarked = var_value;
                }
                p = special + 1;
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

                // The name: a special parameter (${$} ${?} ${!} ${-} ${@} ${*}, and
                // ${#X} for their lengths) is one character; otherwise letters,
                // digits and underscores
                size_t name_len = 0;
                bool is_special = false;
                const char *special = p;
                if ((*special == '\x01' || *special == '\x02') && *(special + 1)) {
                    special++;
                }
                if (is_special_param(*special) && *special != '#') {
                    var_name[0] = *special;
                    var_name[1] = '\0';
                    name_len = 1;
                    is_special = true;
                    p = special + 1;
                } else {
                    while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                        var_name[name_len++] = *p++;
                    }
                    var_name[name_len] = '\0';
                }

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

                if (char_in_string(*p, "-+=?#%")) {
                    modifier = *p++;
                    // Check for ## or %%
                    if (char_in_string(modifier, "#%") && *p == modifier) {
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
                        int is_positional = 0;
                        if (!is_special) {
                            is_positional = 1;
                            for (size_t i = 0; i < name_len; i++) {
                                if (!isdigit(var_name[i])) {
                                    is_positional = 0;
                                    break;
                                }
                            }
                        }
                        static char special_buf[256];
                        if (is_special) {
                            val = special_param_value(var_name[0], is_quoted, last_exit_code,
                                                      special_buf, sizeof(special_buf));
                            if (var_name[0] == '@' || var_name[0] == '*') {
                                premarked = val;
                            }
                        } else if (is_positional) {
                            int param_num = atoi(var_name);
                            val = get_positional_param(param_num);
                        } else if (strcmp(var_name, "LINENO") == 0) {
                            val = exec_lineno_string();
                        } else {
                            val = shellvar_get(var_name);
                        }

                        // Determine if variable is "unset" or "null"
                        bool is_unset = (val == NULL);
                        bool is_null = (val != NULL && val[0] == '\0');

                        // Apply modifier
                        // Note: POSIX requires the word to be expanded when used
                        static char expanded_word_buf[4096];
                        const char *effective_word = word;

                        if (modifier == '-') {
                            // ${var-word}: use word if unset
                            // ${var:-word}: use word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // Expand variables in word, preserving quoting context
                                if (strchr(word, '$')) {
                                    const char *word_to_expand = is_quoted ? mark_dollars_as_quoted(word) : word;
                                    char *expanded_word = varexpand_expand(word_to_expand, last_exit_code);
                                    if (expanded_word) {
                                        safe_strcpy(expanded_word_buf, expanded_word, sizeof(expanded_word_buf));
                                        free(expanded_word);
                                        effective_word = expanded_word_buf;
                                    }
                                }
                                var_value = effective_word;
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '+') {
                            // ${var+word}: use word if set
                            // ${var:+word}: use word if set and not null
                            if (!is_unset && (!check_null || !is_null)) {
                                // Expand variables in word, preserving quoting context
                                if (strchr(word, '$')) {
                                    const char *word_to_expand = is_quoted ? mark_dollars_as_quoted(word) : word;
                                    char *expanded_word = varexpand_expand(word_to_expand, last_exit_code);
                                    if (expanded_word) {
                                        safe_strcpy(expanded_word_buf, expanded_word, sizeof(expanded_word_buf));
                                        free(expanded_word);
                                        effective_word = expanded_word_buf;
                                    }
                                }
                                var_value = effective_word;
                            } else {
                                var_value = "";
                            }
                        } else if (modifier == '=' && is_special) {
                            // ${?=word}: a special parameter cannot be assigned
                            fprintf(stderr, "%s: ${%s=%s}: cannot assign in this way\n", HASH_NAME, var_name, word);
                            varexpand_error = true;
                            var_value = "";
                        } else if (modifier == '=') {
                            // ${var=word}: assign word if unset
                            // ${var:=word}: assign word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // Expand variables in word, preserving quoting context
                                if (strchr(word, '$')) {
                                    const char *word_to_expand = is_quoted ? mark_dollars_as_quoted(word) : word;
                                    char *expanded_word = varexpand_expand(word_to_expand, last_exit_code);
                                    if (expanded_word) {
                                        safe_strcpy(expanded_word_buf, expanded_word, sizeof(expanded_word_buf));
                                        free(expanded_word);
                                        effective_word = expanded_word_buf;
                                    }
                                }
                                setenv(var_name, effective_word, 1);
                                var_value = effective_word;
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '?') {
                            // ${var?word}: error if unset
                            // ${var:?word}: error if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // Expand variables in word for error message, preserving quoting context
                                if (strchr(word, '$')) {
                                    const char *word_to_expand = is_quoted ? mark_dollars_as_quoted(word) : word;
                                    char *expanded_word = varexpand_expand(word_to_expand, last_exit_code);
                                    if (expanded_word) {
                                        safe_strcpy(expanded_word_buf, expanded_word, sizeof(expanded_word_buf));
                                        free(expanded_word);
                                        effective_word = expanded_word_buf;
                                    }
                                }
                                if (effective_word[0]) {
                                    fprintf(stderr, "%s: %s\n", var_name, effective_word);
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

                                // Convert \x01 markers to fnmatch escapes
                                const char *fnmatch_pattern = convert_pattern_for_fnmatch(word);

                                // Try matching pattern at start of string
                                // For ##, find longest match; for #, find shortest
                                if (double_modifier) {
                                    // Longest match - try from end backwards
                                    for (size_t i = val_len; i > 0; i--) {
                                        char saved = pattern_result[i];
                                        pattern_result[i] = '\0';
                                        if (fnmatch(fnmatch_pattern, pattern_result, 0) == 0) {
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
                                        if (fnmatch(fnmatch_pattern, pattern_result, 0) == 0) {
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

                                // Convert \x01 markers to fnmatch escapes
                                const char *fnmatch_pattern = convert_pattern_for_fnmatch(word);

                                // Try matching pattern at end of string
                                // For %%, find longest match; for %, find shortest
                                if (double_modifier) {
                                    // Longest match - try from start forwards
                                    for (size_t i = 0; i < val_len; i++) {
                                        if (fnmatch(fnmatch_pattern, val + i, 0) == 0) {
                                            keep_len = i;
                                            break;
                                        }
                                    }
                                } else {
                                    // Shortest match - find shortest suffix that matches
                                    keep_len = val_len;
                                    for (size_t i = val_len; i > 0; i--) {
                                        if (fnmatch(fnmatch_pattern, val + i - 1, 0) == 0) {
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
                    var_value = (strcmp(var_name, "LINENO") == 0) ? exec_lineno_string() : shellvar_get(var_name);
                    // Check for unset variable with -u option
                    if (var_value == NULL) {
                        check_unset_error(var_name);
                    }
                }
            } else {
                // Just a $ followed by something else, keep the $
                if (out_pos < result_size - 1) {
                    result[out_pos++] = '$';
                }
                is_literal_dollar = true;  // Not a variable expansion
            }

append_value:
            // Append variable value if found, or handle empty/unset expansion
            // Skip if this was just a literal $ (not a variable expansion)
            if (is_literal_dollar) {
                // Do nothing - literal $ was already output
            } else if (var_value) {
                size_t val_len = strlen(var_value);
                if (val_len > 0) {
                    bool keep_pairs = (premarked != NULL && var_value == premarked);
                    if (is_quoted) {
                        // Append with markers to prevent glob expansion and quote interpretation
                        for (size_t i = 0; i < val_len && out_pos < result_size - 2; i++) {
                            char c = var_value[i];
                            if (keep_pairs && c == '\x01' && i + 1 < val_len) {
                                result[out_pos++] = c;          // an already protected byte
                                result[out_pos++] = var_value[++i];
                                continue;
                            }
                            if (keep_pairs && c == '\x04') {
                                result[out_pos++] = c;          // the field separator itself
                                continue;
                            }
                            if (char_in_string(c, "*?[]'\"\\<|>&~") || is_marker_byte(c)) {
                                // Add marker before special characters (globs, quotes, redirects, operators, tilde, bracket close)
                                // and before a real marker-range byte, which travels as \x01 + byte
                                result[out_pos++] = '\x01';
                            }
                            result[out_pos++] = c;
                        }
                    } else {
                        // Unquoted - wrap with \x03 markers for IFS splitting; a
                        // real marker-range byte in the value travels as \x01 + byte
                        if (out_pos + 2 < result_size - 1) {
                            result[out_pos++] = '\x03';  // Start marker
                            for (size_t i = 0; i < val_len && out_pos < result_size - 3; i++) {
                                char c = var_value[i];
                                if (keep_pairs && c == '\x01' && i + 1 < val_len) {
                                    result[out_pos++] = c;      // an already protected byte
                                    result[out_pos++] = var_value[++i];
                                    continue;
                                }
                                if (!keep_pairs && is_marker_byte(c)) {
                                    result[out_pos++] = '\x01';
                                }
                                result[out_pos++] = c;          // a \x04 separator stays raw
                            }
                            result[out_pos++] = '\x03';  // End marker
                        }
                    }
                } else if (!is_quoted && out_pos < result_size - 2) {
                    // Empty unquoted expansion - add markers so IFS splitting can remove it
                    // POSIX: empty unquoted expansion produces no field
                    result[out_pos++] = '\x03';
                    result[out_pos++] = '\x03';
                }
            } else if (!is_quoted && out_pos < result_size - 2) {
                // Unset variable in unquoted context - add empty markers for removal
                // POSIX: unset variable in unquoted expansion produces no field
                result[out_pos++] = '\x03';
                result[out_pos++] = '\x03';
            }
            // Quoted unset variables expand to empty string (kept as argument)

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
