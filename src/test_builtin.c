#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <regex.h>
#include "test_builtin.h"
#include "hash.h"
#include "safe_string.h"

// Forward declarations for recursive parsing
static int eval_expr(char **args, int *pos, int argc);
static int eval_or(char **args, int *pos, int argc);
static int eval_and(char **args, int *pos, int argc);
static int eval_not(char **args, int *pos, int argc);
static int eval_primary(char **args, int *pos, int argc);

// ============================================================================
// File Tests
// ============================================================================

static int test_file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? 0 : 1;
}

static int test_file_regular(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISREG(st.st_mode) ? 0 : 1;
}

static int test_file_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISDIR(st.st_mode) ? 0 : 1;
}

static int test_file_readable(const char *path) {
    return (access(path, R_OK) == 0) ? 0 : 1;
}

static int test_file_writable(const char *path) {
    return (access(path, W_OK) == 0) ? 0 : 1;
}

static int test_file_executable(const char *path) {
    return (access(path, X_OK) == 0) ? 0 : 1;
}

static int test_file_nonempty(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_size > 0) ? 0 : 1;
}

static int test_file_symlink(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 1;
    return S_ISLNK(st.st_mode) ? 0 : 1;
}

static int test_file_block(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISBLK(st.st_mode) ? 0 : 1;
}

static int test_file_char(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISCHR(st.st_mode) ? 0 : 1;
}

static int test_file_pipe(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISFIFO(st.st_mode) ? 0 : 1;
}

static int test_file_socket(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return S_ISSOCK(st.st_mode) ? 0 : 1;
}

static int test_file_setuid(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_mode & S_ISUID) ? 0 : 1;
}

static int test_file_setgid(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_mode & S_ISGID) ? 0 : 1;
}

static int test_file_sticky(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_mode & S_ISVTX) ? 0 : 1;
}

static int test_file_owned(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_uid == getuid()) ? 0 : 1;
}

static int test_file_group_owned(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_gid == getgid()) ? 0 : 1;
}

static int test_file_newer(const char *path1, const char *path2) {
    struct stat st1, st2;
    if (stat(path1, &st1) != 0) return 1;
    if (stat(path2, &st2) != 0) return 1;
    return (st1.st_mtime > st2.st_mtime) ? 0 : 1;
}

static int test_file_older(const char *path1, const char *path2) {
    struct stat st1, st2;
    if (stat(path1, &st1) != 0) return 1;
    if (stat(path2, &st2) != 0) return 1;
    return (st1.st_mtime < st2.st_mtime) ? 0 : 1;
}

static int test_file_same(const char *path1, const char *path2) {
    struct stat st1, st2;
    if (stat(path1, &st1) != 0) return 1;
    if (stat(path2, &st2) != 0) return 1;
    return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) ? 0 : 1;
}

// ============================================================================
// String Tests
// ============================================================================

static int test_string_empty(const char *str) {
    return (str == NULL || str[0] == '\0') ? 0 : 1;
}

static int test_string_nonempty(const char *str) {
    return (str != NULL && str[0] != '\0') ? 0 : 1;
}

static int test_string_equal(const char *s1, const char *s2) {
    return (strcmp(s1, s2) == 0) ? 0 : 1;
}

static int test_string_not_equal(const char *s1, const char *s2) {
    return (strcmp(s1, s2) != 0) ? 0 : 1;
}

// ============================================================================
// Integer Comparisons
// ============================================================================

static int parse_integer(const char *str, long *result) {
    char *endptr;
    errno = 0;
    *result = strtol(str, &endptr, 10);

    // Check for errors
    if (errno != 0 || endptr == str || *endptr != '\0') {
        return -1;
    }
    return 0;
}

static int test_int_eq(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 == n2) ? 0 : 1;
}

static int test_int_ne(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 != n2) ? 0 : 1;
}

static int test_int_lt(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 < n2) ? 0 : 1;
}

static int test_int_le(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 <= n2) ? 0 : 1;
}

static int test_int_gt(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 > n2) ? 0 : 1;
}

static int test_int_ge(const char *s1, const char *s2) {
    long n1, n2;
    if (parse_integer(s1, &n1) != 0 || parse_integer(s2, &n2) != 0) {
        fprintf(stderr, "test: integer expression expected\n");
        return 2;
    }
    return (n1 >= n2) ? 0 : 1;
}

