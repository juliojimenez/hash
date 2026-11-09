#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "script.h"
#include "hash.h"
#include "safe_string.h"
#include "execute.h"
#include "config.h"
#include "history.h"
#include "exec.h"
#include "varexpand.h"
#include "arith.h"
#include "cmdsub.h"
#include "utils.h"

// Global script state
ScriptState script_state;

// Break/continue pending levels (for POSIX dynamic scoping)
// These track how many loop levels to break/continue across function boundaries
int break_pending = 0;
int continue_pending = 0;

// Return pending flag (for return in if/while conditions)
static bool return_pending = false;

// ============================================================================
// Initialization and Cleanup
// ============================================================================

void script_init(void) {
    memset(&script_state, 0, sizeof(script_state));
    script_state.context_depth = 0;
    script_state.function_count = 0;
    script_state.in_script = false;
    script_state.script_path = NULL;
    script_state.script_line = 0;
    script_state.silent_errors = false;
    script_state.positional_params = NULL;
    script_state.positional_count = 0;
    script_state.function_call_depth = 0;
    script_state.exit_requested = false;
    script_state.record_history = false;
}

void script_cleanup(void) {
    // Free function bodies
    for (int i = 0; i < script_state.function_count; i++) {
        free(script_state.functions[i].body);
    }

    // Free positional parameters
    if (script_state.positional_params) {
        for (int i = 0; i < script_state.positional_count; i++) {
            free(script_state.positional_params[i]);
        }
        free(script_state.positional_params);
    }

    script_init();  // Reset to clean state
}

// ============================================================================
// Line Splitting (handle semicolons in compound commands)
// ============================================================================

// ============================================================================
// Context Stack Management
// ============================================================================

int script_count_loops_at_current_depth(void) {
    int count = 0;
    bool dynamic_scoping = shell_option_nonlexicalctrl();

    int current_func_depth = script_state.function_call_depth;
    for (int i = 0; i < script_state.context_depth; i++) {
        const ScriptContext *ctx = &script_state.context_stack[i];
        if (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL) {
            // With dynamic scoping (nonlexicalctrl), count ALL loops
            // With lexical scoping (default), only count loops at current function depth
            if (dynamic_scoping || ctx->function_call_depth == current_func_depth) {
                count++;
            }
        }
    }
    return count;
}

// Get/set break pending levels (for POSIX dynamic scoping)
int script_get_break_pending(void) {
    return break_pending;
}

void script_set_break_pending(int levels) {
    break_pending = levels;
}

// Get/set continue pending levels (for POSIX dynamic scoping)
int script_get_continue_pending(void) {
    return continue_pending;
}

void script_set_continue_pending(int levels) {
    continue_pending = levels;
}

// Reset script state for subshell
// POSIX: break/continue in a subshell should only affect loops within that subshell,
// not loops in the parent shell. When we fork a subshell, we need to clear the
// inherited loop context so that break/continue don't see parent's loops.
void script_reset_for_subshell(void) {
    // Clear break/continue pending state
    break_pending = 0;
    continue_pending = 0;

    // Clear the context stack - subshell starts fresh without parent's loops
    script_state.context_depth = 0;

    // Reset function call depth for the subshell
    script_state.function_call_depth = 0;
}

void script_set_return_pending(bool pending) {
    return_pending = pending;
}

int script_push_context(ContextType type) {
    if (script_state.context_depth >= MAX_SCRIPT_DEPTH) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: maximum nesting depth exceeded\n", HASH_NAME);
        }
        return -1;
    }

    ScriptContext *ctx = &script_state.context_stack[script_state.context_depth];
    ctx->type = type;
    ctx->function_call_depth = script_state.function_call_depth;  // Track when this context was created

    script_state.context_depth++;
    return 1;  // Success, continue processing
}

int script_pop_context(void) {
    if (script_state.context_depth <= 0) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: context stack underflow\n", HASH_NAME);
        }
        return -1;
    }

    script_state.context_depth--;
    return 1;  // Success, continue processing
}

