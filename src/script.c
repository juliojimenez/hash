#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fnmatch.h>
#include "script.h"
#include "hash.h"
#include "safe_string.h"
#include "parser.h"
#include "execute.h"
#include "chain.h"
#include "colors.h"
#include "test_builtin.h"
#include "jobs.h"
#include "redirect.h"
#include "trap.h"
#include "shellvar.h"
#include "config.h"
#include "history.h"
#include "varexpand.h"
#include "arith.h"
#include "cmdsub.h"
#include "expand.h"

// Global script state
ScriptState script_state;

// Break/continue pending levels (for POSIX dynamic scoping)
// These track how many loop levels to break/continue across function boundaries
int break_pending = 0;
int continue_pending = 0;

// Heredoc state for collecting heredoc content
static char *heredoc_content = NULL;
static size_t heredoc_content_len = 0;
static size_t heredoc_content_cap = 0;

// Pending heredoc content for the next command
static char *pending_heredoc = NULL;
static int pending_heredoc_quoted = 0;

// ============================================================================
// Keywords Table
// ============================================================================

static const struct {
    const char *keyword;
    TokenType type;
} keywords[] = {
    { "if",       TOK_IF },
    { "then",     TOK_THEN },
    { "elif",     TOK_ELIF },
    { "else",     TOK_ELSE },
    { "fi",       TOK_FI },
    { "for",      TOK_FOR },
    { "while",    TOK_WHILE },
    { "until",    TOK_UNTIL },
    { "do",       TOK_DO },
    { "done",     TOK_DONE },
    { "case",     TOK_CASE },
    { "esac",     TOK_ESAC },
    { "in",       TOK_IN },
    { "function", TOK_FUNCTION },
    { "{",        TOK_LBRACE },
    { "}",        TOK_RBRACE },
    { NULL,       TOK_WORD }
};

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
}

void script_cleanup(void) {
    // Free function bodies
    for (int i = 0; i < script_state.function_count; i++) {
        free(script_state.functions[i].body);
    }

    // Free context stack resources
    for (int i = 0; i < script_state.context_depth; i++) {
        ScriptContext *ctx = &script_state.context_stack[i];
        free(ctx->loop_var);
        if (ctx->loop_values) {
            for (int j = 0; j < ctx->loop_count; j++) {
                free(ctx->loop_values[j]);
            }
            free(ctx->loop_values);
        }
        free(ctx->loop_body);
        free(ctx->loop_condition);
        free(ctx->case_word);
        free(ctx->func_name);
        free(ctx->func_body);
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
// Heredoc Support
// ============================================================================

// Reset heredoc state
static void heredoc_reset(void) {
    free(heredoc_content);
    heredoc_content = NULL;
    heredoc_content_len = 0;
    heredoc_content_cap = 0;
}

// Append line to heredoc content
static int heredoc_append(const char *line, int strip_tabs) {
    size_t line_len = strlen(line);
    const char *start = line;

    // Strip leading tabs if <<- was used
    if (strip_tabs) {
        while (*start == '\t') start++;
        line_len = strlen(start);
    }

    size_t needed = heredoc_content_len + line_len + 2; // +1 for newline, +1 for null

    if (needed > heredoc_content_cap) {
        size_t new_cap = heredoc_content_cap ? heredoc_content_cap * 2 : 1024;
        if (new_cap < needed) new_cap = needed;

        char *new_content = realloc(heredoc_content, new_cap);
        if (!new_content) return -1;
        heredoc_content = new_content;
        heredoc_content_cap = new_cap;
    }

    if (!heredoc_content) return -1;

    memcpy(heredoc_content + heredoc_content_len, start, line_len);
    heredoc_content_len += line_len;
    heredoc_content[heredoc_content_len++] = '\n';
    heredoc_content[heredoc_content_len] = '\0';

    return 0;
}

// Read a complete line from file, handling backslash-newline continuations
// and multi-line quoted strings
// Returns dynamically allocated string (caller must free), or NULL on EOF/error
static char *read_complete_line(FILE *fp) {
    char *result = NULL;
    size_t result_len = 0;
    size_t result_cap = 0;
    char buf[MAX_SCRIPT_LINE];
    bool in_single_quote = false;
    bool in_double_quote = false;
    int brace_depth = 0;  // Track ${...} depth - # inside braces is not a comment

    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);

        // Grow result buffer if needed
        size_t new_len = result_len + len + 1;
        if (new_len > result_cap) {
            size_t new_cap = result_cap ? result_cap * 2 : 256;
            if (new_cap < new_len) new_cap = new_len;
            char *new_result = realloc(result, new_cap);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
            result_cap = new_cap;
        }

        // Append buffer to result and track quote state
        bool in_comment = false;
        for (size_t i = 0; i < len; i++) {
            char c = buf[i];
            result[result_len++] = c;

            // Once in a comment, stop tracking quotes (comments end at newline)
            if (in_comment) {
                continue;
            }

            // Track quote state
            // Backslash escapes next character when not in single quotes
            if (c == '\\' && !in_single_quote && i + 1 < len) {
                i++;
                result[result_len++] = buf[i];
                continue;
            }
            if (c == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (c == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (c == '{' && !in_single_quote) {
                brace_depth++;
            } else if (c == '}' && !in_single_quote && brace_depth > 0) {
                brace_depth--;
            } else if (c == '#' && !in_single_quote && !in_double_quote && brace_depth == 0) {
                // Start of comment - stop tracking quotes for rest of line
                in_comment = true;
            }
        }
        result[result_len] = '\0';

        // Check for line continuation (backslash before newline)
        // Only do continuation when not inside single quotes
        // (POSIX: \<newline> is literal inside single quotes, continuation elsewhere)
        if (result_len >= 2 && result[result_len - 1] == '\n' && result[result_len - 2] == '\\' &&
            !in_single_quote) {
            // Remove backslash-newline and continue reading
            result_len -= 2;
            result[result_len] = '\0';
            continue;
        }

        // Continue reading if we're inside a quote (to complete multi-line strings)
        if (in_single_quote || in_double_quote) {
            continue;
        }

        // Remove trailing newline
        if (result_len > 0 && result[result_len - 1] == '\n') {
            result[result_len - 1] = '\0';
            result_len--;
        }

        return result;
    }

    // Return what we have (might be partial line at EOF)
    if (result && result_len > 0) {
        return result;
    }
    free(result);
    return NULL;
}

// Collect heredoc content from file until delimiter is found
// For unquoted heredocs (quoted=0), backslash-newline is treated as line continuation
// Returns the collected content (caller must free), or NULL on error
static char *heredoc_collect_from_file(FILE *fp, const char *delimiter, int strip_tabs, int quoted) {
    heredoc_reset();

    char line[MAX_SCRIPT_LINE];
    char *accumulated = NULL;  // For handling line continuations
    size_t accum_len = 0;
    size_t accum_cap = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }

        // Check for delimiter (only on raw lines, not accumulated)
        if (!accumulated) {
            const char *check = line;
            if (strip_tabs) {
                while (*check == '\t') check++;
            }

            if (strcmp(check, delimiter) == 0) {
                // Found delimiter - return collected content
                char *result = heredoc_content;
                heredoc_content = NULL;
                heredoc_content_len = 0;
                heredoc_content_cap = 0;
                return result;
            }
        }

        // Handle line continuation for unquoted heredocs
        if (!quoted && len > 0 && line[len-1] == '\\') {
            // Line ends with backslash - accumulate for continuation
            line[len-1] = '\0';  // Remove trailing backslash
            len--;

            // Grow accumulated buffer
            size_t needed = accum_len + len + 1;
            if (needed > accum_cap) {
                size_t new_cap = accum_cap ? accum_cap * 2 : 256;
                if (new_cap < needed) new_cap = needed;
                char *new_accum = realloc(accumulated, new_cap);
                if (!new_accum) {
                    free(accumulated);
                    heredoc_reset();
                    return NULL;
                }
                accumulated = new_accum;
                accum_cap = new_cap;
            }

            memcpy(accumulated + accum_len, line, len);
            accum_len += len;
            accumulated[accum_len] = '\0';
            continue;  // Read next line to continue
        }

        // Complete line (no continuation or quoted heredoc)
        const char *final_line;
        if (accumulated) {
            // Append current line to accumulated content
            size_t needed = accum_len + len + 1;
            if (needed > accum_cap) {
                char *new_accum = realloc(accumulated, needed);
                if (!new_accum) {
                    free(accumulated);
                    heredoc_reset();
                    return NULL;
                }
                accumulated = new_accum;
            }
            memcpy(accumulated + accum_len, line, len + 1);
            final_line = accumulated;
        } else {
            final_line = line;
        }

        // Add line to heredoc content
        if (heredoc_append(final_line, strip_tabs) < 0) {
            free(accumulated);
            heredoc_reset();
            return NULL;
        }

        // Reset accumulator
        free(accumulated);
        accumulated = NULL;
        accum_len = 0;
        accum_cap = 0;
    }

    // Handle any remaining accumulated content (file ended mid-continuation)
    if (accumulated) {
        if (accum_len > 0) {
            if (heredoc_append(accumulated, strip_tabs) < 0) {
                free(accumulated);
                heredoc_reset();
                return NULL;
            }
        }
        free(accumulated);
    }

    // EOF without delimiter - return what we have (POSIX allows this with warning)
    if (!script_state.silent_errors) {
        fprintf(stderr, "%s: warning: here-document delimited by end-of-file (wanted '%s')\n",
                HASH_NAME, delimiter);
    }

    char *result = heredoc_content;
    heredoc_content = NULL;
    heredoc_content_len = 0;
    heredoc_content_cap = 0;
    return result;
}

// ============================================================================
// Line Splitting (handle semicolons in compound commands)
// ============================================================================