// ============================================================================
// Terminal Tests
// ============================================================================

static int test_terminal(const char *fd_str) {
    long fd;
    if (parse_integer(fd_str, &fd) != 0) {
        return 1;
    }
    return isatty((int)fd) ? 0 : 1;
}

// ============================================================================
// Expression Evaluation (Recursive Descent Parser)
// ============================================================================

// Primary expression: unary operators or atoms
// Helper to check if a string is a binary operator
static int is_binary_op(const char *s) {
    return s && (strcmp(s, "=") == 0 || strcmp(s, "==") == 0 ||
                 strcmp(s, "!=") == 0 || strcmp(s, "-eq") == 0 ||
                 strcmp(s, "-ne") == 0 || strcmp(s, "-lt") == 0 ||
                 strcmp(s, "-le") == 0 || strcmp(s, "-gt") == 0 ||
                 strcmp(s, "-ge") == 0 || strcmp(s, "-nt") == 0 ||
                 strcmp(s, "-ot") == 0 || strcmp(s, "-ef") == 0);
}

static int eval_primary(char **args, int *pos, int argc) {
    if (*pos >= argc) {
        return 1;  // No more arguments = false
    }

    const char *arg = args[*pos];

    // Handle parentheses - but only if not followed by a binary operator
    // This allows [ "(" = "(" ] to work as a string comparison
    if (strcmp(arg, "(") == 0) {
        // Check if this looks like a binary comparison: ( OP arg
        if (*pos + 1 < argc && !is_binary_op(args[*pos + 1])) {
            (*pos)++;
            int result = eval_expr(args, pos, argc);
            if (*pos < argc && strcmp(args[*pos], ")") == 0) {
                (*pos)++;
            } else {
                fprintf(stderr, "test: missing ')'\n");
                return 2;
            }
            return result;
        }
        // Otherwise fall through to treat ( as a string literal
    }

    // Unary file operators
    if (arg[0] == '-' && arg[1] != '\0' && arg[2] == '\0') {
        char op = arg[1];

        // Check if next argument exists for unary operators
        if (*pos + 1 >= argc) {
            // Single argument - treat as string test
            (*pos)++;
            return test_string_nonempty(arg);
        }

        const char *operand = args[*pos + 1];

        switch (op) {
            case 'e':
                (*pos) += 2;
                return test_file_exists(operand);
            case 'f':
                (*pos) += 2;
                return test_file_regular(operand);
            case 'd':
                (*pos) += 2;
                return test_file_directory(operand);
            case 'r':
                (*pos) += 2;
                return test_file_readable(operand);
            case 'w':
                (*pos) += 2;
                return test_file_writable(operand);
            case 'x':
                (*pos) += 2;
                return test_file_executable(operand);
            case 's':
                (*pos) += 2;
                return test_file_nonempty(operand);
            case 'L':
            case 'h':
                (*pos) += 2;
                return test_file_symlink(operand);
            case 'b':
                (*pos) += 2;
                return test_file_block(operand);
            case 'c':
                (*pos) += 2;
                return test_file_char(operand);
            case 'p':
                (*pos) += 2;
                return test_file_pipe(operand);
            case 'S':
                (*pos) += 2;
                return test_file_socket(operand);
            case 'u':
                (*pos) += 2;
                return test_file_setuid(operand);
            case 'g':
                (*pos) += 2;
                return test_file_setgid(operand);
            case 'k':
                (*pos) += 2;
                return test_file_sticky(operand);
            case 'O':
                (*pos) += 2;
                return test_file_owned(operand);
            case 'G':
                (*pos) += 2;
                return test_file_group_owned(operand);
            case 'z':
                (*pos) += 2;
                return test_string_empty(operand);
            case 'n':
                (*pos) += 2;
                return test_string_nonempty(operand);
            case 't':
                (*pos) += 2;
                return test_terminal(operand);
            default:
                // Unknown unary operator - might be a string starting with -
                break;
        }
    }

    // Check for binary operators with look-ahead
    if (*pos + 2 < argc) {
        const char *op = args[*pos + 1];

        // String comparison
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_string_equal(s1, s2);
        }
        if (strcmp(op, "!=") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_string_not_equal(s1, s2);
        }

        // Integer comparison
        if (strcmp(op, "-eq") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_eq(s1, s2);
        }
        if (strcmp(op, "-ne") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_ne(s1, s2);
        }
        if (strcmp(op, "-lt") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_lt(s1, s2);
        }
        if (strcmp(op, "-le") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_le(s1, s2);
        }
        if (strcmp(op, "-gt") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_gt(s1, s2);
        }
        if (strcmp(op, "-ge") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_ge(s1, s2);
        }

        // File comparisons
        if (strcmp(op, "-nt") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_newer(f1, f2);
        }
        if (strcmp(op, "-ot") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_older(f1, f2);
        }
        if (strcmp(op, "-ef") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_same(f1, f2);
        }
    }

    // Single string argument - true if non-empty
    (*pos)++;
    return test_string_nonempty(arg);
}

