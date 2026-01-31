#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include "arith.h"
#include "safe_string.h"
#include "cmdsub.h"
#include "shellvar.h"
#include "jobs.h"
#include "config.h"

// Forward declaration to access positional parameters
// (We can't include script.h due to TokenType name collision)
const char *script_get_positional_param(int index);

// External reference to last command exit code (from builtins.c)
extern int last_command_exit_code;

// External reference to script_state for positional_count
typedef struct {
    void *context_stack;
    int context_depth;
    void *functions;
    int function_count;
    bool in_script;
    const char *script_path;
    int script_line;
    bool silent_errors;
    char **positional_params;
    int positional_count;
    int function_call_depth;
    bool exit_requested;
} ScriptStateArith;
extern ScriptStateArith script_state;

#define MAX_ARITH_LENGTH 8192

// Track if an unset variable error occurred during evaluation
static bool arith_unset_error = false;

// Get the last arithmetic error status
bool arith_had_unset_error(void) {
    return arith_unset_error;
}

// Clear the arithmetic error flag
void arith_clear_unset_error(void) {
    arith_unset_error = false;
}

// Token types for arithmetic parser
typedef enum {
    TOK_EOF,
    TOK_NUMBER,
    TOK_VAR,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_EQ,
    TOK_NE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_BAND,
    TOK_BOR,
    TOK_BXOR,
    TOK_BNOT,
    TOK_LSHIFT,
    TOK_RSHIFT,
    TOK_QUESTION,
    TOK_COLON,
    TOK_ASSIGN,
    TOK_PLUSEQ,
    TOK_MINUSEQ,
    TOK_STAREQ,
    TOK_SLASHEQ,
    TOK_PERCENTEQ,
    TOK_INC,
    TOK_DEC,
    TOK_ERROR
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    long value;
    char name[256];
} Token;

// Parser state
typedef struct {
    const char *input;
    size_t pos;
    Token current;
    int error;
} Parser;

// Forward declarations
static long parse_expression(Parser *p);
static long parse_ternary(Parser *p);
static long parse_logical_or(Parser *p);
static long parse_logical_and(Parser *p);
static long parse_bitwise_or(Parser *p);
static long parse_bitwise_xor(Parser *p);
static long parse_bitwise_and(Parser *p);
static long parse_equality(Parser *p);
static long parse_relational(Parser *p);
static long parse_shift(Parser *p);
static long parse_additive(Parser *p);
static long parse_multiplicative(Parser *p);
static long parse_unary(Parser *p);
static long parse_primary(Parser *p);

// Skip whitespace
static void skip_whitespace(Parser *p) {
    while (p->input[p->pos] && isspace(p->input[p->pos])) {
        p->pos++;
    }
}

// Check and report unset variable error when -u is set
// Returns true if error occurred
static bool check_arith_unset_error(const char *var_name) {
    if (shell_option_nounset()) {
        fprintf(stderr, "hash: %s: unbound variable\n", var_name);
        arith_unset_error = true;
        return true;
    }
    return false;
}