// ============================================================================
// Function Management
// ============================================================================

int script_define_function(const char *name, const char *body) {
    if (!name || !body) return -1;

    extern int last_command_exit_code;

    for (int i = 0; i < script_state.function_count; i++) {
        if (strcmp(script_state.functions[i].name, name) == 0) {
            free(script_state.functions[i].body);
            script_state.functions[i].body = strdup(body);
            script_state.functions[i].body_len = strlen(body);
            script_state.functions[i].def_line = 0;
            // POSIX: function definition sets exit code to 0
            last_command_exit_code = 0;
            return 0;
        }
    }

    if (script_state.function_count >= MAX_FUNCTIONS) {
        fprintf(stderr, "%s: too many functions\n", HASH_NAME);
        return -1;
    }

    ShellFunction *func = &script_state.functions[script_state.function_count];
    safe_strcpy(func->name, name, MAX_FUNC_NAME);
    func->body = strdup(body);
    func->body_len = strlen(body);
    func->def_line = 0;

    if (!func->body) return -1;

    script_state.function_count++;
    // POSIX: function definition sets exit code to 0
    last_command_exit_code = 0;
    return 0;
}

ShellFunction *script_get_function(const char *name) {
    if (!name) return NULL;

    for (int i = 0; i < script_state.function_count; i++) {
        if (strcmp(script_state.functions[i].name, name) == 0) {
            return &script_state.functions[i];
        }
    }
    return NULL;
}

// ============================================================================
// Case Word and Pattern Expansion
// ============================================================================

// Forward declarations for case pattern matching
static char *remove_shell_quotes(const char *str);
static char *expand_case_word(const char *word);
static char *expand_case_pattern(const char *pattern);

// Expand case word using all expansion types (variable, arithmetic, command substitution)
// Remove shell quotes from a string (for case word expansion)
// Respects \x01 markers which indicate protected literal characters
static char *remove_shell_quotes(const char *str) {
    if (!str) return strdup("");

    size_t len = strlen(str);
    char *result = malloc(len + 1);
    if (!result) return strdup("");

    size_t j = 0;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < len; i++) {
        char c = str[i];

        // Handle \x01 marker - next char is literal (from expansion)
        if (c == '\x01' && str[i + 1]) {
            i++;  // Skip marker
            result[j++] = str[i];  // Copy literal character
            continue;
        }

        if (c == '\'' && !in_double) {
            // Single quote - toggle state, don't output the quote
            in_single = !in_single;
            continue;
        }

        if (c == '"' && !in_single) {
            // Double quote - toggle state, don't output the quote
            in_double = !in_double;
            continue;
        }

        if (c == '\\' && !in_single && str[i + 1]) {
            // Backslash escape outside single quotes
            if (in_double) {
                // In double quotes, backslash only escapes $ ` " \ newline
                char next = str[i + 1];
                if (char_in_string(next, "$`\"\\\n")) {
                    i++;  // Skip backslash
                    if (next != '\n') {  // Backslash-newline is removed entirely
                        result[j++] = next;
                    }
                    continue;
                }
                // Other backslashes in double quotes are literal
            } else {
                // Outside quotes, backslash escapes the next character
                i++;  // Skip backslash
                if (str[i] != '\n') {  // Backslash-newline is removed entirely
                    result[j++] = str[i];
                }
                continue;
            }
        }

        result[j++] = c;
    }

    result[j] = '\0';
    return result;
}