// Split a line by semicolons, respecting quotes, parens, and braces
// Returns array of strings (must be freed by caller), NULL-terminated
// Used to handle single-line compound commands like: while cond; do body; done
static char **split_by_semicolons(const char *line, int *count) {
    if (!line || !count) return NULL;

    *count = 0;

    // First pass: count semicolons (outside quotes, parens, and braces)
    int num_parts = 1;
    bool in_single = false;
    bool in_double = false;
    int paren_depth = 0;
    int brace_depth = 0;

    for (const char *p = line; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;  // Skip escaped char
            continue;
        }
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
        } else if (!in_single && !in_double) {
            if (*p == '(' || (*p == '$' && p[1] == '(')) {
                paren_depth++;
                if (*p == '$') p++;  // Skip past $(
            } else if (*p == ')' && paren_depth > 0) {
                paren_depth--;
            } else if (*p == '{') {
                brace_depth++;
            } else if (*p == '}' && brace_depth > 0) {
                brace_depth--;
            } else if (*p == ';' && paren_depth == 0 && brace_depth == 0) {
                // Skip ;; (case clause terminator) - don't split on it
                if (p[1] == ';') {
                    p++;  // Skip the second ;
                } else {
                    num_parts++;
                }
            }
        }
    }

    // Allocate array
    char **parts = malloc((num_parts + 1) * sizeof(char *));
    if (!parts) return NULL;

    // Second pass: extract parts
    const char *start = line;
    int part_idx = 0;
    in_single = false;
    in_double = false;
    paren_depth = 0;
    brace_depth = 0;

    for (const char *p = line; ; p++) {
        if (*p == '\\' && p[1]) {
            p++;  // Skip escaped char
            continue;
        }

        bool at_end = (*p == '\0');
        bool at_semi = false;

        if (!at_end) {
            if (*p == '\'' && !in_double) {
                in_single = !in_single;
            } else if (*p == '"' && !in_single) {
                in_double = !in_double;
            } else if (!in_single && !in_double) {
                if (*p == '(' || (*p == '$' && p[1] == '(')) {
                    paren_depth++;
                    if (*p == '$') p++;
                } else if (*p == ')' && paren_depth > 0) {
                    paren_depth--;
                } else if (*p == '{') {
                    brace_depth++;
                } else if (*p == '}' && brace_depth > 0) {
                    brace_depth--;
                } else if (*p == ';' && paren_depth == 0 && brace_depth == 0) {
                    // Skip ;; (case clause terminator) - don't split on it
                    if (p[1] == ';') {
                        p++;  // Skip the second ;
                    } else {
                        at_semi = true;
                    }
                }
            }
        }

        if (at_end || at_semi) {
            size_t len = p - start;
            // Trim leading whitespace
            while (len > 0 && isspace(*start)) {
                start++;
                len--;
            }
            // Trim trailing whitespace, but not if it's escaped
            while (len > 0 && isspace(start[len-1])) {
                // Check if the whitespace is escaped (preceded by backslash)
                if (len >= 2 && start[len-2] == '\\') {
                    break;  // Don't trim escaped whitespace
                }
                len--;
            }

            if (len > 0) {
                parts[part_idx] = strndup(start, len);
                if (!parts[part_idx]) {
                    // Cleanup on error
                    for (int i = 0; i < part_idx; i++) free(parts[i]);
                    free(parts);
                    return NULL;
                }
                part_idx++;
            }

            if (at_end) break;
            start = p + 1;
        }
    }

    parts[part_idx] = NULL;
    *count = part_idx;
    return parts;
}

// Free array from split_by_semicolons
static void free_split_parts(char **parts, int count) {
    if (!parts) return;
    for (int i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);
}

// ============================================================================
// Keyword Detection
// ============================================================================

bool script_is_keyword(const char *word) {
    if (!word) return false;

    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(word, keywords[i].keyword) == 0) {
            return true;
        }
    }
    return false;
}

TokenType script_get_keyword_type(const char *word) {
    if (!word) return TOK_WORD;

    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(word, keywords[i].keyword) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_WORD;
}

// ============================================================================
// Context Stack Management
// ============================================================================

bool script_in_control_structure(void) {
    return script_state.context_depth > 0;
}

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

// Clear break/continue pending (call when handled)
void script_clear_break_continue(void) {
    break_pending = 0;
    continue_pending = 0;
}

bool script_should_execute(void) {
    if (script_state.context_depth == 0) {
        return true;
    }

    // Check if all contexts allow execution
    for (int i = 0; i < script_state.context_depth; i++) {
        if (!script_state.context_stack[i].should_execute) {
            return false;
        }
    }
    return true;
}

int script_push_context(ContextType type) {
    if (script_state.context_depth >= MAX_SCRIPT_DEPTH) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: maximum nesting depth exceeded\n", HASH_NAME);
        }
        return -1;
    }

    ScriptContext *ctx = &script_state.context_stack[script_state.context_depth];
    memset(ctx, 0, sizeof(ScriptContext));
    ctx->type = type;
    ctx->should_execute = true;
    ctx->condition_met = false;
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

    // Free context resources
    ScriptContext *ctx = &script_state.context_stack[script_state.context_depth];
    free(ctx->loop_var);
    ctx->loop_var = NULL;

    if (ctx->loop_values) {
        for (int i = 0; i < ctx->loop_count; i++) {
            free(ctx->loop_values[i]);
        }
        free(ctx->loop_values);
        ctx->loop_values = NULL;
    }

    free(ctx->loop_body);
    ctx->loop_body = NULL;
    ctx->loop_body_len = 0;
    ctx->loop_body_cap = 0;

    free(ctx->loop_condition);
    ctx->loop_condition = NULL;

    free(ctx->case_word);
    ctx->case_word = NULL;

    free(ctx->func_name);
    ctx->func_name = NULL;
    free(ctx->func_body);
    ctx->func_body = NULL;

    return 1;  // Success, continue processing
}

ContextType script_current_context(void) {
    if (script_state.context_depth == 0) {
        return CTX_NONE;
    }
    return script_state.context_stack[script_state.context_depth - 1].type;
}

static ScriptContext *get_current_context(void) {
    if (script_state.context_depth == 0) {
        return NULL;
    }
    return &script_state.context_stack[script_state.context_depth - 1];
}

// ============================================================================
// Line Classification
// ============================================================================

static char *get_first_word(const char *line, char *buf, size_t bufsize) {
    if (!line || !buf || bufsize == 0) return NULL;

    while (*line && isspace(*line)) line++;

    size_t i = 0;
    while (*line && !isspace(*line) && *line != ';' && i < bufsize - 1) {
        buf[i++] = *line++;
    }
    buf[i] = '\0';

    return buf;
}

LineType script_classify_line(const char *line) {
    if (!line) return LINE_UNKNOWN;

    while (*line && isspace(*line)) line++;

    if (*line == '\0' || *line == '#') {
        return LINE_EMPTY;
    }

    char first_word[64];
    get_first_word(line, first_word, sizeof(first_word));

    if (strcmp(first_word, "if") == 0) return LINE_IF_START;
    if (strcmp(first_word, "then") == 0) return LINE_THEN;
    if (strcmp(first_word, "elif") == 0) return LINE_ELIF;
    if (strcmp(first_word, "else") == 0) return LINE_ELSE;
    if (strcmp(first_word, "fi") == 0) return LINE_FI;
    if (strcmp(first_word, "for") == 0) return LINE_FOR_START;
    if (strcmp(first_word, "while") == 0) return LINE_WHILE_START;
    if (strcmp(first_word, "until") == 0) return LINE_UNTIL_START;
    if (strcmp(first_word, "do") == 0) return LINE_DO;
    if (strcmp(first_word, "done") == 0) return LINE_DONE;
    if (strcmp(first_word, "case") == 0) return LINE_CASE_START;
    if (strcmp(first_word, "esac") == 0) return LINE_ESAC;
    if (strcmp(first_word, "{") == 0) return LINE_LBRACE;
    if (strcmp(first_word, "}") == 0) return LINE_RBRACE;
    if (strcmp(first_word, "function") == 0) return LINE_FUNCTION_START;

    // Check for name() pattern
    const char *paren = strchr(line, '(');
    if (paren) {
        const char *close = strchr(paren, ')');
        if (close && close == paren + 1) {
            const char *p = line;
            while (p < paren && isspace(*p)) p++;
            bool valid_name = (p < paren);
            const char *name_start = p;
            while (p < paren) {
                if (!isalnum(*p) && *p != '_') {
                    valid_name = false;
                    break;
                }
                p++;
            }
            if (valid_name && p > name_start) {
                return LINE_FUNCTION_START;
            }
        }
    }

    return LINE_SIMPLE;
}

// ============================================================================
// Condition Evaluation
// ============================================================================