// Get variable value from environment or positional parameters
static long get_variable(const char *name) {
    // Handle special parameters
    if (name[0] != '\0' && name[1] == '\0') {
        switch (name[0]) {
            case '$':  // $$ - shell PID
                return (long)getpid();
            case '?':  // $? - last exit code
                return (long)last_command_exit_code;
            case '!':  // $! - last background PID
                return (long)jobs_get_last_bg_pid();
            case '#':  // $# - number of positional params
                return (long)script_state.positional_count;
            case '-':  // $- - current options (return 0 for arithmetic)
                return 0;
            case '0':  // $0 - script/shell name
                {
                    const char *val = script_get_positional_param(0);
                    if (val) {
                        return strtol(val, NULL, 10);
                    }
                    return 0;
                }
            default:
                break;
        }
    }

    // Check for positional parameters ($1, $2, etc.)
    // Single digit positional params
    if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0') {
        int idx = name[0] - '0';
        const char *val = script_get_positional_param(idx);
        if (val) {
            return strtol(val, NULL, 10);
        }
        // Check for unset positional parameter with -u
        check_arith_unset_error(name);
        return 0;
    }

    // Also check for multi-digit positional params like ${10}
    bool all_digits = true;
    for (const char *p = name; *p; p++) {
        if (*p < '0' || *p > '9') {
            all_digits = false;
            break;
        }
    }
    if (all_digits && name[0] != '\0') {
        int idx = (int)strtol(name, NULL, 10);
        const char *val = script_get_positional_param(idx);
        if (val) {
            return strtol(val, NULL, 10);
        }
        // Check for unset positional parameter with -u
        check_arith_unset_error(name);
        return 0;
    }

    // Regular shell variable (checks shell vars first, then environment)
    const char *val = shellvar_get(name);
    if (!val) {
        // Check for unset variable with -u
        check_arith_unset_error(name);
        return 0;
    }
    return strtol(val, NULL, 10);
}

// Set variable in shell variable system
static void set_variable(const char *name, long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", value);
    shellvar_set(name, buf);
}

// Get next token
static void next_token(Parser *p) {
    skip_whitespace(p);

    if (!p->input[p->pos]) {
        p->current.type = TOK_EOF;
        return;
    }

    char c = p->input[p->pos];
    char nc = p->input[p->pos + 1];

    // Numbers
    if (isdigit(c)) {
        char *end;
        p->current.value = strtol(p->input + p->pos, &end, 0);
        p->current.type = TOK_NUMBER;
        p->pos = end - p->input;
        return;
    }

    // Variable names (or starting with $)
    if (isalpha(c) || c == '_' || c == '$') {
        size_t start = p->pos;
        if (c == '$') {
            p->pos++;
            // Handle ${var} syntax
            if (p->input[p->pos] == '{') {
                p->pos++;
                start = p->pos;
                while (p->input[p->pos] && p->input[p->pos] != '}') {
                    p->pos++;
                }
                size_t len = p->pos - start;
                if (len < sizeof(p->current.name)) {
                    memcpy(p->current.name, p->input + start, len);
                    p->current.name[len] = '\0';
                }
                if (p->input[p->pos] == '}') p->pos++;
            } else {
                // Handle special variables like $$, $?, $!, $#, $@, $*, $-, $0
                char special = p->input[p->pos];
                if (special == '$' || special == '?' || special == '!' ||
                    special == '#' || special == '@' || special == '*' ||
                    special == '-' || special == '0') {
                    p->current.name[0] = special;
                    p->current.name[1] = '\0';
                    p->pos++;
                } else {
                    // Handle $var syntax
                    start = p->pos;
                    while (isalnum(p->input[p->pos]) || p->input[p->pos] == '_') {
                        p->pos++;
                    }
                    size_t len = p->pos - start;
                    if (len < sizeof(p->current.name)) {
                        memcpy(p->current.name, p->input + start, len);
                        p->current.name[len] = '\0';
                    }
                }
            }
        } else {
            while (isalnum(p->input[p->pos]) || p->input[p->pos] == '_') {
                p->pos++;
            }
            size_t len = p->pos - start;
            if (len < sizeof(p->current.name)) {
                memcpy(p->current.name, p->input + start, len);
                p->current.name[len] = '\0';
            }
        }
        p->current.type = TOK_VAR;
        p->current.value = get_variable(p->current.name);
        return;
    }

    // Two-character operators
    if (c == '+' && nc == '+') { p->current.type = TOK_INC; p->pos += 2; return; }
    if (c == '-' && nc == '-') { p->current.type = TOK_DEC; p->pos += 2; return; }
    if (c == '+' && nc == '=') { p->current.type = TOK_PLUSEQ; p->pos += 2; return; }
    if (c == '-' && nc == '=') { p->current.type = TOK_MINUSEQ; p->pos += 2; return; }
    if (c == '*' && nc == '=') { p->current.type = TOK_STAREQ; p->pos += 2; return; }
    if (c == '/' && nc == '=') { p->current.type = TOK_SLASHEQ; p->pos += 2; return; }
    if (c == '%' && nc == '=') { p->current.type = TOK_PERCENTEQ; p->pos += 2; return; }
    if (c == '<' && nc == '=') { p->current.type = TOK_LE; p->pos += 2; return; }
    if (c == '>' && nc == '=') { p->current.type = TOK_GE; p->pos += 2; return; }
    if (c == '=' && nc == '=') { p->current.type = TOK_EQ; p->pos += 2; return; }
    if (c == '!' && nc == '=') { p->current.type = TOK_NE; p->pos += 2; return; }
    if (c == '&' && nc == '&') { p->current.type = TOK_AND; p->pos += 2; return; }
    if (c == '|' && nc == '|') { p->current.type = TOK_OR; p->pos += 2; return; }
    if (c == '<' && nc == '<') { p->current.type = TOK_LSHIFT; p->pos += 2; return; }
    if (c == '>' && nc == '>') { p->current.type = TOK_RSHIFT; p->pos += 2; return; }

    // Single-character operators
    switch (c) {
        case '+': p->current.type = TOK_PLUS; break;
        case '-': p->current.type = TOK_MINUS; break;
        case '*': p->current.type = TOK_STAR; break;
        case '/': p->current.type = TOK_SLASH; break;
        case '%': p->current.type = TOK_PERCENT; break;
        case '(': p->current.type = TOK_LPAREN; break;
        case ')': p->current.type = TOK_RPAREN; break;
        case '<': p->current.type = TOK_LT; break;
        case '>': p->current.type = TOK_GT; break;
        case '!': p->current.type = TOK_NOT; break;
        case '&': p->current.type = TOK_BAND; break;
        case '|': p->current.type = TOK_BOR; break;
        case '^': p->current.type = TOK_BXOR; break;
        case '~': p->current.type = TOK_BNOT; break;
        case '?': p->current.type = TOK_QUESTION; break;
        case ':': p->current.type = TOK_COLON; break;
        case '=': p->current.type = TOK_ASSIGN; break;
        default:
            p->current.type = TOK_ERROR;
            p->error = 1;
            break;
    }
    p->pos++;
}