// fnmatch on macOS misreads a bracket expression that has an escaped ] after
// an escaped backslash: in [\\\]a] the backslash is not a member there (glibc
// reads it as POSIX says). Both agree on the form POSIX describes without
// escapes: a ] member first, a - member last. Rewrite every bracket expression
// of an fnmatch pattern that way, in place (the result is never longer).
static void canonicalize_bracket_members(char *pat) {
    size_t n = strlen(pat);
    for (size_t i = 0; i < n; i++) {
        if (pat[i] == '\\' && pat[i + 1]) {
            i++;
            continue;
        }
        if (pat[i] != '[') continue;
        size_t j = i + 1;
        if (pat[j] == '!' || pat[j] == '^') j++;
        size_t members = j;
        if (pat[j] == ']') j++;             // a ] right after [ or [! is a member
        bool closed = false;
        while (pat[j]) {
            if (pat[j] == '\\' && pat[j + 1]) {
                j += 2;
                continue;
            }
            if (pat[j] == '[' && pat[j + 1] == ':') {
                const char *end = strstr(pat + j + 2, ":]");
                if (!end) break;
                j = (size_t)(end - pat) + 2;
                continue;
            }
            if (pat[j] == ']') {
                closed = true;
                break;
            }
            j++;
        }
        if (!closed) continue;              // a [ with no ]: a literal character
        char *kept = malloc(j - members + 1);
        if (!kept) return;
        size_t k = 0;
        bool has_rbracket = false, has_dash = false;
        for (size_t p = members; p < j;) {
            if (p == members && pat[p] == ']') {
                has_rbracket = true;
                p++;
            } else if (pat[p] == '\\' && pat[p + 1] == ']') {
                has_rbracket = true;
                p += 2;
            } else if (pat[p] == '\\' && pat[p + 1] == '-') {
                has_dash = true;
                p += 2;
            } else if (pat[p] == '-' && (p == members || p + 1 == j)) {
                has_dash = true;             // a - that is not a range operator
                p++;
            } else if (pat[p] == '\\' && pat[p + 1]) {
                kept[k++] = pat[p++];
                kept[k++] = pat[p++];
            } else {
                kept[k++] = pat[p++];
            }
        }
        size_t w = members;
        if (has_rbracket) pat[w++] = ']';
        memcpy(pat + w, kept, k);
        w += k;
        if (has_dash) pat[w++] = '-';
        free(kept);
        if (w < j) {                        // \] and \- lost their backslashes: close the gap
            memmove(pat + w, pat + j, n - j + 1);
            n -= j - w;
            j = w;
        }
        pat[j] = ']';
        i = j;
    }
}

// Remove shell quotes for case patterns - similar to remove_shell_quotes but
// escapes backslashes that are meant to be literal (from inside single quotes)
// so fnmatch treats them correctly
static char *remove_shell_quotes_for_pattern(const char *str) {
    if (!str) return strdup("");

    size_t len = strlen(str);
    // Worst case: every char is a backslash that needs escaping
    char *result = malloc(len * 2 + 1);
    if (!result) return strdup("");

    size_t j = 0;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < len; i++) {
        char c = str[i];

        // Handle \x01 marker - next char is literal (from expansion)
        // Need to escape fnmatch special characters
        if (c == '\x01' && str[i + 1]) {
            i++;  // Skip marker
            char next = str[i];
            // Escape fnmatch pattern metacharacters: * ? [ ] and backslash
            if (char_in_string(next, "\\*?[]")) {
                result[j++] = '\\';  // Escape for fnmatch
            }
            result[j++] = next;  // Copy literal character
            continue;
        }
        // Field markers around an unquoted expansion mean nothing in a pattern
        if (c == '\x03' || c == '\x04') {
            continue;
        }

        if (c == '\'' && !in_double) {
            // Single quote - toggle state, don't output the quote
            in_single = !in_single;
            continue;
        }

        if (c == '"' && !in_single) {
            // Double quote - toggle state, don't output the quote
            in_double = !in_double;
            continue;
        }

        if (c == '\\') {
            if (in_single) {
                // Inside single quotes, backslash is literal - escape for fnmatch
                result[j++] = '\\';
                result[j++] = '\\';
                continue;
            } else if (str[i + 1]) {
                // Backslash escape outside single quotes
                if (in_double) {
                    // In double quotes, backslash only escapes $ ` " \ newline
                    char next = str[i + 1];
                    if (char_in_string(next, "$`\"\\\n")) {
                        i++;  // Skip backslash
                        if (next == '\\') {
                            // Escaped backslash - output as escaped for fnmatch
                            result[j++] = '\\';
                            result[j++] = '\\';
                        } else if (next != '\n') {  // Backslash-newline is removed
                            result[j++] = next;
                        }
                        continue;
                    }
                    // Other backslashes in double quotes are literal
                    result[j++] = '\\';
                    result[j++] = '\\';
                    continue;
                } else {
                    // Outside quotes, backslash escapes the next character
                    // Keep the escape sequence for fnmatch
                    result[j++] = '\\';
                    i++;
                    if (str[i] == '\x01' && str[i + 1]) {
                        i++;  // a protected byte: the byte itself is the escaped character
                    }
                    if (str[i] != '\n') {  // Backslash-newline is removed
                        result[j++] = str[i];
                    }
                    continue;
                }
            }
        }

        // A quoted * ? [ ] is a literal character, not a pattern character
        if ((in_single || in_double) && char_in_string(c, "*?[]")) {
            result[j++] = '\\';
        }
        result[j++] = c;
    }

    result[j] = '\0';
    return result;
}