bool script_eval_condition(const char *condition) {
    if (!condition) return false;

    while (*condition && isspace(*condition)) condition++;
    if (*condition == '\0') return false;

    char *line_copy = strdup(condition);
    if (!line_copy) return false;

    // Set in_condition flag to suppress errexit during condition evaluation
    bool old_in_condition = script_get_in_condition();
    script_set_in_condition(true);

    CommandChain *chain = chain_parse(line_copy);
    int exit_code = 1;

    if (chain) {
        chain_execute(chain);
        exit_code = execute_get_last_exit_code();
        chain_free(chain);
    } else {
        ParseResult parsed = parse_line(line_copy);
        if (parsed.tokens && parsed.tokens[0]) {
            execute(parsed.tokens);
            exit_code = execute_get_last_exit_code();
        }
        parse_result_free(&parsed);
    }

    // Restore in_condition flag
    script_set_in_condition(old_in_condition);

    free(line_copy);
    return (exit_code == 0);
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

int script_execute_function(const ShellFunction *func, int argc, char **argv) {
    if (!func || !func->body) return 1;

    char **old_params = script_state.positional_params;
    int old_count = script_state.positional_count;
    bool old_exit_requested = script_state.exit_requested;
    bool dynamic_scoping = shell_option_nonlexicalctrl();

    // For lexical scoping (default): save and reset break/continue state
    // For dynamic scoping (nonlexicalctrl): let them propagate
    int old_break_pending = break_pending;
    int old_continue_pending = continue_pending;
    if (!dynamic_scoping) {
        break_pending = 0;
        continue_pending = 0;
    }

    script_state.positional_params = argv;
    script_state.positional_count = argc;
    script_state.exit_requested = false;  // Reset for this function

    // Increment function call depth (still useful for tracking)
    script_state.function_call_depth++;

    (void)script_execute_string(func->body);  // Result handled via last_command_exit_code

    // Decrement function call depth
    script_state.function_call_depth--;

    // Check if exit was called inside the function
    bool exit_called = script_state.exit_requested;

    script_state.positional_params = old_params;
    script_state.positional_count = old_count;

    // If exit was called inside function, propagate it
    if (exit_called) {
        if (!dynamic_scoping) {
            break_pending = old_break_pending;
            continue_pending = old_continue_pending;
        }
        script_state.exit_requested = true;
        return 0;  // Stop execution
    }

    // Dynamic scoping: if break/continue was set inside function, propagate it
    if (dynamic_scoping && (break_pending > 0 || continue_pending > 0)) {
        return 0;  // Signal to caller that break/continue is pending
    }

    // Lexical scoping: restore break/continue state (don't propagate)
    if (!dynamic_scoping) {
        break_pending = old_break_pending;
        continue_pending = old_continue_pending;
    }

    // Restore old exit_requested state (in case we're in nested functions)
    script_state.exit_requested = old_exit_requested;
    return 1;  // Continue execution
}

// ============================================================================
// Helper Functions for Control Structures
// ============================================================================

static char *extract_condition(const char *line, const char *keyword) {
    size_t keyword_len = strlen(keyword);
    const char *start = line;

    while (*start && isspace(*start)) start++;
    if (strncmp(start, keyword, keyword_len) != 0) return NULL;
    start += keyword_len;
    while (*start && isspace(*start)) start++;

    char *result = strdup(start);
    if (!result) return NULL;

    // Remove trailing "; then" or ";then" or "then" or "; do" etc.
    char *patterns[] = { "; then", ";then", "; do", ";do", NULL };
    for (int i = 0; patterns[i]; i++) {
        char *pos = strstr(result, patterns[i]);
        if (pos) {
            *pos = '\0';
            break;
        }
    }

    // Trim trailing whitespace, but not if it's escaped
    size_t len = strlen(result);
    while (len > 0 && isspace(result[len - 1])) {
        // Check if the whitespace is escaped (preceded by backslash)
        if (len >= 2 && result[len - 2] == '\\') {
            break;  // Don't trim escaped whitespace
        }
        result[--len] = '\0';
    }

    return result;
}

static int execute_simple_line(const char *line) {
    if (!line) return 0;

    // Skip leading whitespace
    while (*line && isspace(*line)) line++;
    if (!*line) return 1;

    // Check if line ends with background operator &
    // If so, let chain_parse handle it rather than processing synchronously
    size_t len = strlen(line);
    const char *end = line + len - 1;
    while (end > line && isspace(*end)) end--;
    if (*end == '&' && (end == line || *(end - 1) != '&') && (end == line || *(end - 1) != '>')) {
        // Has background operator - let chain_parse handle it
        goto use_chain_parse;
    }

    // Check for subshell syntax: (commands) [redirections]
    if (*line == '(') {
        // Find matching closing paren by tracking depth
        const char *p = line + 1;
        int depth = 1;
        int in_single_quote = 0;
        int in_double_quote = 0;

        while (*p && depth > 0) {
            if (*p == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (*p == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (!in_single_quote && !in_double_quote) {
                if (*p == '(') depth++;
                else if (*p == ')') depth--;
            }
            if (depth > 0) p++;
        }

        if (depth == 0 && *p == ')') {
            // p points to matching ')'
            const char *end_paren = p;

            // Extract the subshell content
            const char *start = line + 1;
            size_t subshell_len = end_paren - start;
            char *subshell_cmd = malloc(subshell_len + 1);
            if (!subshell_cmd) return -1;
            memcpy(subshell_cmd, start, subshell_len);
            subshell_cmd[subshell_len] = '\0';

            // Check for redirections after the closing paren
            const char *after_paren = end_paren + 1;
            while (*after_paren && isspace(*after_paren)) after_paren++;

            // Parse any external redirections (e.g., "8>pipe" after subshell)
            char *redir_str = NULL;
            if (*after_paren) {
                redir_str = strdup(after_paren);
            }

            // Flush stdout/stderr before forking to prevent child from
            // inheriting buffered content that it would then flush on exit
            fflush(stdout);
            fflush(stderr);

            // Fork a child process for the subshell
            pid_t pid = fork();
            if (pid < 0) {
                perror(HASH_NAME);
                free(subshell_cmd);
                free(redir_str);
                return 1;
            }

            if (pid == 0) {
                // Child process - apply external redirections first
                if (redir_str) {
                    // Parse and apply redirections
                    // The redir_str contains things like "8>pipe" or "2>&1"
                    char *redir_copy = strdup(redir_str);
                    if (redir_copy) {
                        // Simple redirect parsing for subshell external redirections
                        char *r = redir_copy;
                        while (*r) {
                            while (*r && isspace(*r)) r++;
                            if (!*r) break;

                            // Parse fd number
                            int fd = -1;
                            if (isdigit(*r)) {
                                fd = 0;
                                while (isdigit(*r)) {
                                    fd = fd * 10 + (*r - '0');
                                    r++;
                                }
                            }

                            if (*r == '<') {
                                r++;
                                if (fd < 0) fd = 0;
                                if (*r == '&') {
                                    r++;
                                    if (*r == '-') {
                                        close(fd);
                                        r++;
                                    } else {
                                        int src_fd = atoi(r);
                                        while (isdigit(*r)) r++;
                                        dup2(src_fd, fd);
                                    }
                                } else {
                                    // Get filename
                                    while (*r && isspace(*r)) r++;
                                    const char *filename = r;
                                    while (*r && !isspace(*r)) r++;
                                    char saved = *r;
                                    *r = '\0';
                                    int new_fd = open(filename, O_RDONLY);
                                    if (new_fd >= 0) {
                                        if (new_fd != fd) {
                                            dup2(new_fd, fd);
                                            close(new_fd);
                                        }
                                    }
                                    *r = saved;
                                }
                            } else if (*r == '>') {
                                r++;
                                if (fd < 0) fd = 1;
                                int append = 0;
                                if (*r == '>') {
                                    append = 1;
                                    r++;
                                }
                                if (*r == '&') {
                                    r++;
                                    if (*r == '-') {
                                        close(fd);
                                        r++;
                                    } else {
                                        int src_fd = atoi(r);
                                        while (isdigit(*r)) r++;
                                        dup2(src_fd, fd);
                                    }
                                } else {
                                    // Get filename
                                    while (*r && isspace(*r)) r++;
                                    const char *filename = r;
                                    while (*r && !isspace(*r)) r++;
                                    char saved = *r;
                                    *r = '\0';
                                    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                                    int new_fd = open(filename, flags, 0644);
                                    if (new_fd >= 0) {
                                        if (new_fd != fd) {
                                            dup2(new_fd, fd);
                                            close(new_fd);
                                        }
                                    }
                                    *r = saved;
                                }
                            } else {
                                // Unknown, skip this char
                                r++;
                            }
                        }
                        free(redir_copy);
                    }
                    free(redir_str);
                }

                // Execute the subshell commands
                // POSIX: Traps are not inherited by subshells - reset them
                trap_reset_for_subshell();
                // Note: don't close stdin for subshells as they run synchronously
                int exit_code = script_execute_string(subshell_cmd);
                free(subshell_cmd);
                // Execute EXIT trap before exiting subshell (only if set in this subshell)
                trap_execute_exit();
                // Flush output before exit to ensure all output is visible
                fflush(stdout);
                fflush(stderr);
                // script_execute_string returns the exit code directly
                _exit(exit_code);
            }

            // Parent process - wait for child
            free(subshell_cmd);
            free(redir_str);
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                last_command_exit_code = WEXITSTATUS(status);
            } else {
                last_command_exit_code = 1;
            }
            return 1;
        }
    }

    // Check for brace group syntax: { commands; } [&]
    // Note: { must be followed by space, and } must be preceded by ; or newline
    if (*line == '{' && (line[1] == '\0' || isspace(line[1]))) {
        // Find matching closing brace
        const char *p = line + 1;
        while (*p && isspace(*p)) p++;

        // Find the closing } (might have & after it)
        const char *brace_end = line + strlen(line) - 1;
        while (brace_end > p && isspace(*brace_end)) brace_end--;

        // Check for background operator
        bool background = false;
        if (*brace_end == '&') {
            background = true;
            brace_end--;
            while (brace_end > p && isspace(*brace_end)) brace_end--;
        }

        if (*brace_end == '}') {
            // Extract the brace group content
            size_t group_len = brace_end - p;
            char *group_cmd = malloc(group_len + 1);
            if (!group_cmd) return -1;
            memcpy(group_cmd, p, group_len);
            group_cmd[group_len] = '\0';

            if (background) {
                // Flush stdout/stderr before forking to prevent child from
                // inheriting buffered content that it would then flush on exit
                fflush(stdout);
                fflush(stderr);

                // Fork to run in background
                pid_t pid = fork();
                if (pid < 0) {
                    perror(HASH_NAME);
                    free(group_cmd);
                    return 1;
                }
                if (pid == 0) {
                    // Child process - close stdin to avoid interference with parent
                    close(STDIN_FILENO);
                    script_execute_string(group_cmd);
                    free(group_cmd);
                    fflush(stdout);
                    fflush(stderr);
                    _exit(last_command_exit_code);
                }
                // Parent - set last background PID and add to job table
                free(group_cmd);
                jobs_set_last_bg_pid(pid);
                // Add job to job table (needed for jobs command and $!)
                int job_id = jobs_add(pid, line);
                // Only print job notification in interactive mode with job control
                if (job_id > 0 && isatty(STDIN_FILENO) && shell_option_monitor()) {
                    printf("[%d] %d\n", job_id, pid);
                }
                last_command_exit_code = 0;
                return 1;
            } else {
                // Execute the commands in the current shell (not a subshell)
                int result = script_execute_string(group_cmd);
                free(group_cmd);
                return result == 0 ? 1 : result;
            }
        }
    }

use_chain_parse:;
    char *line_copy = strdup(line);
    if (!line_copy) return -1;

    CommandChain *chain = chain_parse(line_copy);
    int result = 0;

    if (chain) {
        result = chain_execute(chain);
        chain_free(chain);
    }

    free(line_copy);
    return result;
}

// ============================================================================
// Control Structure Processing
// ============================================================================

static int process_if(const char *line) {
    if (script_push_context(CTX_IF) < 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    // Check parent context
    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing) {
        char *condition = extract_condition(line, "if");
        if (condition) {
            bool result = script_eval_condition(condition);
            ctx->condition_met = result;
            ctx->should_execute = result;
            free(condition);
        } else {
            ctx->should_execute = false;
        }
    } else {
        ctx->should_execute = false;
    }

    return 1;  // Continue processing
}

static int process_then(const char *line) {
    ContextType ctx_type = script_current_context();
    if (ctx_type != CTX_IF && ctx_type != CTX_ELIF) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'then'\n", HASH_NAME);
        }
        return -1;
    }

    // Check if there's a command after 'then'
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "then", 4) == 0) {
        p += 4;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            // There's a command after 'then', execute it
            if (script_should_execute()) {
                return execute_simple_line(p);
            }
        }
    }
    return 1;  // Continue processing
}

static int process_elif(const char *line) {
    ScriptContext *ctx = get_current_context();
    if (!ctx || (ctx->type != CTX_IF && ctx->type != CTX_ELIF && ctx->type != CTX_ELSE)) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'elif'\n", HASH_NAME);
        }
        return -1;
    }

    ctx->type = CTX_ELIF;

    // Check parent context
    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing && !ctx->condition_met) {
        char *condition = extract_condition(line, "elif");
        if (condition) {
            bool result = script_eval_condition(condition);
            if (result) {
                ctx->condition_met = true;
                ctx->should_execute = true;
            } else {
                ctx->should_execute = false;
            }
            free(condition);
        }
    } else {
        ctx->should_execute = false;
    }

    return 1;  // Continue processing
}