// Primary: number, variable, (expression)
static long parse_primary(Parser *p) {
    if (p->error) return 0;

    if (p->current.type == TOK_NUMBER) {
        long val = p->current.value;
        next_token(p);
        return val;
    }

    if (p->current.type == TOK_VAR) {
        char name[256];
        safe_strcpy(name, p->current.name, sizeof(name));
        long val = p->current.value;
        next_token(p);

        // Check for post-increment/decrement
        if (p->current.type == TOK_INC) {
            set_variable(name, val + 1);
            next_token(p);
            return val;  // Return original value
        }
        if (p->current.type == TOK_DEC) {
            set_variable(name, val - 1);
            next_token(p);
            return val;  // Return original value
        }

        // Check for assignment operators
        if (p->current.type == TOK_ASSIGN) {
            next_token(p);
            long newval = parse_ternary(p);
            set_variable(name, newval);
            return newval;
        }
        if (p->current.type == TOK_PLUSEQ) {
            next_token(p);
            long newval = val + parse_ternary(p);
            set_variable(name, newval);
            return newval;
        }
        if (p->current.type == TOK_MINUSEQ) {
            next_token(p);
            long newval = val - parse_ternary(p);
            set_variable(name, newval);
            return newval;
        }
        if (p->current.type == TOK_STAREQ) {
            next_token(p);
            long newval = val * parse_ternary(p);
            set_variable(name, newval);
            return newval;
        }
        if (p->current.type == TOK_SLASHEQ) {
            next_token(p);
            long divisor = parse_ternary(p);
            if (divisor == 0) {
                p->error = 1;
                return 0;
            }
            long newval = val / divisor;
            set_variable(name, newval);
            return newval;
        }
        if (p->current.type == TOK_PERCENTEQ) {
            next_token(p);
            long divisor = parse_ternary(p);
            if (divisor == 0) {
                p->error = 1;
                return 0;
            }
            long newval = val % divisor;
            set_variable(name, newval);
            return newval;
        }

        return val;
    }

    if (p->current.type == TOK_LPAREN) {
        next_token(p);
        long val = parse_expression(p);
        if (p->current.type == TOK_RPAREN) {
            next_token(p);
        } else {
            p->error = 1;
        }
        return val;
    }

    // Unexpected token
    p->error = 1;
    return 0;
}