// Mark a case word or pattern for the expansion passes. The text still has
// its quotes, which varexpand does not understand, so this pass tells it what
// the quotes mean:
//   \x02 before $            a quoted expansion: the result is literal (in
//                            double quotes; every $ when all_literal)
//   \x01 before $            a $ in single quotes: no expansion at all
//   \x01 before \\ (pair)    keep both backslashes; without it varexpand
//                            collapses \\ to \ and quote removal then takes
//                            the survivor as an escape (case \\z lost its \)
//   \x01 before \$ and \`    inside single quotes: keep the backslash too
// Everything else goes through untouched and quote removal handles it.
static char *mark_case_text(const char *word, bool all_literal) {
    if (!word) return strdup("");

    size_t len = strlen(word);
    char *result = malloc(len * 2 + 1);
    if (!result) return strdup(word);

    size_t j = 0;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < len; i++) {
        char c = word[i];

        if (is_marker_byte(c)) {
            // A real marker-range byte in the text travels as \x01 + byte
            result[j++] = '\x01';
            result[j++] = c;
            continue;
        }
        // The body of $(...), $((...)) or `...` is a command (or expression) of
        // its own: copy it verbatim, so its quoted $ are not marked here (a
        // \x02 inside the command text reached the child as a path byte:
        // `case $(f "${d}/${n}") in` exec'd "\x02/bin/\x02sh")
        if (!in_single && !in_double && c == '$' && word[i + 1] == '\'') {
            // $'...': decoded here; every byte is quoted text
            char *decoded = malloc(len + 1);
            size_t n = 0;
            const char *end = decoded ? ansi_c_decode(word + i + 1, decoded, &n) : NULL;
            if (end) {
                for (size_t k = 0; k < n; k++) {
                    if (is_marker_byte(decoded[k]) || char_in_string(decoded[k], "$`\\*?[]~<>|&;()\"'")) {
                        result[j++] = '\x01';
                    }
                    result[j++] = decoded[k];
                }
                free(decoded);
                i = (size_t)(end - word) - 1;
                continue;
            }
            free(decoded);
        }
        if (!in_single && c == '$' && word[i + 1] == '(') {
            size_t k = i + 2;
            int depth = 1;
            bool q1 = false, q2 = false;
            while (word[k] && depth > 0) {
                char d = word[k];
                if (d == '\\' && word[k + 1] && !q1) { k += 2; continue; }
                if (d == '\'' && !q2) q1 = !q1;
                else if (d == '"' && !q1) q2 = !q2;
                else if (!q1 && !q2) {
                    if (d == '(') depth++;
                    else if (d == ')') depth--;
                }
                k++;
            }
            memcpy(result + j, word + i, k - i);
            j += k - i;
            i = k - 1;
            continue;
        }
        if (!in_single && c == '`') {
            size_t k = i + 1;
            while (word[k] && word[k] != '`') {
                if (word[k] == '\\' && word[k + 1]) k++;
                k++;
            }
            if (word[k] == '`') k++;
            memcpy(result + j, word + i, k - i);
            j += k - i;
            i = k - 1;
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            result[j++] = c;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            result[j++] = c;
            continue;
        }

        if (c == '\\' && in_single) {
            // A literal byte: marked, and the byte after it is left to its own
            // handling ('a \'$x: the quote closes and $x expands; the
            // backslash used to swallow the quote and leave $x unexpanded)
            result[j++] = '\x01';
            result[j++] = c;
            continue;
        }
        if (c == '\\' && word[i + 1]) {
            char next = word[i + 1];
            if (next == '\\') {
                result[j++] = '\x01';
                result[j++] = c;
                result[j++] = next;
                i++;
                continue;
            }
            if (is_marker_byte(next)) {
                // An escaped control byte in the marker range: the byte travels
                // protected, or the expander reads it as a marker and takes the
                // byte after it with it (a `\$` later in the same pattern was
                // then read as "literal backslash, expand $": `*[\<01>...\$...]*`
                // matched nothing, modernish match.t 018)
                result[j++] = c;
                result[j++] = '\x01';
                result[j++] = next;
                i++;
                continue;
            }
            // \$ and \` outside single quotes: varexpand drops the backslash
            // and keeps the character, which is what quote removal would do.
            // Any other escape passes through for quote removal.
            result[j++] = c;
            result[j++] = next;
            i++;
            continue;
        }

        if (c == '$') {
            if (in_single) {
                result[j++] = '\x01';
            } else if (word[i + 1] == '(' && word[i + 2] == '(') {
                ;   // $((...)): arithmetic, never marked (a \x02 would make cmdsub_expand
                    // read it as a quoted $( ) command substitution); its result is digits
            } else if (in_double || all_literal) {
                result[j++] = '\x02';
            }
        }

        result[j++] = c;
    }

    result[j] = '\0';
    return result;
}

