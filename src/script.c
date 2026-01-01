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
    script_state.positional_params = NULL;
    script_state.positional_count = 0;
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
        free(ctx->case_word);
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
        fprintf(stderr, "%s: maximum nesting depth exceeded\n", HASH_NAME);
        return -1;
    }

    ScriptContext *ctx = &script_state.context_stack[script_state.context_depth];
    memset(ctx, 0, sizeof(ScriptContext));
    ctx->type = type;
    ctx->should_execute = true;
    ctx->condition_met = false;

    script_state.context_depth++;
    return 0;
}

int script_pop_context(void) {
    if (script_state.context_depth <= 0) {
        fprintf(stderr, "%s: context stack underflow\n", HASH_NAME);
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

    free(ctx->case_word);
    ctx->case_word = NULL;

    return 0;
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

    script_state.positional_params = argv;
    script_state.positional_count = argc;

    int result = script_execute_string(func->body);

    script_state.positional_params = old_params;
    script_state.positional_count = old_count;

    return result;
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
    if (script_push_context(CTX_IF) != 0) return -1;

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

    return 0;
}

static int process_then(const char *line) {
    (void)line;

    ContextType ctx_type = script_current_context();
    if (ctx_type != CTX_IF && ctx_type != CTX_ELIF) {
        fprintf(stderr, "%s: syntax error: unexpected 'then'\n", HASH_NAME);
        return -1;
    }
    return 0;
}

static int process_elif(const char *line) {
    ScriptContext *ctx = get_current_context();
    if (!ctx || (ctx->type != CTX_IF && ctx->type != CTX_ELIF && ctx->type != CTX_ELSE)) {
        fprintf(stderr, "%s: syntax error: unexpected 'elif'\n", HASH_NAME);
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

    return 0;
}

static int process_else(const char *line) {
    (void)line;

    ScriptContext *ctx = get_current_context();
    if (!ctx || (ctx->type != CTX_IF && ctx->type != CTX_ELIF)) {
        fprintf(stderr, "%s: syntax error: unexpected 'else'\n", HASH_NAME);
        return -1;
    }

    ctx->type = CTX_ELSE;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    ctx->should_execute = parent_executing && !ctx->condition_met;

    return 0;
}

static int process_fi(const char *line) {
    (void)line;

    ContextType ctx_type = script_current_context();
    if (ctx_type != CTX_IF && ctx_type != CTX_ELIF && ctx_type != CTX_ELSE) {
        fprintf(stderr, "%s: syntax error: unexpected 'fi'\n", HASH_NAME);
        return -1;
    }

    return script_pop_context();
}

static int process_for(const char *line) {
    if (script_push_context(CTX_FOR) != 0) return -1;

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
        fprintf(stderr, "%s: syntax error: expected variable name after 'for'\n", HASH_NAME);
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

    return 0;
}

static int process_while(const char *line) {
    if (script_push_context(CTX_WHILE) != 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing) {
        char *condition = extract_condition(line, "while");
        if (condition) {
            bool result = script_eval_condition(condition);
            ctx->should_execute = result;
            free(condition);
        } else {
            ctx->should_execute = false;
        }
    } else {
        ctx->should_execute = false;
    }

    return 0;
}

static int process_until(const char *line) {
    if (script_push_context(CTX_UNTIL) != 0) return -1;

    ScriptContext *ctx = get_current_context();
    if (!ctx) return -1;

    bool parent_executing = (script_state.context_depth <= 1) ||
        script_state.context_stack[script_state.context_depth - 2].should_execute;

    if (parent_executing) {
        char *condition = extract_condition(line, "until");
        if (condition) {
            bool result = script_eval_condition(condition);
            ctx->should_execute = !result;
            free(condition);
        } else {
            ctx->should_execute = false;
        }
    } else {
        ctx->should_execute = false;
    }

    return 0;
}

static int process_do(const char *line) {
    (void)line;

    ContextType ctx_type = script_current_context();
    if (ctx_type != CTX_FOR && ctx_type != CTX_WHILE && ctx_type != CTX_UNTIL) {
        fprintf(stderr, "%s: syntax error: unexpected 'do'\n", HASH_NAME);
        return -1;
    }
    return 0;
}

static int process_done(const char *line) {
    (void)line;

    ScriptContext *ctx = get_current_context();
    ContextType ctx_type = script_current_context();

    if (!ctx || (ctx_type != CTX_FOR && ctx_type != CTX_WHILE && ctx_type != CTX_UNTIL)) {
        fprintf(stderr, "%s: syntax error: unexpected 'done'\n", HASH_NAME);
        return -1;
    }

    if (ctx_type == CTX_FOR) {
        ctx->loop_index++;
        if (ctx->loop_index < ctx->loop_count) {
            if (ctx->loop_var && ctx->loop_values) {
                setenv(ctx->loop_var, ctx->loop_values[ctx->loop_index], 1);
            }
            return 1;  // Continue loop
        }
    }

    return script_pop_context();
}

// ============================================================================
// Script Line Processing
// ============================================================================

int script_process_line(const char *line) {
    if (!line) return 0;

    script_state.script_line++;

    LineType ltype = script_classify_line(line);

    if (ltype == LINE_EMPTY) {
        return 0;
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
        case LINE_SIMPLE:
        default:
            if (script_should_execute()) {
                return execute_simple_line(line);
            }
            return 0;
    }
}

// ============================================================================
// Script File Execution
// ============================================================================

int script_execute_file(const char *filepath, int argc, char **argv) {
    if (!filepath) return 1;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "%s: cannot open '%s': ", HASH_NAME, filepath);
        perror("");
        return 1;
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
    int result = 0;

    // Skip shebang line if present
    if (fgets(line, sizeof(line), fp)) {
        if (line[0] != '#' || line[1] != '!') {
            // Not a shebang, process this line
            script_state.script_line = 0;  // Will be incremented by process_line
            result = script_process_line(line);
        }
    }

    // Process remaining lines
    while (fgets(line, sizeof(line), fp) && result >= 0) {
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
        fprintf(stderr, "%s: %s: unexpected end of file\n", HASH_NAME, filepath);
        result = 1;
    }

    // Cleanup
    script_state.in_script = false;
    script_state.script_path = NULL;

    return result < 0 ? 1 : execute_get_last_exit_code();
}

int script_execute_string(const char *script) {
    if (!script) return 0;

    char *script_copy = strdup(script);
    if (!script_copy) return 1;

    bool old_in_script = script_state.in_script;
    script_state.in_script = true;

    int result = 0;
    char *saveptr;
    const char *line = strtok_r(script_copy, "\n", &saveptr);

    while (line && result >= 0) {
        result = script_process_line(line);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    script_state.in_script = old_in_script;
    free(script_copy);

    return result < 0 ? 1 : execute_get_last_exit_code();
}