// Unary: +, -, !, ~, ++var, --var
static long parse_unary(Parser *p) {
    if (p->error) return 0;

    if (p->current.type == TOK_PLUS) {
        next_token(p);
        return parse_unary(p);
    }
    if (p->current.type == TOK_MINUS) {
        next_token(p);
        return -parse_unary(p);
    }
    if (p->current.type == TOK_NOT) {
        next_token(p);
        return !parse_unary(p);
    }
    if (p->current.type == TOK_BNOT) {
        next_token(p);
        return ~parse_unary(p);
    }
    if (p->current.type == TOK_INC) {
        next_token(p);
        if (p->current.type == TOK_VAR) {
            char name[256];
            safe_strcpy(name, p->current.name, sizeof(name));
            long val = p->current.value + 1;
            set_variable(name, val);
            next_token(p);
            return val;
        }
        p->error = 1;
        return 0;
    }
    if (p->current.type == TOK_DEC) {
        next_token(p);
        if (p->current.type == TOK_VAR) {
            char name[256];
            safe_strcpy(name, p->current.name, sizeof(name));
            long val = p->current.value - 1;
            set_variable(name, val);
            next_token(p);
            return val;
        }
        p->error = 1;
        return 0;
    }

    return parse_primary(p);
}

// Multiplicative: *, /, %
static long parse_multiplicative(Parser *p) {
    if (p->error) return 0;

    long left = parse_unary(p);

    while (!p->error && (p->current.type == TOK_STAR ||
                         p->current.type == TOK_SLASH ||
                         p->current.type == TOK_PERCENT)) {
        TokenType op = p->current.type;
        next_token(p);
        long right = parse_unary(p);

        if (op == TOK_STAR) {
            left = left * right;
        } else if (op == TOK_SLASH) {
            if (right == 0) {
                p->error = 1;
                return 0;
            }
            left = left / right;
        } else {
            if (right == 0) {
                p->error = 1;
                return 0;
            }
            left = left % right;
        }
    }

    return left;
}

// Additive: +, -
static long parse_additive(Parser *p) {
    if (p->error) return 0;

    long left = parse_multiplicative(p);

    while (!p->error && (p->current.type == TOK_PLUS ||
                         p->current.type == TOK_MINUS)) {
        TokenType op = p->current.type;
        next_token(p);
        long right = parse_multiplicative(p);

        if (op == TOK_PLUS) {
            left = left + right;
        } else {
            left = left - right;
        }
    }

    return left;
}

// Shift: <<, >>
static long parse_shift(Parser *p) {
    if (p->error) return 0;

    long left = parse_additive(p);

    while (!p->error && (p->current.type == TOK_LSHIFT ||
                         p->current.type == TOK_RSHIFT)) {
        TokenType op = p->current.type;
        next_token(p);
        long right = parse_additive(p);

        if (op == TOK_LSHIFT) {
            left = left << right;
        } else {
            left = left >> right;
        }
    }

    return left;
}