static int process_else(const char *line) {
    ScriptContext *ctx = get_current_context();
    if (!ctx || (ctx->type != CTX_IF && ctx->type != CTX_ELIF)) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'else'\n", HASH_NAME);
        }
        return -1;
    }

    ctx->type = CTX_ELSE;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    ctx->should_execute = parent_executing && !ctx->condition_met;

    // Check if there's a command after 'else'
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "else", 4) == 0) {
        p += 4;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            // There's a command after 'else', execute it
            if (script_should_execute()) {
                return execute_simple_line(p);
            }
        }
    }

    return 1;  // Continue processing
}

static int process_fi(const char *line) {
    (void)line;

    ContextType ctx_type = script_current_context();
    if (ctx_type != CTX_IF && ctx_type != CTX_ELIF && ctx_type != CTX_ELSE) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'fi'\n", HASH_NAME);
        }
        return -1;
    }

    return script_pop_context();
}

// Extract function name from "name() {" or "function name {"
static char *extract_function_name(const char *line) {
    const char *p = line;
    while (*p && isspace(*p)) p++;

    // Check for "function name" syntax
    if (strncmp(p, "function", 8) == 0 && isspace(p[8])) {
        p += 8;
        while (*p && isspace(*p)) p++;
    }

    // Now p points to the function name
    const char *name_start = p;
    while (*p && (isalnum(*p) || *p == '_')) p++;

    if (p == name_start) return NULL;

    size_t name_len = p - name_start;
    char *name = malloc(name_len + 1);
    if (!name) return NULL;
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    return name;
}

// Append a line to the function body buffer
static int append_to_func_body(ScriptContext *ctx, const char *line) {
    size_t line_len = strlen(line);
    size_t needed = ctx->func_body_len + line_len + 2; // +1 for newline, +1 for null

    if (needed > ctx->func_body_cap) {
        size_t new_cap = ctx->func_body_cap ? ctx->func_body_cap * 2 : 1024;
        if (new_cap < needed) new_cap = needed;
        if (new_cap > MAX_FUNC_BODY) new_cap = MAX_FUNC_BODY;
        if (needed > MAX_FUNC_BODY) {
            fprintf(stderr, "%s: function body too large\n", HASH_NAME);
            return -1;
        }

        char *new_body = realloc(ctx->func_body, new_cap);
        if (!new_body) return -1;
        ctx->func_body = new_body;
        ctx->func_body_cap = new_cap;
    }

    if (!ctx->func_body) return -1;

    if (ctx->func_body_len > 0) {
        ctx->func_body[ctx->func_body_len++] = '\n';
    }
    memcpy(ctx->func_body + ctx->func_body_len, line, line_len);
    ctx->func_body_len += line_len;
    ctx->func_body[ctx->func_body_len] = '\0';

    return 0;
}

// Append a line to the loop body buffer
static int append_to_loop_body(ScriptContext *ctx, const char *line) {
    size_t line_len = strlen(line);
    size_t needed = ctx->loop_body_len + line_len + 2; // +1 for newline, +1 for null

    if (needed > ctx->loop_body_cap) {
        size_t new_cap = ctx->loop_body_cap ? ctx->loop_body_cap * 2 : 1024;
        if (new_cap < needed) new_cap = needed;
        if (new_cap > MAX_FUNC_BODY) new_cap = MAX_FUNC_BODY;  // Use same limit
        if (needed > MAX_FUNC_BODY) {
            fprintf(stderr, "%s: loop body too large\n", HASH_NAME);
            return -1;
        }

        char *new_body = realloc(ctx->loop_body, new_cap);
        if (!new_body) return -1;
        ctx->loop_body = new_body;
        ctx->loop_body_cap = new_cap;
    }

    if (!ctx->loop_body) return -1;

    if (ctx->loop_body_len > 0) {
        ctx->loop_body[ctx->loop_body_len++] = '\n';
    }
    memcpy(ctx->loop_body + ctx->loop_body_len, line, line_len);
    ctx->loop_body_len += line_len;
    ctx->loop_body[ctx->loop_body_len] = '\0';

    return 0;
}

// Count brace depth change in a line
static int count_braces(const char *line) {
    int delta = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (const char *p = line; *p; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip escaped character
            continue;
        }
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote) {
            if (*p == '{') delta++;
            else if (*p == '}') delta--;
        }
    }
    return delta;
}

static int process_function(const char *line) {
    char *name = extract_function_name(line);
    if (!name) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: invalid function definition\n", HASH_NAME);
        }
        return -1;
    }

    if (script_push_context(CTX_FUNCTION) < 0) {
        free(name);
        return -1;
    }

    ScriptContext *ctx = get_current_context();
    if (!ctx) {
        free(name);
        return -1;
    }

    ctx->func_name = name;
    ctx->func_body = NULL;
    ctx->func_body_len = 0;
    ctx->func_body_cap = 0;
    ctx->should_execute = false;  // Don't execute lines inside function definition

    // Check if line contains opening brace or paren (for subshell function bodies)
    const char *brace = strchr(line, '{');
    const char *paren = strchr(line, '(');

    // Skip the () after function name to find actual body start
    // Format: funcname() ( ... ) or funcname() { ... }
    if (paren) {
        // Skip past "funcname()" to find the body opener
        const char *p = paren + 1;
        while (*p && isspace(*p)) p++;
        if (*p == ')') {
            p++;  // Skip the )
            while (*p && isspace(*p)) p++;
            if (*p == '(') {
                paren = p;  // This is the body-opening paren
            } else if (*p == '{') {
                paren = NULL;  // Use brace instead
                brace = p;
            } else {
                paren = NULL;  // No valid body start
            }
        } else {
            paren = NULL;  // Not funcname()
        }
    }

    // Handle subshell function body: f() ( ... )
    if (paren && (!brace || paren < brace)) {
        // Find matching closing paren
        const char *after_paren = paren + 1;
        int depth = 1;
        const char *p = after_paren;
        const char *body_end = NULL;
        bool in_sq = false, in_dq = false;

        while (*p && depth > 0) {
            if (*p == '\\' && *(p + 1)) {
                p += 2;
                continue;
            }
            if (*p == '\'' && !in_dq) in_sq = !in_sq;
            else if (*p == '"' && !in_sq) in_dq = !in_dq;
            else if (!in_sq && !in_dq) {
                if (*p == '(') depth++;
                else if (*p == ')') {
                    depth--;
                    if (depth == 0) {
                        body_end = p;
                    }
                }
            }
            p++;
        }

        if (body_end) {
            // Complete subshell function body on this line
            // Store the body WITH parentheses so it runs as subshell
            size_t body_len = body_end - paren + 1;  // Include both parens
            char *body = malloc(body_len + 1);
            if (body) {
                memcpy(body, paren, body_len);
                body[body_len] = '\0';
                script_define_function(ctx->func_name, body);
                free(body);
            }
            script_pop_context();

            // Execute any commands after the function definition
            const char *after_func = body_end + 1;
            while (*after_func && isspace(*after_func)) after_func++;
            if (*after_func == ';') after_func++;
            while (*after_func && isspace(*after_func)) after_func++;
            if (*after_func && *after_func != '#') {
                return execute_simple_line(after_func);
            }
            return 1;
        }
        // Multi-line subshell function body not supported yet
        ctx->brace_depth = 0;
        return 1;
    }

    if (brace) {
        ctx->brace_depth = 1;
        // Check for content after the brace
        const char *after_brace = brace + 1;
        while (*after_brace && isspace(*after_brace)) after_brace++;
        if (*after_brace && *after_brace != '#') {
            // There's content after {
            // Find the closing } for the function body
            int depth = 1;
            const char *p = after_brace;
            const char *body_end = NULL;
            bool in_sq = false, in_dq = false;

            while (*p && depth > 0) {
                if (*p == '\\' && *(p + 1)) {
                    p += 2;
                    continue;
                }
                if (*p == '\'' && !in_dq) in_sq = !in_sq;
                else if (*p == '"' && !in_sq) in_dq = !in_dq;
                else if (!in_sq && !in_dq) {
                    if (*p == '{') depth++;
                    else if (*p == '}') {
                        depth--;
                        if (depth == 0) {
                            body_end = p;
                        }
                    }
                }
                p++;
            }

            if (body_end) {
                // Function body ends on this line
                size_t body_len = body_end - after_brace;
                char *body = malloc(body_len + 1);
                if (body) {
                    memcpy(body, after_brace, body_len);
                    body[body_len] = '\0';
                    // Trim trailing whitespace
                    while (body_len > 0 && isspace(body[body_len - 1])) {
                        body[--body_len] = '\0';
                    }
                    script_define_function(ctx->func_name, body);
                    free(body);
                }
                script_pop_context();

                // Execute any commands after the function definition
                const char *after_func = body_end + 1;
                while (*after_func && isspace(*after_func)) after_func++;
                if (*after_func == ';') after_func++;
                while (*after_func && isspace(*after_func)) after_func++;
                if (*after_func && *after_func != '#') {
                    return execute_simple_line(after_func);
                }
                return 1;  // Continue processing
            } else {
                // Body continues on next line
                append_to_func_body(ctx, after_brace);
                ctx->brace_depth += count_braces(after_brace);
            }
        }
    } else {
        ctx->brace_depth = 0;  // Waiting for opening brace
    }

    return 1;  // Continue processing
}

static int process_lbrace(const char *line) {
    ScriptContext *ctx = get_current_context();

    // If we're in a function context waiting for opening brace
    if (ctx && ctx->type == CTX_FUNCTION && ctx->brace_depth == 0) {
        ctx->brace_depth = 1;
        // Check for content after the brace
        const char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p == '{') p++;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            append_to_func_body(ctx, p);
            ctx->brace_depth += count_braces(p);
        }
        return 1;  // Continue processing
    }

    // Otherwise treat as simple command
    if (script_should_execute()) {
        return execute_simple_line(line);
    }
    return 1;  // Continue processing
}

static int process_rbrace(const char *line) {
    (void)line;

    ScriptContext *ctx = get_current_context();
    if (ctx && ctx->type == CTX_FUNCTION) {
        ctx->brace_depth--;
        if (ctx->brace_depth <= 0) {
            // Function definition complete
            script_define_function(ctx->func_name, ctx->func_body ? ctx->func_body : "");
            return script_pop_context();
        }
        // Still inside nested braces, add line to body
        append_to_func_body(ctx, line);
        return 1;  // Continue processing
    }

    // Otherwise treat as simple command
    if (script_should_execute()) {
        return execute_simple_line(line);
    }
    return 1;  // Continue processing
}