// NOT expression: ! expr
static int eval_not(char **args, int *pos, int argc) {
    if (*pos < argc && strcmp(args[*pos], "!") == 0) {
        (*pos)++;
        int result = eval_not(args, pos, argc);
        if (result == 2) return 2;  // Error propagates
        return result ? 0 : 1;  // Invert result
    }
    return eval_primary(args, pos, argc);
}

// AND expression: expr -a expr
static int eval_and(char **args, int *pos, int argc) {
    int result = eval_not(args, pos, argc);
    if (result == 2) return 2;

    while (*pos < argc && strcmp(args[*pos], "-a") == 0) {
        (*pos)++;
        int right = eval_not(args, pos, argc);
        if (right == 2) return 2;

        // Short-circuit: if left is false, result is false
        if (result != 0) {
            result = 1;
        } else {
            result = right;
        }
    }

    return result;
}

// OR expression: expr -o expr
static int eval_or(char **args, int *pos, int argc) {
    int result = eval_and(args, pos, argc);
    if (result == 2) return 2;

    while (*pos < argc && strcmp(args[*pos], "-o") == 0) {
        (*pos)++;
        int right = eval_and(args, pos, argc);
        if (right == 2) return 2;

        // Short-circuit: if left is true, result is true
        if (result == 0) {
            result = 0;
        } else {
            result = right;
        }
    }

    return result;
}

// Top-level expression
static int eval_expr(char **args, int *pos, int argc) {
    return eval_or(args, pos, argc);
}

// ============================================================================
// Public API
// ============================================================================

int test_evaluate(char **args, int argc) {
    if (argc == 0) {
        return 1;  // No arguments = false
    }

    int pos = 0;
    int result = eval_expr(args, &pos, argc);

    // Check for extra arguments
    if (result != 2 && pos < argc) {
        fprintf(stderr, "test: too many arguments\n");
        return 2;
    }

    return result;
}

int builtin_test(char **args) {
    // Skip "test" command name
    if (args[0] == NULL) {
        return 1;
    }

    // Count arguments (excluding "test")
    int argc = 0;
    while (args[argc + 1] != NULL) {
        argc++;
    }

    return test_evaluate(args + 1, argc);
}

int builtin_bracket(char **args) {
    // Skip "[" command name
    if (args[0] == NULL) {
        return 1;
    }

    // Count arguments and find closing bracket
    int argc = 0;
    while (args[argc + 1] != NULL) {
        argc++;
    }

    // Check for closing bracket
    if (argc == 0 || strcmp(args[argc], "]") != 0) {
        fprintf(stderr, "[: missing ']'\n");
        return 2;
    }

    // Evaluate without the closing bracket
    return test_evaluate(args + 1, argc - 1);
}

// ============================================================================
// [[ ]] Extended Test - Bash-style
// ============================================================================

// Pattern matching for [[ ]] (uses fnmatch)
static int test_pattern_match(const char *string, const char *pattern) {
    // fnmatch returns 0 on match
    return fnmatch(pattern, string, 0) == 0 ? 0 : 1;
}

// Regex matching for [[ =~ ]]
static int test_regex_match(const char *string, const char *pattern) {
    regex_t regex;
    int ret;

    ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        fprintf(stderr, "[[: invalid regex: %s\n", pattern);
        return 2;
    }

    ret = regexec(&regex, string, 0, NULL, 0);
    regfree(&regex);

    return (ret == 0) ? 0 : 1;
}

// String comparison for [[ < ]] and [[ > ]]
static int test_string_less_than(const char *s1, const char *s2) {
    return (strcmp(s1, s2) < 0) ? 0 : 1;
}

static int test_string_greater_than(const char *s1, const char *s2) {
    return (strcmp(s1, s2) > 0) ? 0 : 1;
}