// Case word: expansions in double quotes are literal, the rest as usual
static char *add_quote_markers(const char *word) {
    return mark_case_text(word, false);
}

// Case pattern: the same marking. An unquoted expansion's result is pattern
// text (POSIX 2.9.4.3; `p='[+-]*'; case --x in ($p)` matches), a quoted one
// matches literally.
static char *add_pattern_markers(const char *word) {
    return mark_case_text(word, false);
}

static char *expand_case_word(const char *word) {
    if (!word) return strdup("");

    extern int last_command_exit_code;

    // Add quote markers to indicate quoted context to varexpand
    char *marked = add_quote_markers(word);
    if (!marked) return strdup("");

    char *result = marked;

    // Apply command substitution expansion
    char *cmdsub = cmdsub_expand(result);
    if (cmdsub) {
        free(result);
        result = cmdsub;
    }

    // Apply arithmetic expansion
    char *arith = arith_expand(result);
    if (arith) {
        free(result);
        result = arith;
    }

    // Apply variable expansion; the case word is never field-split, so $* in it
    // joins with the first IFS character
    bool was = varexpand_set_no_split(true);
    char *varexp = varexpand_expand(result, last_command_exit_code);
    varexpand_set_no_split(was);
    if (varexp) {
        free(result);
        result = varexp;
    }

    // Process \x03 IFS markers from expansion
    // Case words don't undergo IFS splitting, but we need to:
    // 1. Protect backslashes and quotes that came from expansion with \x01 markers
    //    so remove_shell_quotes won't re-interpret them
    // 2. Remove the \x03 markers themselves
    {
        size_t len = strlen(result);
        // Worst case: every char in expansion needs a marker
        char *new_result = malloc(len * 2 + 1);
        if (new_result) {
            const char *read = result;
            char *write = new_result;
            bool in_expansion = false;
            while (*read) {
                if (*read == '\x01' && read[1]) {
                    // A protected byte (a real \x03 in the value, say): keep
                    // the pair whole so it is not read as a marker here
                    *write++ = *read++;
                    *write++ = *read++;
                    continue;
                }
                if (*read == '\x03') {
                    in_expansion = !in_expansion;
                    read++;
                    continue;
                }
                if (*read == '\x04') {
                    // "$@" in a case word: the fields are joined with a space
                    *write++ = ' ';
                    read++;
                    continue;
                }
                // Within expansion regions, protect backslashes and quotes
                // so remove_shell_quotes treats them as literal
                if (in_expansion && char_in_string(*read, "\\'\"")) {
                    *write++ = '\x01';
                }
                *write++ = *read++;
            }
            *write = '\0';
            free(result);
            result = new_result;
        }
    }

    // Remove shell quotes (quote removal phase of word expansion)
    char *unquoted = remove_shell_quotes(result);
    if (unquoted) {
        free(result);
        result = unquoted;
    }

    return result;
}