static int process_for(const char *line) {
    if (script_push_context(CTX_FOR) < 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (!parent_executing) {
        ctx->should_execute = false;
        return 0;
    }

    // Parse: for var in word1 word2 ...
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "for", 3) != 0) return -1;
    p += 3;
    while (*p && isspace(*p)) p++;

    // Get variable name
    char varname[256];
    size_t vi = 0;
    while (*p && (isalnum(*p) || *p == '_') && vi < sizeof(varname) - 1) {
        varname[vi++] = *p++;
    }
    varname[vi] = '\0';

    if (vi == 0) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: expected variable name after 'for'\n", HASH_NAME);
        }
        return -1;
    }

    ctx->loop_var = strdup(varname);

    while (*p && isspace(*p)) p++;

    // Check for "in"
    if (strncmp(p, "in", 2) == 0 && (isspace(p[2]) || p[2] == '\0' || p[2] == ';')) {
        p += 2;
        while (*p && isspace(*p)) p++;

        char *values_str = strdup(p);
        if (values_str) {
            // Remove trailing "; do" or ";do"
            char *semi = strstr(values_str, "; do");
            if (semi) *semi = '\0';
            semi = strstr(values_str, ";do");
            if (semi) *semi = '\0';

            size_t len = strlen(values_str);
            while (len > 0 && isspace(values_str[len-1])) {
                values_str[--len] = '\0';
            }

            // Parse values using parse_line for proper quote handling
            char **values = NULL;
            int count = 0;

            if (len > 0) {
                ParseResult parsed = parse_line(values_str);
                if (parsed.tokens) {
                    // Count parsed values
                    int parsed_count = 0;
                    while (parsed.tokens[parsed_count]) parsed_count++;

                    if (parsed_count > 0) {
                        values = malloc((size_t)(parsed_count + 1) * sizeof(char*));
                        if (values) {
                            for (int i = 0; i < parsed_count && count < 255; i++) {
                                values[count] = strdup(parsed.tokens[i]);
                                // Strip quote markers so loop variable gets actual value
                                strip_quote_markers(values[count]);
                                count++;
                            }
                            values[count] = NULL;
                        }
                    }
                    parse_result_free(&parsed);
                }
            }

            ctx->loop_values = values;
            ctx->loop_count = count;
            ctx->loop_index = 0;

            free(values_str);
        }
    }

    ctx->should_execute = (ctx->loop_count > 0);

    if (ctx->should_execute && ctx->loop_var && ctx->loop_values && ctx->loop_count > 0) {
        // Check for readonly before setting loop variable
        if (shellvar_is_readonly(ctx->loop_var)) {
            fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, ctx->loop_var);
            last_command_exit_code = 1;
            script_pop_context();
            return is_interactive ? 1 : 0;  // Exit in non-interactive mode
        }
        shellvar_set(ctx->loop_var, ctx->loop_values[0]);
        setenv(ctx->loop_var, ctx->loop_values[0], 1);
    }

    return 1;  // Continue processing
}

static int process_while(const char *line) {
    if (script_push_context(CTX_WHILE) < 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing) {
        char *condition = extract_condition(line, "while");
        if (condition) {
            // Store the condition for re-evaluation
            ctx->loop_condition = condition;
            // Don't evaluate yet - we'll evaluate in process_done
            ctx->should_execute = true;  // Allow body collection
        } else {
            ctx->should_execute = false;
        }
    } else {
        ctx->should_execute = false;
    }

    return 1;  // Continue processing
}

static int process_until(const char *line) {
    if (script_push_context(CTX_UNTIL) < 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing) {
        char *condition = extract_condition(line, "until");
        if (condition) {
            // Store the condition for re-evaluation
            ctx->loop_condition = condition;
            // Don't evaluate yet - we'll evaluate in process_done
            ctx->should_execute = true;  // Allow body collection
        } else {
            ctx->should_execute = false;
        }
    } else {
        ctx->should_execute = false;
    }

    return 1;  // Continue processing
}

static int process_do(const char *line) {
    ScriptContext *ctx = get_current_context();
    ContextType ctx_type = script_current_context();
    if (!ctx || (ctx_type != CTX_FOR && ctx_type != CTX_WHILE && ctx_type != CTX_UNTIL)) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'do'\n", HASH_NAME);
        }
        return -1;
    }

    // Start collecting the loop body
    ctx->collecting_body = true;
    ctx->body_nesting_depth = 0;  // Track nested loops during collection
    ctx->loop_body = NULL;
    ctx->loop_body_len = 0;
    ctx->loop_body_cap = 0;

    // Check if there's a command after 'do'
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "do", 2) == 0) {
        p += 2;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            // There's a command after 'do', add it to the body
            append_to_loop_body(ctx, p);
        }
    }
    return 1;  // Continue processing
}

// Execute a buffered loop body (multi-line string)
// Collect heredoc content from a string buffer, advancing the pointer past the heredoc
// For unquoted heredocs (quoted=0), backslash-newline is treated as line continuation
// Returns allocated heredoc content string (caller must free)
static char *heredoc_collect_from_string(const char **ptr, const char *delimiter, int strip_tabs, int quoted) {
    heredoc_reset();

    const char *p = *ptr;
    char *accumulated = NULL;  // For handling line continuations
    size_t accum_len = 0;
    size_t accum_cap = 0;

    while (*p) {
        // Find end of current line
        const char *line_start = p;
        while (*p && *p != '\n') p++;

        // Extract line (without newline)
        size_t line_len = p - line_start;
        char *line = malloc(line_len + 1);
        if (!line) {
            free(accumulated);
            heredoc_reset();
            return NULL;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        // Skip past newline if present
        if (*p == '\n') p++;

        // Check for delimiter (only on raw lines, not accumulated)
        if (!accumulated) {
            const char *check = line;
            if (strip_tabs) {
                while (*check == '\t') check++;
            }

            if (strcmp(check, delimiter) == 0) {
                // Found delimiter - update pointer and return collected content
                free(line);
                *ptr = p;
                char *result = heredoc_content;
                heredoc_content = NULL;
                heredoc_content_len = 0;
                heredoc_content_cap = 0;
                return result;
            }
        }

        // Handle line continuation for unquoted heredocs
        if (!quoted && line_len > 0 && line[line_len-1] == '\\') {
            // Line ends with backslash - accumulate for continuation
            line[line_len-1] = '\0';  // Remove trailing backslash
            line_len--;

            // Grow accumulated buffer
            size_t needed = accum_len + line_len + 1;
            if (needed > accum_cap) {
                size_t new_cap = accum_cap ? accum_cap * 2 : 256;
                if (new_cap < needed) new_cap = needed;
                char *new_accum = realloc(accumulated, new_cap);
                if (!new_accum) {
                    free(accumulated);
                    free(line);
                    heredoc_reset();
                    return NULL;
                }
                accumulated = new_accum;
                accum_cap = new_cap;
            }

            memcpy(accumulated + accum_len, line, line_len);
            accum_len += line_len;
            accumulated[accum_len] = '\0';
            free(line);
            continue;  // Read next line to continue
        }

        // Complete line (no continuation or quoted heredoc)
        const char *final_line;
        if (accumulated) {
            // Append current line to accumulated content
            size_t needed = accum_len + line_len + 1;
            if (needed > accum_cap) {
                char *new_accum = realloc(accumulated, needed);
                if (!new_accum) {
                    free(accumulated);
                    free(line);
                    heredoc_reset();
                    return NULL;
                }
                accumulated = new_accum;
            }
            memcpy(accumulated + accum_len, line, line_len + 1);
            final_line = accumulated;
        } else {
            final_line = line;
        }

        // Add line to heredoc content
        if (heredoc_append(final_line, strip_tabs) < 0) {
            free(accumulated);
            free(line);
            heredoc_reset();
            return NULL;
        }

        // Cleanup
        free(line);
        free(accumulated);
        accumulated = NULL;
        accum_len = 0;
        accum_cap = 0;
    }

    // Handle any remaining accumulated content (string ended mid-continuation)
    if (accumulated) {
        if (accum_len > 0) {
            if (heredoc_append(accumulated, strip_tabs) < 0) {
                free(accumulated);
                heredoc_reset();
                return NULL;
            }
        }
        free(accumulated);
    }

    // End of string without delimiter - return what we have
    *ptr = p;
    char *result = heredoc_content;
    heredoc_content = NULL;
    heredoc_content_len = 0;
    heredoc_content_cap = 0;
    return result;
}

static int execute_loop_body(const char *body) {
    if (!body || *body == '\0') return 1;

    const char *ptr = body;
    int result = 1;

    while (*ptr && result > 0) {
        // Find end of current line
        const char *line_start = ptr;
        while (*ptr && *ptr != '\n') ptr++;

        // Extract line
        size_t line_len = ptr - line_start;
        char *line = malloc(line_len + 1);
        if (!line) return -1;
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        // Skip past newline if present
        if (*ptr == '\n') ptr++;

        // Skip empty lines and comments
        const char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            // Check for heredoc and collect content if present
            if (redirect_has_heredoc(line)) {
                int strip_tabs = 0;
                int quoted = 0;
                char *delim = redirect_get_heredoc_delim(line, &strip_tabs, &quoted);
                if (delim) {
                    free(pending_heredoc);
                    pending_heredoc = heredoc_collect_from_string(&ptr, delim, strip_tabs, quoted);
                    pending_heredoc_quoted = quoted;
                    free(delim);
                }
            }

            // Use script_process_line to handle nested control structures
            result = script_process_line(line);

            // Clear pending heredoc after processing
            free(pending_heredoc);
            pending_heredoc = NULL;
            pending_heredoc_quoted = 0;
        }

        free(line);

        // Check for pending break/continue (POSIX dynamic scoping)
        if (break_pending > 0 || continue_pending > 0) {
            break;  // Stop executing body lines
        }
    }

    // Return signal if break/continue is pending
    if (break_pending > 0) return -3;
    if (continue_pending > 0) return -4;

    return result;
}