// Forward declarations for [[ ]] recursive parsing
static int eval_double_bracket_expr(char **args, int *pos, int argc);
static int eval_double_bracket_or(char **args, int *pos, int argc);
static int eval_double_bracket_and(char **args, int *pos, int argc);
static int eval_double_bracket_not(char **args, int *pos, int argc);
static int eval_double_bracket_primary(char **args, int *pos, int argc);

// Helper to check if a string is a binary operator for [[ ]]
static int is_double_bracket_binary_op(const char *s) {
    return s && (strcmp(s, "=") == 0 || strcmp(s, "==") == 0 ||
                 strcmp(s, "!=") == 0 || strcmp(s, "=~") == 0 ||
                 strcmp(s, "<") == 0 || strcmp(s, ">") == 0 ||
                 strcmp(s, "-eq") == 0 || strcmp(s, "-ne") == 0 ||
                 strcmp(s, "-lt") == 0 || strcmp(s, "-le") == 0 ||
                 strcmp(s, "-gt") == 0 || strcmp(s, "-ge") == 0 ||
                 strcmp(s, "-nt") == 0 || strcmp(s, "-ot") == 0 ||
                 strcmp(s, "-ef") == 0);
}

// Primary expression for [[ ]]
static int eval_double_bracket_primary(char **args, int *pos, int argc) {
    if (*pos >= argc) {
        return 1;  // No more arguments = false
    }

    const char *arg = args[*pos];

    // Handle parentheses - but only if not followed by a binary operator
    // This allows [[ "(" = "(" ]] to work as a string comparison
    if (strcmp(arg, "(") == 0) {
        // Check if this looks like a binary comparison: ( OP arg
        if (*pos + 1 < argc && !is_double_bracket_binary_op(args[*pos + 1])) {
            (*pos)++;
            int result = eval_double_bracket_expr(args, pos, argc);
            if (*pos < argc && strcmp(args[*pos], ")") == 0) {
                (*pos)++;
            } else {
                fprintf(stderr, "[[: missing ')'\n");
                return 2;
            }
            return result;
        }
        // Otherwise fall through to treat ( as a string literal
    }

    // Unary file operators (same as [ ])
    if (arg[0] == '-' && arg[1] != '\0' && arg[2] == '\0') {
        char op = arg[1];

        if (*pos + 1 >= argc) {
            (*pos)++;
            return test_string_nonempty(arg);
        }

        const char *operand = args[*pos + 1];

        switch (op) {
            case 'e':
                (*pos) += 2;
                return test_file_exists(operand);
            case 'f':
                (*pos) += 2;
                return test_file_regular(operand);
            case 'd':
                (*pos) += 2;
                return test_file_directory(operand);
            case 'r':
                (*pos) += 2;
                return test_file_readable(operand);
            case 'w':
                (*pos) += 2;
                return test_file_writable(operand);
            case 'x':
                (*pos) += 2;
                return test_file_executable(operand);
            case 's':
                (*pos) += 2;
                return test_file_nonempty(operand);
            case 'L':
            case 'h':
                (*pos) += 2;
                return test_file_symlink(operand);
            case 'b':
                (*pos) += 2;
                return test_file_block(operand);
            case 'c':
                (*pos) += 2;
                return test_file_char(operand);
            case 'p':
                (*pos) += 2;
                return test_file_pipe(operand);
            case 'S':
                (*pos) += 2;
                return test_file_socket(operand);
            case 'u':
                (*pos) += 2;
                return test_file_setuid(operand);
            case 'g':
                (*pos) += 2;
                return test_file_setgid(operand);
            case 'k':
                (*pos) += 2;
                return test_file_sticky(operand);
            case 'O':
                (*pos) += 2;
                return test_file_owned(operand);
            case 'G':
                (*pos) += 2;
                return test_file_group_owned(operand);
            case 'z':
                (*pos) += 2;
                return test_string_empty(operand);
            case 'n':
                (*pos) += 2;
                return test_string_nonempty(operand);
            case 't':
                (*pos) += 2;
                return test_terminal(operand);
            case 'v':  // [[ -v VAR ]] - variable is set
                (*pos) += 2;
                return (getenv(operand) != NULL) ? 0 : 1;
            default:
                break;
        }
    }

    // Check for binary operators with look-ahead
    if (*pos + 2 < argc) {
        const char *op = args[*pos + 1];

        // Pattern matching with == (in [[ ]], right side is pattern)
        if (strcmp(op, "==") == 0 || strcmp(op, "=") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            // In [[ ]], == does pattern matching
            return test_pattern_match(s1, s2);
        }
        if (strcmp(op, "!=") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            // In [[ ]], != does pattern matching (inverted)
            return test_pattern_match(s1, s2) ? 0 : 1;
        }

        // Regex matching with =~
        if (strcmp(op, "=~") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_regex_match(s1, s2);
        }

        // String comparison with < and >
        if (strcmp(op, "<") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_string_less_than(s1, s2);
        }
        if (strcmp(op, ">") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_string_greater_than(s1, s2);
        }

        // Integer comparison (same as [ ])
        if (strcmp(op, "-eq") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_eq(s1, s2);
        }
        if (strcmp(op, "-ne") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_ne(s1, s2);
        }
        if (strcmp(op, "-lt") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_lt(s1, s2);
        }
        if (strcmp(op, "-le") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_le(s1, s2);
        }
        if (strcmp(op, "-gt") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_gt(s1, s2);
        }
        if (strcmp(op, "-ge") == 0) {
            const char *s1 = args[*pos];
            const char *s2 = args[*pos + 2];
            (*pos) += 3;
            return test_int_ge(s1, s2);
        }

        // File comparisons
        if (strcmp(op, "-nt") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_newer(f1, f2);
        }
        if (strcmp(op, "-ot") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_older(f1, f2);
        }
        if (strcmp(op, "-ef") == 0) {
            const char *f1 = args[*pos];
            const char *f2 = args[*pos + 2];
            (*pos) += 3;
            return test_file_same(f1, f2);
        }
    }

    // Single string argument - true if non-empty
    (*pos)++;
    return test_string_nonempty(arg);
}