// Expand case pattern - similar to expand_case_word but marks ALL $ as quoted
// so expanded characters are treated as literals
static char *expand_case_pattern(const char *pattern) {
    if (!pattern) return strdup("");

    extern int last_command_exit_code;

    // Mark ALL $ to indicate all expansions should protect special chars
    char *marked = add_pattern_markers(pattern);
    if (!marked) return strdup("");

    char *result = marked;

    // Apply command substitution expansion
    char *cmdsub = cmdsub_expand(result);
    if (cmdsub) {
        free(result);
        result = cmdsub;
    }

    // Apply arithmetic expansion
    char *arith = arith_expand(result);
    if (arith) {
        free(result);
        result = arith;
    }

    // Apply variable expansion (mark_case_text keeps \\ pairs and quoted $
    // away from it, so it is safe on every pattern)
    char *varexp = varexpand_expand(result, last_command_exit_code);
    if (varexp) {
        free(result);
        result = varexp;
    }

    // A quote character that came out of an unquoted expansion is a plain
    // character, not a quote (foo='"'; case '"' in $foo) matches): protect
    // the quotes inside the \x03 regions before quote removal. Backslashes
    // stay as they are: in an unquoted expansion they escape for the pattern.
    {
        size_t len = strlen(result);
        char *marked_regions = malloc(len * 2 + 1);
        if (marked_regions) {
            const char *read = result;
            char *write = marked_regions;
            bool in_region = false;
            while (*read) {
                if (*read == '\x01' && read[1]) {
                    *write++ = *read++;
                    *write++ = *read++;
                    continue;
                }
                if (*read == '\x03') {
                    in_region = !in_region;
                } else if (in_region && (*read == '"' || *read == '\'')) {
                    *write++ = '\x01';
                }
                *write++ = *read++;
            }
            *write = '\0';
            free(result);
            result = marked_regions;
        }
    }

    // Remove shell quotes (quote removal phase)
    // Use pattern-specific version that escapes literal backslashes for fnmatch
    char *unquoted = remove_shell_quotes_for_pattern(result);
    if (unquoted) {
        free(result);
        result = unquoted;
    }
    canonicalize_bracket_members(result);

    return result;
}

// ============================================================================
// Script File Execution
// ============================================================================

// Record a line the shell read as its own input into history.
// Only the script given on the command line together with -i qualifies.
// hash-shell -i script: each complete program of the script becomes one history
// entry, the lines joined with newlines as at the prompt
static void script_record_program(const char *text, void *ctx) {
    (void)ctx;
    if (!is_interactive || shell_option_nolog()) {
        return;
    }
    size_t len = strlen(text);
    while (len > 0 && text[len - 1] == '\n') {
        len--;
    }
    if (len == 0) {
        return;
    }
    char *entry = malloc(len + 1);
    if (!entry) {
        return;
    }
    memcpy(entry, text, len);
    entry[len] = '\0';
    history_add(entry);
    free(entry);
}