static int process_done(const char *line) {
    (void)line;

    ScriptContext *ctx = get_current_context();
    ContextType ctx_type = script_current_context();

    if (!ctx || (ctx_type != CTX_FOR && ctx_type != CTX_WHILE && ctx_type != CTX_UNTIL)) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'done'\n", HASH_NAME);
        }
        return -1;
    }

    // Stop collecting the loop body
    ctx->collecting_body = false;

    // Check if parent context allows execution
    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (!parent_executing) {
        return script_pop_context();
    }

    // POSIX: Exit status of loop is exit status of last body command, or 0 if none executed
    extern int last_command_exit_code;
    int body_exit_code = 0;  // Default if body never executes
    bool body_executed = false;
    bool should_propagate_break = false;
    bool should_propagate_continue = false;

    if (ctx_type == CTX_FOR) {
        // Execute the loop body for each value
        while (ctx->loop_index < ctx->loop_count) {
            if (ctx->loop_var && ctx->loop_values) {
                // Check for readonly before setting loop variable
                if (shellvar_is_readonly(ctx->loop_var)) {
                    fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, ctx->loop_var);
                    last_command_exit_code = 1;
                    script_pop_context();
                    return is_interactive ? 1 : 0;  // Exit in non-interactive mode
                }
                shellvar_set(ctx->loop_var, ctx->loop_values[ctx->loop_index]);
                setenv(ctx->loop_var, ctx->loop_values[ctx->loop_index], 1);
            }
            ctx->should_execute = true;
            int result = execute_loop_body(ctx->loop_body);
            body_executed = true;
            body_exit_code = last_command_exit_code;

            if (result == 0) {
                // Exit was called
                script_pop_context();
                return 0;
            }

            // Handle break with level decrementing
            if (break_pending > 0) {
                break_pending--;
                if (break_pending > 0) {
                    should_propagate_break = true;
                }
                break;  // Exit this loop
            }

            // Handle continue with level decrementing
            if (continue_pending > 0) {
                continue_pending--;
                if (continue_pending > 0) {
                    should_propagate_continue = true;
                    break;  // Propagate to outer loop
                }
                // continue_pending is now 0, continue this loop
                ctx->loop_index++;
                continue;
            }

            if (result < 0 && result != -3 && result != -4) {
                // Other error
                break;
            }

            ctx->loop_index++;
        }
    } else if (ctx_type == CTX_WHILE) {
        // Execute while condition is true
        while (ctx->loop_condition && script_eval_condition(ctx->loop_condition)) {
            ctx->should_execute = true;
            int result = execute_loop_body(ctx->loop_body);
            body_executed = true;
            body_exit_code = last_command_exit_code;

            if (result == 0) {
                // Exit was called
                script_pop_context();
                return 0;
            }

            // Handle break with level decrementing
            if (break_pending > 0) {
                break_pending--;
                if (break_pending > 0) {
                    should_propagate_break = true;
                }
                break;  // Exit this loop
            }

            // Handle continue with level decrementing
            if (continue_pending > 0) {
                continue_pending--;
                if (continue_pending > 0) {
                    should_propagate_continue = true;
                    break;  // Propagate to outer loop
                }
                // continue_pending is now 0, continue this loop
                continue;
            }

            if (result < 0 && result != -3 && result != -4) {
                // Other error
                break;
            }
        }
    } else if (ctx_type == CTX_UNTIL) {
        // Execute until condition is true (while condition is false)
        while (ctx->loop_condition && !script_eval_condition(ctx->loop_condition)) {
            ctx->should_execute = true;
            int result = execute_loop_body(ctx->loop_body);
            body_executed = true;
            body_exit_code = last_command_exit_code;

            if (result == 0) {
                // Exit was called
                script_pop_context();
                return 0;
            }

            // Handle break with level decrementing
            if (break_pending > 0) {
                break_pending--;
                if (break_pending > 0) {
                    should_propagate_break = true;
                }
                break;  // Exit this loop
            }

            // Handle continue with level decrementing
            if (continue_pending > 0) {
                continue_pending--;
                if (continue_pending > 0) {
                    should_propagate_continue = true;
                    break;  // Propagate to outer loop
                }
                // continue_pending is now 0, continue this loop
                continue;
            }

            if (result < 0 && result != -3 && result != -4) {
                // Other error
                break;
            }
        }
    }

    // Restore exit code to last body execution (or 0 if body never executed)
    // This ensures condition check doesn't override the loop's exit status
    if (body_executed) {
        last_command_exit_code = body_exit_code;
    } else {
        last_command_exit_code = 0;
    }

    script_pop_context();

    // Propagate break/continue to outer loops if levels remain
    if (should_propagate_break) {
        return -3;
    }
    if (should_propagate_continue) {
        return -4;
    }

    return 1;
}

// ============================================================================
// Case Statement Processing
// ============================================================================

// Forward declarations for case pattern matching
static char *remove_shell_quotes(const char *str);
static char *expand_case_word(const char *word);
static char *expand_case_pattern(const char *pattern);

// Append line to case body buffer (reuses loop_body fields)
static int append_to_case_body(ScriptContext *ctx, const char *line) {
    return append_to_loop_body(ctx, line);  // Same implementation
}

// Find the next logical line boundary (newline not inside quotes)
// Returns pointer to the newline, or NULL if not found
static char *find_logical_line_end(char *str) {
    if (!str) return NULL;

    bool in_single = false;
    bool in_double = false;

    for (char *p = str; *p; p++) {
        if (*p == '\\' && !in_single && *(p + 1)) {
            p++;  // Skip escaped character
            continue;
        }
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
        } else if (*p == '\n' && !in_single && !in_double) {
            return p;
        }
    }
    return NULL;
}

// Find closing ) that's not inside quotes
// Returns pointer to the ), or NULL if not found
static char *find_unquoted_close_paren(char *str) {
    if (!str) return NULL;

    bool in_single = false;
    bool in_double = false;

    for (char *p = str; *p; p++) {
        if (*p == '\\' && !in_single && *(p + 1)) {
            p++;  // Skip escaped character
            continue;
        }
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
        } else if (*p == ')' && !in_single && !in_double) {
            return p;
        }
    }
    return NULL;
}

// Check if a pattern matches a word using fnmatch
// Handles POSIX shell pattern matching with *, ?, [...]
static bool case_pattern_matches(const char *pattern, const char *word) {
    if (!pattern || !word) return false;
    // fnmatch returns 0 on match
    return fnmatch(pattern, word, 0) == 0;
}

// Parse and execute the case body
// Returns exit code of last executed command, or 0 if no match
static int execute_case_body(const char *body, const char *word) {
    if (!body || !word) return 0;

    extern int last_command_exit_code;
    int result_exit_code = 0;  // Default exit code if no match
    bool matched = false;
    bool in_matched_clause = false;

    char *body_copy = strdup(body);
    if (!body_copy) return 1;

    char *line = body_copy;
    char *next_line;

    while (line && *line) {
        // Find end of current logical line (respecting quotes)
        next_line = find_logical_line_end(line);
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        // Skip leading whitespace
        while (*line && isspace(*line)) line++;

        // Skip empty lines and comments
        if (!*line || *line == '#') {
            line = next_line;
            continue;
        }

        // Check if this line is a pattern (ends with ) and has pattern before it)
        // Pattern format: pattern) or (pattern) or pattern|pattern)
        // Skip if we're in a matched clause executing commands

        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;

        // Check for ;; which ends a clause
        if (strncmp(trimmed, ";;", 2) == 0) {
            if (in_matched_clause) {
                // End of matched clause
                in_matched_clause = false;
            }
            line = next_line;
            continue;
        }

        // Check if this is a pattern line
        // Patterns can be: pattern) or (pattern) or pat1|pat2)
        // A pattern line ends with ) but not ;;
        char *close_paren = NULL;

        // Skip commands that might contain subshells
        // A pattern line is: [whitespace][(][pattern][|pattern...][)]
        // We need to detect pattern lines vs command lines

        // If we're in a matched clause, this should be a command, not a pattern
        // Use script_process_line to handle nested control structures (case, if, for, etc.)
        if (in_matched_clause) {
            // Execute this command using script_process_line to handle nested structures
            int cmd_result = script_process_line(line);
            result_exit_code = last_command_exit_code;
            if (cmd_result == 0) {
                // Exit was called
                free(body_copy);
                return result_exit_code;
            }
            line = next_line;
            continue;
        }

        // Not in a matched clause - this should be a pattern
        // Look for pattern ending with )
        // Skip leading ( if present
        char *p = trimmed;
        if (*p == '(') {
            p++;
            while (*p && isspace(*p)) p++;
        }

        // Find the closing ) for this pattern (respecting quotes)
        // Need to handle patterns with | separators
        close_paren = find_unquoted_close_paren(p);
        if (!close_paren) {
            // Not a valid pattern line, treat as command (shouldn't happen in valid case)
            line = next_line;
            continue;
        }

        // Extract the pattern(s)
        size_t pattern_len = close_paren - p;
        char *patterns = malloc(pattern_len + 1);
        if (!patterns) {
            line = next_line;
            continue;
        }
        memcpy(patterns, p, pattern_len);
        patterns[pattern_len] = '\0';

        // Trim trailing whitespace from pattern
        size_t plen = strlen(patterns);
        while (plen > 0 && isspace(patterns[plen - 1])) {
            patterns[--plen] = '\0';
        }

        // Check if any pattern in the list matches (patterns separated by |)
        bool this_matches = false;
        if (!matched) {  // Only check if we haven't matched yet
            char *pat_copy = strdup(patterns);
            if (pat_copy) {
                char *saveptr;
                char *single_pat = strtok_r(pat_copy, "|", &saveptr);
                while (single_pat) {
                    // Trim whitespace from individual pattern
                    while (*single_pat && isspace(*single_pat)) single_pat++;
                    char *end = single_pat + strlen(single_pat) - 1;
                    while (end > single_pat && isspace(*end)) *end-- = '\0';

                    // Expand pattern (variable expansion, then quote removal)
                    char *expanded_pat = expand_case_pattern(single_pat);
                    if (expanded_pat) {
                        if (case_pattern_matches(expanded_pat, word)) {
                            this_matches = true;
                            free(expanded_pat);
                            break;
                        }
                        free(expanded_pat);
                    }
                    single_pat = strtok_r(NULL, "|", &saveptr);
                }
                free(pat_copy);
            }
        }

        free(patterns);

        if (this_matches) {
            matched = true;
            in_matched_clause = true;

            // Check if there are commands after the ) on this line
            char *after_paren = close_paren + 1;
            while (*after_paren && isspace(*after_paren)) after_paren++;

            // Check for ;; immediately after pattern
            if (strncmp(after_paren, ";;", 2) == 0) {
                // Empty clause, matched but no commands
                in_matched_clause = false;
            } else if (*after_paren && *after_paren != '#') {
                // There's a command on the same line as the pattern
                // Need to handle commands that might end with ;;
                // But we must find the RIGHT ;; - not one inside a nested case
                char *double_semi = NULL;
                int nested_depth = 0;
                for (char *s = after_paren; *s; s++) {
                    // Track nested case statements
                    if (strncmp(s, "case", 4) == 0 &&
                        (s == after_paren || isspace(*(s-1)) || *(s-1) == ';') &&
                        (isspace(s[4]) || s[4] == '\0')) {
                        nested_depth++;
                        s += 3;  // Skip past 'case' (loop will add 1 more)
                        continue;
                    }
                    if (strncmp(s, "esac", 4) == 0 &&
                        (s == after_paren || isspace(*(s-1)) || *(s-1) == ';') &&
                        (s[4] == '\0' || isspace(s[4]) || s[4] == ';')) {
                        if (nested_depth > 0) nested_depth--;
                        s += 3;  // Skip past 'esac'
                        continue;
                    }
                    // Check for ;; at the right nesting level
                    if (nested_depth == 0 && s[0] == ';' && s[1] == ';') {
                        double_semi = s;
                        break;
                    }
                }

                if (double_semi) {
                    // Command ends with ;; on this line
                    *double_semi = '\0';
                    if (*after_paren) {
                        int cmd_result = script_process_line(after_paren);
                        result_exit_code = last_command_exit_code;
                        if (cmd_result == 0) {
                            free(body_copy);
                            return result_exit_code;
                        }
                    }
                    in_matched_clause = false;
                } else {
                    // Command continues, no ;; on this line
                    int cmd_result = script_process_line(after_paren);
                    result_exit_code = last_command_exit_code;
                    if (cmd_result == 0) {
                        free(body_copy);
                        return result_exit_code;
                    }
                }
            }
        }

        line = next_line;
    }

    free(body_copy);
    return result_exit_code;
}

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
                if (next == '$' || next == '`' || next == '"' ||
                    next == '\\' || next == '\n') {
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
        // Need to escape if it's a backslash
        if (c == '\x01' && str[i + 1]) {
            i++;  // Skip marker
            if (str[i] == '\\') {
                result[j++] = '\\';  // Escape for fnmatch
            }
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
                    if (next == '$' || next == '`' || next == '"' ||
                        next == '\\' || next == '\n') {
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
                    if (str[i] != '\n') {  // Backslash-newline is removed
                        result[j++] = str[i];
                    }
                    continue;
                }
            }
        }

        result[j++] = c;
    }

    result[j] = '\0';
    return result;
}