// NOT expression for [[ ]]: ! expr
static int eval_double_bracket_not(char **args, int *pos, int argc) {
    if (*pos < argc && strcmp(args[*pos], "!") == 0) {
        (*pos)++;
        int result = eval_double_bracket_not(args, pos, argc);
        if (result == 2) return 2;
        return result ? 0 : 1;
    }
    return eval_double_bracket_primary(args, pos, argc);
}

// AND expression for [[ ]]: expr && expr
static int eval_double_bracket_and(char **args, int *pos, int argc) {
    int result = eval_double_bracket_not(args, pos, argc);
    if (result == 2) return 2;

    while (*pos < argc && strcmp(args[*pos], "&&") == 0) {
        (*pos)++;
        int right = eval_double_bracket_not(args, pos, argc);
        if (right == 2) return 2;

        if (result != 0) {
            result = 1;
        } else {
            result = right;
        }
    }

    return result;
}

// OR expression for [[ ]]: expr || expr
static int eval_double_bracket_or(char **args, int *pos, int argc) {
    int result = eval_double_bracket_and(args, pos, argc);
    if (result == 2) return 2;

    while (*pos < argc && strcmp(args[*pos], "||") == 0) {
        (*pos)++;
        int right = eval_double_bracket_and(args, pos, argc);
        if (right == 2) return 2;

        if (result == 0) {
            result = 0;
        } else {
            result = right;
        }
    }

    return result;
}

// Top-level expression for [[ ]]
static int eval_double_bracket_expr(char **args, int *pos, int argc) {
    return eval_double_bracket_or(args, pos, argc);
}

// Evaluate [[ ]] expression
static int double_bracket_evaluate(char **args, int argc) {
    if (argc == 0) {
        return 1;  // No arguments = false
    }

    int pos = 0;
    int result = eval_double_bracket_expr(args, &pos, argc);

    if (result != 2 && pos < argc) {
        fprintf(stderr, "[[: too many arguments\n");
        return 2;
    }

    return result;
}

int builtin_double_bracket(char **args) {
    // Skip "[[" command name
    if (args[0] == NULL) {
        return 1;
    }

    // Count arguments and find closing bracket
    int argc = 0;
    while (args[argc + 1] != NULL) {
        argc++;
    }

    // Check for closing bracket
    if (argc == 0 || strcmp(args[argc], "]]") != 0) {
        fprintf(stderr, "[[: missing ']]'\n");
        return 2;
    }

    // Evaluate without the closing bracket
    return double_bracket_evaluate(args + 1, argc - 1);
}
