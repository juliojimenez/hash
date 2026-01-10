#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include "script.h"
#include "hash.h"
#include "safe_string.h"
#include "parser.h"
#include "execute.h"
#include "chain.h"
#include "colors.h"
#include "test_builtin.h"

// Global script state
ScriptState script_state;

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
                num_parts++;
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
                    at_semi = true;
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
            // Trim trailing whitespace
            while (len > 0 && isspace(start[len-1])) {
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
    int current_depth = script_state.function_call_depth;

    // Count loops that were created at the current function call depth
    for (int i = 0; i < script_state.context_depth; i++) {
        ScriptContext *ctx = &script_state.context_stack[i];
        // Only count loops at the same function call depth (lexical scoping)
        if (ctx->function_call_depth == current_depth) {
            if (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL) {
                count++;
            }
        }
    }
    return count;
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

    CommandChain *chain = chain_parse(line_copy);
    int exit_code = 1;

    if (chain) {
        chain_execute(chain);
        exit_code = execute_get_last_exit_code();
        chain_free(chain);
    } else {
        char **args = parse_line(line_copy);
        if (args && args[0]) {
            execute(args);
            exit_code = execute_get_last_exit_code();
            free(args);
        }
    }

    free(line_copy);
    return (exit_code == 0);
}

// ============================================================================
// Function Management
// ============================================================================

int script_define_function(const char *name, const char *body) {
    if (!name || !body) return -1;

    for (int i = 0; i < script_state.function_count; i++) {
        if (strcmp(script_state.functions[i].name, name) == 0) {
            free(script_state.functions[i].body);
            script_state.functions[i].body = strdup(body);
            script_state.functions[i].body_len = strlen(body);
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

    script_state.positional_params = argv;
    script_state.positional_count = argc;
    script_state.exit_requested = false;  // Reset for this function

    // Increment function call depth for lexical scoping of break/continue
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
        script_state.exit_requested = true;
        return 0;  // Stop execution
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

    // Trim trailing whitespace
    size_t len = strlen(result);
    while (len > 0 && isspace(result[len - 1])) {
        result[--len] = '\0';
    }

    return result;
}

static int execute_simple_line(const char *line) {
    if (!line) return 0;

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

    // Check if line contains opening brace
    const char *brace = strchr(line, '{');
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

            // Parse values
            char **values = malloc(256 * sizeof(char*));
            int count = 0;

            if (values && len > 0) {
                char *saveptr;
                const char *token = strtok_r(values_str, " \t", &saveptr);
                while (token && count < 255) {
                    values[count++] = strdup(token);
                    token = strtok_r(NULL, " \t", &saveptr);
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
static int execute_loop_body(const char *body) {
    if (!body || *body == '\0') return 1;

    char *body_copy = strdup(body);
    if (!body_copy) return -1;

    char *saveptr;
    char *line = strtok_r(body_copy, "\n", &saveptr);
    int result = 1;

    while (line && result > 0) {
        // Skip empty lines
        const char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p && *p != '#') {
            // Use script_process_line to handle nested control structures
            result = script_process_line(line);
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(body_copy);
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

    if (ctx_type == CTX_FOR) {
        // Execute the loop body for each value
        while (ctx->loop_index < ctx->loop_count) {
            if (ctx->loop_var && ctx->loop_values) {
                setenv(ctx->loop_var, ctx->loop_values[ctx->loop_index], 1);
            }
            ctx->should_execute = true;
            int result = execute_loop_body(ctx->loop_body);
            body_executed = true;
            body_exit_code = last_command_exit_code;
            if (result == 0) {
                // Exit was called
                return script_pop_context();
            }
            if (result < 0) {
                // Error or break
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
                return script_pop_context();
            }
            if (result < 0) {
                // Error or break
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
                return script_pop_context();
            }
            if (result < 0) {
                // Error or break
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

    return script_pop_context();
}

// ============================================================================
// Script Line Processing
// ============================================================================

// Internal function that processes a single logical line (no semicolons)
static int process_single_line(const char *line);

int script_process_line(const char *line) {
    if (!line) return 0;

    // Check if we're inside a function body or loop body being collected
    // If so, don't split - buffer the full line
    ScriptContext *ctx = get_current_context();
    bool collecting = false;
    if (ctx) {
        if (ctx->type == CTX_FUNCTION && ctx->brace_depth > 0) {
            collecting = true;
        } else if (ctx->collecting_body &&
                   (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL)) {
            collecting = true;
        }
    }

    // If not collecting and line contains semicolons, split and process each part
    if (!collecting && strchr(line, ';')) {
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
        // Even empty lines need to be counted for function/loop bodies
        ScriptContext *ctx = get_current_context();
        if (ctx && ctx->type == CTX_FUNCTION && ctx->brace_depth > 0) {
            append_to_func_body(ctx, "");
        } else if (ctx && ctx->collecting_body &&
                   (ctx->type == CTX_FOR || ctx->type == CTX_WHILE || ctx->type == CTX_UNTIL)) {
            append_to_loop_body(ctx, "");
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

    // Save and set silent_errors flag
    // This flag is inherited by nested source operations
    bool old_silent = script_state.silent_errors;
    if (silent_errors) {
        script_state.silent_errors = true;
    }

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

    char line[MAX_SCRIPT_LINE];
    memset(line, 0, sizeof(line));  // Clear buffer to prevent corruption
    int result = 1;  // 1 = continue, 0 = exit called, < 0 = error

    // Skip shebang line if present
    if (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline from first line too
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        if (line[0] != '#' || line[1] != '!') {
            // Not a shebang, process this line
            script_state.script_line = 0;  // Will be incremented by process_line
            result = script_process_line(line);
        }
    }

    // Process remaining lines (stop if result == 0 means exit was called)
    while (result > 0) {
        memset(line, 0, sizeof(line));  // Clear buffer before each read
        if (!fgets(line, sizeof(line), fp)) break;

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        result = script_process_line(line);

        // Handle loop continuation
        // (In a real implementation, we'd need to track loop body and re-execute)
    }

    fclose(fp);

    // Check for unclosed control structures
    if (script_state.context_depth > 0) {
        if (!silent_errors && !script_state.silent_errors) {
            fprintf(stderr, "%s: %s: unexpected end of file\n", HASH_NAME, filepath);
        }
        // Clear context stack on error to prevent cascading issues
        while (script_state.context_depth > 0) {
            script_pop_context();
        }
        result = 1;
    }

    // Cleanup
    script_state.in_script = false;
    script_state.script_path = NULL;
    script_state.silent_errors = old_silent;  // Restore silent flag

    return result < 0 ? 1 : execute_get_last_exit_code();
}

int script_execute_string(const char *script) {
    if (!script) return 0;

    char *script_copy = strdup(script);
    if (!script_copy) return 1;

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
    return result < 0 ? 1 : execute_get_last_exit_code();
}

// Get a positional parameter value
const char *script_get_positional_param(int index) {
    if (index < 0 || index >= script_state.positional_count) {
        return NULL;
    }
    return script_state.positional_params ? script_state.positional_params[index] : NULL;
}