// Relational: <, >, <=, >=
static long parse_relational(Parser *p) {
    if (p->error) return 0;

    long left = parse_shift(p);

    while (!p->error && (p->current.type == TOK_LT ||
                         p->current.type == TOK_GT ||
                         p->current.type == TOK_LE ||
                         p->current.type == TOK_GE)) {
        TokenType op = p->current.type;
        next_token(p);
        long right = parse_shift(p);

        if (op == TOK_LT) left = left < right;
        else if (op == TOK_GT) left = left > right;
        else if (op == TOK_LE) left = left <= right;
        else left = left >= right;
    }

    return left;
}

// Equality: ==, !=
static long parse_equality(Parser *p) {
    if (p->error) return 0;

    long left = parse_relational(p);

    while (!p->error && (p->current.type == TOK_EQ ||
                         p->current.type == TOK_NE)) {
        TokenType op = p->current.type;
        next_token(p);
        long right = parse_relational(p);

        if (op == TOK_EQ) left = left == right;
        else left = left != right;
    }

    return left;
}

// Bitwise AND: &
static long parse_bitwise_and(Parser *p) {
    if (p->error) return 0;

    long left = parse_equality(p);

    while (!p->error && p->current.type == TOK_BAND) {
        next_token(p);
        long right = parse_equality(p);
        left = left & right;
    }

    return left;
}

// Bitwise XOR: ^
static long parse_bitwise_xor(Parser *p) {
    if (p->error) return 0;

    long left = parse_bitwise_and(p);

    while (!p->error && p->current.type == TOK_BXOR) {
        next_token(p);
        long right = parse_bitwise_and(p);
        left = left ^ right;
    }

    return left;
}

// Bitwise OR: |
static long parse_bitwise_or(Parser *p) {
    if (p->error) return 0;

    long left = parse_bitwise_xor(p);

    while (!p->error && p->current.type == TOK_BOR) {
        next_token(p);
        long right = parse_bitwise_xor(p);
        left = left | right;
    }

    return left;
}

// Logical AND: &&
static long parse_logical_and(Parser *p) {
    if (p->error) return 0;

    long left = parse_bitwise_or(p);

    while (!p->error && p->current.type == TOK_AND) {
        next_token(p);
        // Short-circuit evaluation
        if (!left) {
            parse_bitwise_or(p);  // Parse but ignore result
            left = 0;
        } else {
            left = parse_bitwise_or(p) ? 1 : 0;
        }
    }

    return left;
}

// Logical OR: ||
static long parse_logical_or(Parser *p) {
    if (p->error) return 0;

    long left = parse_logical_and(p);

    while (!p->error && p->current.type == TOK_OR) {
        next_token(p);
        // Short-circuit evaluation
        if (left) {
            parse_logical_and(p);  // Parse but ignore result
            left = 1;
        } else {
            left = parse_logical_and(p) ? 1 : 0;
        }
    }

    return left;
}

// Ternary: cond ? true_expr : false_expr
static long parse_ternary(Parser *p) {
    if (p->error) return 0;

    long cond = parse_logical_or(p);

    if (p->current.type == TOK_QUESTION) {
        next_token(p);
        long true_val = parse_ternary(p);

        if (p->current.type != TOK_COLON) {
            p->error = 1;
            return 0;
        }
        next_token(p);
        long false_val = parse_ternary(p);

        return cond ? true_val : false_val;
    }

    return cond;
}

// Top-level expression (handles comma operator)
static long parse_expression(Parser *p) {
    if (p->error) return 0;

    long result = parse_ternary(p);

    // Handle comma operator (rare, but POSIX)
    while (!p->error && p->input[p->pos] == ',') {
        p->pos++;
        next_token(p);
        result = parse_ternary(p);
    }

    return result;
}

// Main evaluation function
int arith_evaluate(const char *expr, long *result) {
    if (!expr || !result) return -1;

    Parser p = {0};
    p.input = expr;
    p.pos = 0;
    p.error = 0;

    next_token(&p);
    *result = parse_expression(&p);

    // Check for trailing garbage
    skip_whitespace(&p);
    if (p.input[p.pos] != '\0') {
        p.error = 1;
    }

    return p.error ? -1 : 0;
}