// Pre-process word to add \x02 markers before $ inside double quotes
// This tells varexpand that the expansion is in a quoted context
static char *add_quote_markers(const char *word) {
    if (!word) return strdup("");

    size_t len = strlen(word);
    // Worst case: every char needs a marker
    char *result = malloc(len * 2 + 1);
    if (!result) return strdup(word);

    size_t j = 0;
    bool in_double = false;
    bool in_single = false;

    for (size_t i = 0; i < len; i++) {
        char c = word[i];

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

        // Mark $ in double quotes with \x02
        if (c == '$' && in_double && !in_single) {
            result[j++] = '\x02';
        }

        result[j++] = c;
    }

    result[j] = '\0';
    return result;
}

// Pre-process pattern to mark ALL $ with \x02 (not just in double quotes)
// For patterns, all expanded content should be literal
static char *add_pattern_markers(const char *word) {
    if (!word) return strdup("");

    size_t len = strlen(word);
    char *result = malloc(len * 2 + 1);
    if (!result) return strdup(word);

    size_t j = 0;
    bool in_single = false;

    for (size_t i = 0; i < len; i++) {
        char c = word[i];

        if (c == '\'' && !in_single) {
            in_single = !in_single;
            result[j++] = c;
            continue;
        }
        if (c == '\'') {
            in_single = !in_single;
            result[j++] = c;
            continue;
        }

        // Mark ALL $ with \x02 (except in single quotes where $ is literal)
        if (c == '$' && !in_single) {
            result[j++] = '\x02';
        }

        result[j++] = c;
    }

    result[j] = '\0';
    return result;
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

    // Apply variable expansion
    char *varexp = varexpand_expand(result, last_command_exit_code);
    if (varexp) {
        free(result);
        result = varexp;
    }

    // Strip \x03 IFS markers from expansion (but NOT \x01 which protect literals)
    // Case words don't undergo IFS splitting, but \x03 markers still need removal
    // Note: \x01 markers are handled by remove_shell_quotes below
    {
        const char *read = result;
        char *write = result;
        while (*read) {
            if (*read != '\x03') {
                *write++ = *read;
            }
            read++;
        }
        *write = '\0';
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

    // Apply variable expansion only if there's a $ to expand
    // Skip varexpand for patterns without $ to avoid its \\ processing
    // which interferes with quote removal
    bool has_dollar = (strchr(result, '$') != NULL);
    if (has_dollar) {
        char *varexp = varexpand_expand(result, last_command_exit_code);
        if (varexp) {
            free(result);
            result = varexp;
        }
    }

    // Remove shell quotes (quote removal phase)
    // Use pattern-specific version that escapes literal backslashes for fnmatch
    char *unquoted = remove_shell_quotes_for_pattern(result);
    if (unquoted) {
        free(result);
        result = unquoted;
    }

    return result;
}

static int process_case(const char *line) {
    if (script_push_context(CTX_CASE) < 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    // Parse: case word in
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "case", 4) != 0) return -1;
    p += 4;
    while (*p && isspace(*p)) p++;

    // Extract the word (may be quoted or contain $(...) or $((...)))
    char word[256];
    size_t wi = 0;
    bool in_single = false, in_double = false;
    int subst_depth = 0;  // Track $() and $(()) nesting level
    int paren_depth = 0;  // Track () inside substitutions

    while (*p && wi < sizeof(word) - 1) {
        if (*p == '\'' && !in_double && subst_depth == 0) {
            in_single = !in_single;
            word[wi++] = *p++;
            continue;
        }
        if (*p == '"' && !in_single && subst_depth == 0) {
            in_double = !in_double;
            word[wi++] = *p++;
            continue;
        }
        // Track $( - entering command/arithmetic substitution
        if (!in_single && *p == '$' && *(p + 1) == '(') {
            subst_depth++;
            word[wi++] = *p++;
            word[wi++] = *p++;
            // Check for $(( - arithmetic
            if (*p == '(') {
                word[wi++] = *p++;
                paren_depth++;  // Extra ( for $((
            }
            continue;
        }
        // Track nested ( inside substitution
        if (!in_single && subst_depth > 0 && *p == '(') {
            paren_depth++;
            word[wi++] = *p++;
            continue;
        }
        // Track ) - could be closing substitution or nested paren
        if (!in_single && subst_depth > 0 && *p == ')') {
            if (paren_depth > 0) {
                paren_depth--;
                word[wi++] = *p++;
            } else {
                // Closing the substitution
                word[wi++] = *p++;
                if (*p == ')') {
                    // )) for $((...))
                    word[wi++] = *p++;
                }
                subst_depth--;
            }
            continue;
        }
        if (!in_single && !in_double && subst_depth == 0 && isspace(*p)) {
            break;
        }
        word[wi++] = *p++;
    }
    word[wi] = '\0';

    // Skip to 'in' keyword
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "in", 2) != 0 || (!isspace(p[2]) && p[2] != '\0')) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: expected 'in' after case word\n", HASH_NAME);
        }
        script_pop_context();
        return -1;
    }

    // Expand the word (variable expansion, etc.)
    // For now, store the raw word - expansion happens at match time
    ctx->case_word = strdup(word);
    ctx->case_matched = false;
    ctx->should_execute = parent_executing;

    // Check if 'esac' is on the same line (single-line case statement)
    // Skip past 'in'
    p += 2;  // Skip 'in'
    while (*p && isspace(*p)) p++;

    // Look for matching 'esac' in the remaining line
    // Need to handle nested case statements by counting case/esac pairs
    const char *esac_pos = NULL;
    int case_depth = 1;  // We're already inside one case
    const char *scan = p;

    while (*scan) {
        // Check for 'case' keyword (increases depth)
        if (strncmp(scan, "case", 4) == 0 &&
            (scan == p || isspace(*(scan - 1)) || *(scan - 1) == ';') &&
            (isspace(scan[4]) || scan[4] == '\0')) {
            case_depth++;
            scan += 4;
            continue;
        }
        // Check for 'esac' keyword (decreases depth)
        if (strncmp(scan, "esac", 4) == 0 &&
            (scan == p || isspace(*(scan - 1)) || *(scan - 1) == ';') &&
            (scan[4] == '\0' || isspace(scan[4]) || scan[4] == ';')) {
            case_depth--;
            if (case_depth == 0) {
                esac_pos = scan;
                break;
            }
            scan += 4;
            continue;
        }
        scan++;
    }

    if (esac_pos) {
        // Single-line case statement - extract body between 'in' and 'esac'
        size_t body_len = esac_pos - p;
        char *body = malloc(body_len + 1);
        if (body) {
            memcpy(body, p, body_len);
            body[body_len] = '\0';

            // Execute the case body if parent allows
            extern int last_command_exit_code;
            if (parent_executing) {
                char *expanded_word = expand_case_word(ctx->case_word);
                int exit_code = execute_case_body(body, expanded_word);
                last_command_exit_code = exit_code;
                free(expanded_word);
            }

            free(body);
        }

        // Pop context
        script_pop_context();

        // Check for content after 'esac' and process it
        const char *after_esac = esac_pos + 4;  // Skip 'esac'
        while (*after_esac && isspace(*after_esac)) after_esac++;
        if (*after_esac == ';') after_esac++;  // Skip optional semicolon
        while (*after_esac && isspace(*after_esac)) after_esac++;

        if (*after_esac && *after_esac != '#') {
            // There's more content after the case statement
            return script_process_line(after_esac);
        }

        return 1;
    }

    // Multi-line case - start collecting the case body
    ctx->collecting_body = true;
    ctx->body_nesting_depth = 0;  // Track nested case statements
    ctx->loop_body = NULL;
    ctx->loop_body_len = 0;
    ctx->loop_body_cap = 0;

    return 1;  // Continue processing
}

static int process_esac(const char *line) {
    (void)line;

    ScriptContext *ctx = get_current_context();
    if (!ctx || ctx->type != CTX_CASE) {
        if (!script_state.silent_errors) {
            fprintf(stderr, "%s: syntax error: unexpected 'esac'\n", HASH_NAME);
        }
        return -1;
    }

    // Stop collecting the case body
    ctx->collecting_body = false;

    // Check if parent context allows execution
    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    extern int last_command_exit_code;

    if (parent_executing && ctx->case_word && ctx->loop_body) {
        // Expand the case word before matching
        char *expanded_word = expand_case_word(ctx->case_word);
        int exit_code = execute_case_body(ctx->loop_body, expanded_word);
        last_command_exit_code = exit_code;
        free(expanded_word);
    } else if (parent_executing && ctx->case_word && !ctx->loop_body) {
        // Empty case body - exit code is 0
        last_command_exit_code = 0;
    }

    return script_pop_context();
}

// ============================================================================
// Script Line Processing
// ============================================================================

// Internal function that processes a single logical line (no semicolons)
static int process_single_line(const char *line);

int script_process_line(const char *line) {
    if (!line) return 0;

    // If running in interactive mode, add commands to history (unless nolog is set)
    if (is_interactive && !shell_option_nolog() && line[0] != '\0') {
        // Skip whitespace-only lines
        const char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p != '\0') {
            history_add(line);
        }
    }

    // Check if we're inside a function body or loop/case body being collected
    // If so, don't split - buffer the full line
    const ScriptContext *ctx = get_current_context();
    bool collecting = false;
    if (ctx) {
        if (ctx->type == CTX_FUNCTION && ctx->brace_depth > 0) {
            collecting = true;
        } else if (ctx->collecting_body &&
                   (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL || ctx->type == CTX_CASE)) {
            collecting = true;
        }
    }

    // Check if this line starts a case statement - don't split on semicolons
    // because ;; is the clause terminator and shouldn't be split
    const char *p = line;
    while (*p && isspace(*p)) p++;
    bool starts_case = (strncmp(p, "case", 4) == 0 && (isspace(p[4]) || p[4] == '\0'));

    // If not collecting and line contains semicolons (and not starting a case), split and process each part
    if (!collecting && !starts_case && strchr(line, ';')) {
        int count;
        char **parts = split_by_semicolons(line, &count);
        if (parts && count > 1) {
            int result = 1;
            for (int i = 0; i < count && result > 0; i++) {
                result = process_single_line(parts[i]);
            }
            free_split_parts(parts, count);
            return result;
        }
        if (parts) {
            free_split_parts(parts, count);
        }
        // Fall through to process as single line if splitting failed or only 1 part
    }

    return process_single_line(line);
}