// Feed the file line by line so each complete command runs as
// soon as it is read (and, for `hash-shell -i script`, lands in history)
static int execute_file_lines_ast(FILE *fp, bool record_history) {
    ExecFeeder feeder;
    exec_feeder_init(&feeder);
    if (record_history) {
        feeder.on_program = script_record_program;
    }
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    bool first = true;
    int result = 1;

    while ((n = getline(&line, &cap, fp)) != -1) {
        if (n > 0 && line[n - 1] == '\n') {
            line[n - 1] = '\0';
        }
        if (first) {
            first = false;
            if (line[0] == '#' && line[1] == '!') {
                exec_feeder_skip_line(&feeder);
                continue;  // shebang
            }
        }
        ExecStatus st = EXEC_OK;
        FeedResult fed = exec_feed_line(&feeder, line, &st);
        if (fed == FEED_SYNTAX_ERROR) {
            result = -1;
            break;
        }
        if (fed == FEED_RAN && (st == EXEC_EXIT || st == EXEC_ABORT)) {
            result = 0;  // exit, or the nesting limit: the file ends here
            break;
        }
        if (fed == FEED_RAN && st == EXEC_RETURN) {
            result = -2;
            break;
        }
    }
    if (result > 0 && exec_feeder_pending(&feeder)) {
        exec_report_syntax_error("syntax error: unexpected end of file", feeder.next_line - 1);
        last_command_exit_code = 2;
        result = -1;
    }
    exec_feeder_free(&feeder);
    free(line);
    return result;
}

int script_execute_file(const char *filepath, int argc, char **argv) {
    return script_execute_file_ex(filepath, argc, argv, false);
}

// Extended version with option to suppress errors
// Used for sourcing system files that may contain unsupported syntax
int script_execute_file_ex(const char *filepath, int argc, char **argv, bool silent_errors) {
    if (!filepath) return 1;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        if (!silent_errors && !script_state.silent_errors) {
            fprintf(stderr, "%s: cannot open '%s': ", HASH_NAME, filepath);
            perror("");
        }
        return 1;
    }

    // Move script FD to high number (10+) with close-on-exec
    // This keeps FDs 3-9 available for scripts and prevents child processes
    // from inheriting the script file descriptor
    int old_fd = fileno(fp);
    int new_fd = fcntl(old_fd, F_DUPFD_CLOEXEC, 10);
    if (new_fd >= 0) {
        FILE *new_fp = fdopen(new_fd, "r");
        if (new_fp) {
            fclose(fp);
            fp = new_fp;
        } else {
            close(new_fd);
        }
    }

    if (!exec_nesting_enter(".")) {
        fclose(fp);
        return 1;
    }

    // Save and set silent_errors flag
    // This flag is inherited by nested source operations
    bool old_silent = script_state.silent_errors;
    if (silent_errors) {
        script_state.silent_errors = true;
    }

    // POSIX lexical scoping: save and reset break/continue/return state for
    // sourced files; none of them may leak into the caller
    int old_break_pending = break_pending;
    int old_continue_pending = continue_pending;
    bool old_return_pending = return_pending;
    break_pending = 0;
    continue_pending = 0;
    return_pending = false;

    // Increment function call depth so break/continue only see loops in this file
    script_state.function_call_depth++;

    // Save context depth to detect unclosed structures only from THIS file
    int saved_context_depth = script_state.context_depth;

    // Only the script the shell was started on records its lines into history.
    // Clear the flag so any file it sources does not record.
    bool record_history = script_state.record_history;
    script_state.record_history = false;

    // Set up script state
    bool old_in_script = script_state.in_script;
    const char *old_script_path = script_state.script_path;
    script_state.in_script = true;
    script_state.script_path = filepath;
    script_state.script_line = 0;

    // Set positional parameters
    if (argc > 0 && argv) {
        script_state.positional_params = malloc(argc * sizeof(char*));
        if (script_state.positional_params) {
            for (int i = 0; i < argc; i++) {
                script_state.positional_params[i] = strdup(argv[i]);
            }
            script_state.positional_count = argc;
        }
    }

    int result = 1;  // 1 = continue, 0 = exit called, < 0 = error

    // Messages from this file name it; the caller's source comes back after
    const char *old_source = exec_source();
    exec_set_source(filepath);
    result = execute_file_lines_ast(fp, record_history);
    exec_set_source(old_source);

    fclose(fp);

    // Check for unclosed control structures (only those added by THIS file)
    if (script_state.context_depth > saved_context_depth) {
        if (!silent_errors && !script_state.silent_errors) {
            fprintf(stderr, "%s: %s: unexpected end of file\n", HASH_NAME, filepath);
        }
        // Clear only the contexts added by this file
        while (script_state.context_depth > saved_context_depth) {
            script_pop_context();
        }
        result = 1;
    }

    // Cleanup
    script_state.in_script = old_in_script;      // a nested source must not end the caller's script
    script_state.script_path = old_script_path;
    script_state.silent_errors = old_silent;  // Restore silent flag
    script_state.record_history = record_history;

    // POSIX lexical scoping: restore break/continue state
    // Any break/continue set inside the sourced file stays inside that file
    script_state.function_call_depth--;
    exec_nesting_leave();
    break_pending = old_break_pending;
    continue_pending = old_continue_pending;
    return_pending = old_return_pending;

    // For return (-2), use the last exit code
    // For other errors (< 0), return 1
    // A syntax error is 2 (POSIX); return (-2) and exit carry the last status
    if (result == -1) {
        return 2;
    }
    return (result < 0 && result != -2) ? 1 : execute_get_last_exit_code();
}