// Check if string contains arithmetic expansion
int has_arith(const char *str) {
    if (!str) return 0;

    const char *p = str;
    const char *prev = NULL;
    while (*p) {
        if (*p == '$' && *(p + 1) == '(' && *(p + 2) == '(') {
            // Check if $( is escaped by \ or \x01 marker (from single quotes)
            if (prev && (*prev == '\\' || *prev == '\x01')) {
                // Escaped, skip
                p++;
                continue;
            }
            return 1;
        }
        prev = p;
        p++;
    }
    return 0;
}

// Find matching )) for $((
static const char *find_arith_end(const char *start) {
    // start points to first char after $((
    // We need to find the matching )) while tracking nested parentheses
    // Note: (( inside arithmetic is just two regular parens, not a nested $((
    int paren_depth = 0;  // Track regular ( ) balance
    const char *p = start;

    while (*p) {
        if (*p == '(') {
            paren_depth++;
        } else if (*p == ')') {
            if (paren_depth > 0) {
                paren_depth--;
            } else if (*(p + 1) == ')') {
                // Found )) with balanced inner parens - this is the end
                return p;
            }
        }
        p++;
    }

    return NULL;  // No matching )) found
}

// Expand arithmetic substitutions in a string
char *arith_expand(const char *str) {
    if (!str || !has_arith(str)) return NULL;

    char *result = malloc(MAX_ARITH_LENGTH);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    while (*p && out_pos < MAX_ARITH_LENGTH - 1) {
        // Look for $(( but not if escaped
        if (*p == '$' && *(p + 1) == '(' && *(p + 2) == '(') {
            // Check if $( is escaped by \ or \x01 marker (from single quotes)
            if (out_pos > 0 && (result[out_pos - 1] == '\\' || result[out_pos - 1] == '\x01')) {
                // Escaped - copy $ literally and continue
                result[out_pos++] = *p++;
                continue;
            }
            p += 3;  // Skip $((

            const char *end = find_arith_end(p);
            if (!end) {
                // Malformed, copy literally
                if (out_pos < MAX_ARITH_LENGTH - 3) {
                    result[out_pos++] = '$';
                    result[out_pos++] = '(';
                    result[out_pos++] = '(';
                }
                continue;
            }

            // Extract expression
            size_t expr_len = end - p;
            char *expr = malloc(expr_len + 1);
            if (!expr) {
                free(result);
                return NULL;
            }
            memcpy(expr, p, expr_len);
            expr[expr_len] = '\0';

            // First, expand any command substitutions in the expression
            char *cmdsub_expanded = cmdsub_expand(expr);
            if (cmdsub_expanded) {
                free(expr);
                expr = cmdsub_expanded;
                // Strip \x03 IFS markers from cmdsub output - they're not needed
                // for arithmetic and would cause the parser to error
                const char *read = expr;
                char *write = expr;
                while (*read) {
                    if (*read != '\x03') {
                        *write++ = *read;
                    }
                    read++;
                }
                *write = '\0';
            }

            // Evaluate
            long value;
            if (arith_evaluate(expr, &value) == 0) {
                // Format result
                char buf[32];
                int n = snprintf(buf, sizeof(buf), "%ld", value);
                if (n > 0 && out_pos + n < MAX_ARITH_LENGTH) {
                    memcpy(result + out_pos, buf, n);
                    out_pos += n;
                }
            } else {
                // Error - output 0 or keep original?
                // POSIX says error, but let's be lenient
                result[out_pos++] = '0';
            }

            free(expr);
            p = end + 2;  // Skip ))
            continue;
        }

        // Regular character
        result[out_pos++] = *p++;
    }

    result[out_pos] = '\0';
    return result;
}

// Expand arithmetic substitutions in all arguments
int arith_args(char **args) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (has_arith(args[i])) {
            char *expanded = arith_expand(args[i]);
            if (expanded) {
                args[i] = expanded;
            }
        }
    }

    return 0;
}