static int process_single_line(const char *line) {
    if (!line) return 0;

    script_state.script_line++;

    LineType ltype = script_classify_line(line);

    if (ltype == LINE_EMPTY) {
        // Even empty lines need to be counted for function/loop/case bodies
        ScriptContext *ctx = get_current_context();
        if (ctx && ctx->type == CTX_FUNCTION && ctx->brace_depth > 0) {
            append_to_func_body(ctx, "");
        } else if (ctx && ctx->collecting_body &&
                   (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL)) {
            append_to_loop_body(ctx, "");
        } else if (ctx && ctx->collecting_body && ctx->type == CTX_CASE) {
            append_to_case_body(ctx, "");
        }
        return 1;  // Continue processing
    }

    // If we're inside a function definition, accumulate lines
    ScriptContext *ctx = get_current_context();
    if (ctx && ctx->type == CTX_FUNCTION && ctx->brace_depth > 0) {
        // Check for closing brace
        if (ltype == LINE_RBRACE) {
            return process_rbrace(line);
        }

        // Track brace depth and add line to body
        int delta = count_braces(line);
        ctx->brace_depth += delta;

        // If this is the closing brace, don't add it to the body
        if (ctx->brace_depth <= 0) {
            // Function definition complete
            script_define_function(ctx->func_name, ctx->func_body ? ctx->func_body : "");
            return script_pop_context();
        }

        append_to_func_body(ctx, line);
        return 1;  // Continue processing
    }

    // If we're collecting a loop body, buffer lines until 'done'
    if (ctx && ctx->collecting_body &&
        (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL)) {
        // Track nested loops - increment depth for for/while/until
        if (ltype == LINE_FOR_START || ltype == LINE_WHILE_START || ltype == LINE_UNTIL_START) {
            ctx->body_nesting_depth++;
            append_to_loop_body(ctx, line);
            return 1;
        }
        // Check for 'done' - only end collection when nesting depth is 0
        if (ltype == LINE_DONE) {
            if (ctx->body_nesting_depth > 0) {
                // This 'done' is for a nested loop, just append it
                ctx->body_nesting_depth--;
                append_to_loop_body(ctx, line);
                return 1;
            }
            // This 'done' is for our loop - process it
            return process_done(line);
        }
        // Buffer the line for later execution
        append_to_loop_body(ctx, line);
        // If there's pending heredoc content, append it too (with delimiter)
        if (pending_heredoc) {
            int strip_tabs = 0;
            int quoted = 0;
            char *delim = redirect_get_heredoc_delim(line, &strip_tabs, &quoted);
            if (delim) {
                append_to_loop_body(ctx, pending_heredoc);
                append_to_loop_body(ctx, delim);
                free(delim);
            }
        }
        return 1;  // Continue processing
    }

    // If we're collecting a case body, buffer lines until 'esac'
    if (ctx && ctx->collecting_body && ctx->type == CTX_CASE) {
        // Track nested case statements
        if (ltype == LINE_CASE_START) {
            ctx->body_nesting_depth++;
            append_to_case_body(ctx, line);
            return 1;
        }
        // Check for 'esac' - only end collection when nesting depth is 0
        if (ltype == LINE_ESAC) {
            if (ctx->body_nesting_depth > 0) {
                // This 'esac' is for a nested case, just append it
                ctx->body_nesting_depth--;
                append_to_case_body(ctx, line);
                return 1;
            }
            // This 'esac' is for our case - process it
            return process_esac(line);
        }
        // Buffer the line for later execution
        append_to_case_body(ctx, line);
        return 1;  // Continue processing
    }

    switch (ltype) {
        case LINE_IF_START:
            return process_if(line);
        case LINE_THEN:
            return process_then(line);
        case LINE_ELIF:
            return process_elif(line);
        case LINE_ELSE:
            return process_else(line);
        case LINE_FI:
            return process_fi(line);
        case LINE_FOR_START:
            return process_for(line);
        case LINE_WHILE_START:
            return process_while(line);
        case LINE_UNTIL_START:
            return process_until(line);
        case LINE_DO:
            return process_do(line);
        case LINE_DONE:
            return process_done(line);
        case LINE_CASE_START:
            return process_case(line);
        case LINE_ESAC:
            return process_esac(line);
        case LINE_FUNCTION_START:
            return process_function(line);
        case LINE_LBRACE:
            return process_lbrace(line);
        case LINE_RBRACE:
            return process_rbrace(line);
        case LINE_SIMPLE:
        default:
            if (script_should_execute()) {
                return execute_simple_line(line);
            }
            return 1;  // Continue processing (not executing due to context)
    }
}

// ============================================================================
// Script File Execution
// ============================================================================

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

    // Save and set silent_errors flag
    // This flag is inherited by nested source operations
    bool old_silent = script_state.silent_errors;
    if (silent_errors) {
        script_state.silent_errors = true;
    }

    // POSIX lexical scoping: save and reset break/continue state for sourced files
    // break/continue inside a sourced file should NOT affect the caller's loops
    int old_break_pending = break_pending;
    int old_continue_pending = continue_pending;
    break_pending = 0;
    continue_pending = 0;

    // Increment function call depth so break/continue only see loops in this file
    script_state.function_call_depth++;

    // Save context depth to detect unclosed structures only from THIS file
    int saved_context_depth = script_state.context_depth;

    // Set up script state
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

    // Skip shebang line if present
    char *first_line = read_complete_line(fp);
    if (first_line) {
        if (first_line[0] != '#' || first_line[1] != '!') {
            // Not a shebang, process this line
            // Check for heredoc and collect content if present
            if (redirect_has_heredoc(first_line)) {
                int strip_tabs = 0;
                int quoted = 0;
                char *delim = redirect_get_heredoc_delim(first_line, &strip_tabs, &quoted);
                if (delim) {
                    free(pending_heredoc);
                    pending_heredoc = heredoc_collect_from_file(fp, delim, strip_tabs, quoted);
                    pending_heredoc_quoted = quoted;
                    free(delim);
                }
            }
            script_state.script_line = 0;  // Will be incremented by process_line
            result = script_process_line(first_line);
            // Clear pending heredoc after processing
            free(pending_heredoc);
            pending_heredoc = NULL;
            pending_heredoc_quoted = 0;
        }
        free(first_line);
    }

    // Process remaining lines (stop if result == 0 means exit was called)
    while (result > 0) {
        char *full_line = read_complete_line(fp);
        if (!full_line) break;

        // Check for heredoc and collect content if present
        if (redirect_has_heredoc(full_line)) {
            int strip_tabs = 0;
            int quoted = 0;
            char *delim = redirect_get_heredoc_delim(full_line, &strip_tabs, &quoted);
            if (delim) {
                free(pending_heredoc);
                pending_heredoc = heredoc_collect_from_file(fp, delim, strip_tabs, quoted);
                pending_heredoc_quoted = quoted;
                free(delim);
            }
        }

        result = script_process_line(full_line);
        free(full_line);

        // Clear pending heredoc after processing
        free(pending_heredoc);
        pending_heredoc = NULL;
        pending_heredoc_quoted = 0;
    }

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
    script_state.in_script = false;
    script_state.script_path = NULL;
    script_state.silent_errors = old_silent;  // Restore silent flag

    // POSIX lexical scoping: restore break/continue state
    // Any break/continue set inside the sourced file stays inside that file
    script_state.function_call_depth--;
    break_pending = old_break_pending;
    continue_pending = old_continue_pending;

    // For return (-2), use the last exit code
    // For other errors (< 0), return 1
    return (result < 0 && result != -2) ? 1 : execute_get_last_exit_code();
}

// Preprocess script to handle backslash-newline continuations
// Respects quote context: \<newline> is literal inside single quotes
// Also respects comments: quotes inside comments are ignored
// Returns newly allocated string (caller must free)
static char *preprocess_line_continuations(const char *script) {
    if (!script) return NULL;

    size_t len = strlen(script);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool in_comment = false;
    int brace_depth = 0;  // Track ${...} depth - # inside braces is not a comment

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = script[i];

        // Comments end at newline
        if (in_comment) {
            result[j++] = c;
            if (c == '\n') {
                in_comment = false;
            }
            continue;
        }

        // Check for backslash-newline (line continuation)
        // Only remove when not inside single quotes
        if (c == '\\' && i + 1 < len && script[i + 1] == '\n' && !in_single_quote) {
            // Skip both backslash and newline
            i++;  // Skip newline (loop will advance past backslash)
            continue;
        }

        result[j++] = c;

        // Track quote state
        // Backslash escapes next character when not in single quotes
        if (c == '\\' && !in_single_quote && i + 1 < len) {
            i++;
            result[j++] = script[i];
            continue;
        }
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (c == '{' && !in_single_quote) {
            brace_depth++;
        } else if (c == '}' && !in_single_quote && brace_depth > 0) {
            brace_depth--;
        } else if (c == '#' && !in_single_quote && !in_double_quote && brace_depth == 0) {
            in_comment = true;
        }
    }
    result[j] = '\0';
    return result;
}

int script_execute_string(const char *script) {
    if (!script) return 0;

    // Preprocess to handle backslash-newline continuations
    char *preprocessed = preprocess_line_continuations(script);
    if (!preprocessed) return 1;

    char *script_copy = preprocessed;

    bool old_in_script = script_state.in_script;
    script_state.in_script = true;

    int result = 1;  // 1 = continue, 0 = exit called, < 0 = error
    char *saveptr;
    const char *line = strtok_r(script_copy, "\n", &saveptr);

    while (line && result > 0) {
        result = script_process_line(line);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    script_state.in_script = old_in_script;
    free(script_copy);

    // Return exit code for compatibility with main.c and cmdsub.c
    // For return (-2), use the last exit code
    // For other errors (< 0), return 1
    return (result < 0 && result != -2) ? 1 : execute_get_last_exit_code();
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

// Get the pending heredoc content (for heredoc execution)
const char *script_get_pending_heredoc(void) {
    return pending_heredoc;
}

// Get whether the pending heredoc delimiter was quoted (no expansion)
int script_get_pending_heredoc_quoted(void) {
    return pending_heredoc_quoted;
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