int script_execute_string(const char *script) {
    if (!script) return 0;

    return exec_string_status(script);
}

// Get a positional parameter value
const char *script_get_positional_param(int index) {
    if (index < 0 || index >= script_state.positional_count) {
        return NULL;
    }
    return script_state.positional_params ? script_state.positional_params[index] : NULL;
}

// Set positional parameters ($1, $2, etc.) from set builtin
// $0 is preserved, only $1 onwards are replaced
void script_set_positional_params(int argc, char **argv) {
    // Save $0 if it exists
    char *param0 = NULL;
    if (script_state.positional_count > 0 && script_state.positional_params &&
        script_state.positional_params[0]) {
        param0 = strdup(script_state.positional_params[0]);
    }

    // Free existing positional parameters
    if (script_state.positional_params) {
        for (int i = 0; i < script_state.positional_count; i++) {
            free(script_state.positional_params[i]);
        }
        free(script_state.positional_params);
        script_state.positional_params = NULL;
        script_state.positional_count = 0;
    }

    // Allocate new array: $0 + argc new parameters
    int new_count = 1 + argc;  // $0 plus the new args
    script_state.positional_params = malloc((size_t)new_count * sizeof(char*));
    if (!script_state.positional_params) {
        free(param0);
        return;
    }

    // Set $0 (restore saved or use empty string)
    script_state.positional_params[0] = param0 ? param0 : strdup("");

    // Copy new parameters to $1, $2, etc.
    for (int i = 0; i < argc; i++) {
        script_state.positional_params[i + 1] = argv[i] ? strdup(argv[i]) : strdup("");
    }

    script_state.positional_count = new_count;
}

// Track whether we're in a condition context (if/while/until condition)
// where errexit should not trigger
static bool in_condition_context = false;

void script_set_in_condition(bool in_condition) {
    in_condition_context = in_condition;
}

bool script_get_in_condition(void) {
    return in_condition_context;
}

// Public wrappers so other engines can match case patterns the same way
char *script_expand_case_word(const char *word) {
    return expand_case_word(word);
}

char *script_expand_case_pattern(const char *pattern) {
    return expand_case_pattern(pattern);
}
