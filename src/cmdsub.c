#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include "cmdsub.h"
#include "safe_string.h"
#include "arith.h"
#include "script.h"
#include "trap.h"

#define MAX_CMDSUB_LENGTH 8192
#define MAX_CMD_OUTPUT 65536

// Helper to check if a character needs protection in quoted context
// This includes glob chars, redirect operators, quote chars, and other special chars
static int needs_quote_protection(char c) {
    return c == '*' || c == '?' || c == '[' ||  // glob chars
           c == '<' || c == '>' || c == '|' || c == '&' ||  // redirect/pipe operators
           c == '"' || c == '\'' || c == '\\' || c == '~';  // quotes, backslash, tilde
}

// Track exit code from last command substitution
static int last_cmdsub_exit_code = 0;

// Get the exit code from the last command substitution
int cmdsub_get_last_exit_code(void) {
    return last_cmdsub_exit_code;
}

// Reset the command substitution exit code tracker
void cmdsub_reset_exit_code(void) {
    last_cmdsub_exit_code = 0;
}

// Execute a command and capture its output
static char *execute_and_capture(const char *cmd) {
    if (!cmd || *cmd == '\0') return strdup("");

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return NULL;
    }

    // Flush stdout before forking to avoid duplicating buffered output
    fflush(stdout);

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Reset traps for subshell - inherited traps should not execute
        trap_reset_for_subshell();

        // Use hash's own script execution to preserve function definitions
        int result = script_execute_string(cmd);
        fflush(stdout);  // Ensure output is flushed before exit
        trap_execute_exit();  // Run EXIT trap before exiting subshell
        fflush(stdout);  // Flush any trap output
        _exit(result);
    }

    // Parent process
    close(pipefd[1]);

    char *output = malloc(MAX_CMD_OUTPUT);
    if (!output) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipefd[0], output + total_read,
                              MAX_CMD_OUTPUT - total_read - 1)) > 0) {
        total_read += bytes_read;
        if (total_read >= MAX_CMD_OUTPUT - 1) break;
    }

    close(pipefd[0]);
    output[total_read] = '\0';

    // Wait for child and capture exit status
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        last_cmdsub_exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        last_cmdsub_exit_code = 128 + WTERMSIG(status);
    } else {
        last_cmdsub_exit_code = 1;
    }

    // Remove trailing newlines (like bash does)
    while (total_read > 0 && output[total_read - 1] == '\n') {
        output[--total_read] = '\0';
    }

    return output;
}

// Find matching closing parenthesis, handling nesting
static const char *find_closing_paren(const char *start) {
    int depth = 1;
    const char *p = start;

    while (*p && depth > 0) {
        if (*p == '\\' && *(p + 1)) {
            p += 2;
            continue;
        }
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        if (depth > 0) p++;
    }

    return (depth == 0) ? p : NULL;
}

// Find matching closing backtick
static const char *find_closing_backtick(const char *start) {
    const char *p = start;

    while (*p) {
        if (*p == '\\' && *(p + 1) == '`') {
            p += 2;
            continue;
        }
        if (*p == '`') {
            return p;
        }
        p++;
    }

    return NULL;
}

// Check if string contains command substitution or escaped sequences that need processing
static int has_cmdsub(const char *str) {
    if (!str) return 0;

    const char *p = str;
    while (*p) {
        // Check for SOH marker with protected backslash (from single quotes: \$ or \`)
        if (*p == '\x01' && *(p + 1) == '\\' && (*(p + 2) == '$' || *(p + 2) == '`')) {
            return 1;
        }
        // Check for SOH marker (single-quoted dollar sign)
        if (*p == '\x01' && *(p + 1) == '$') {
            return 1;
        }
        if (*p == '\\' && *(p + 1)) {
            if (*(p + 1) == '$' || *(p + 1) == '`') {
                return 1;
            }
            p += 2;
            continue;
        }
        // Check for $( but NOT $(( which is arithmetic
        if (*p == '$' && *(p + 1) == '(') {
            if (*(p + 2) != '(') {
                return 1;  // This is command substitution $()
            }
            // Skip past $(( - it's arithmetic, not command substitution
            p += 3;
            continue;
        }
        if (*p == '`') return 1;
        p++;
    }
    return 0;
}

// Extract command, execute it, and append output to result buffer
// When in_quoted is true, protect glob characters with \x01 markers
// Returns new position in result buffer, or -1 on allocation failure
static ssize_t process_substitution(const char *cmd_start, size_t cmd_len,
                                    char *result, size_t out_pos, int in_quoted) {
    char *cmd = malloc(cmd_len + 1);
    if (!cmd) {
        return -1;
    }
    memcpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';

    char *output = execute_and_capture(cmd);
    free(cmd);

    if (output) {
        size_t output_len = strlen(output);
        // Copy output to result
        // - If in_quoted: protect special chars with \x01 markers
        // - If not in_quoted: wrap with \x03 markers for IFS splitting
        if (!in_quoted && out_pos < MAX_CMDSUB_LENGTH - 2) {
            result[out_pos++] = '\x03';  // Start IFS split marker
        }
        for (size_t i = 0; i < output_len && out_pos < MAX_CMDSUB_LENGTH - 2; i++) {
            char c = output[i];
            if (in_quoted && needs_quote_protection(c)) {
                // Protect special character with \x01 marker
                result[out_pos++] = '\x01';
            }
            result[out_pos++] = c;
        }
        if (!in_quoted && out_pos < MAX_CMDSUB_LENGTH - 1) {
            result[out_pos++] = '\x03';  // End IFS split marker
        }
        free(output);
    } else if (!in_quoted && out_pos < MAX_CMDSUB_LENGTH - 2) {
        // Empty output in unquoted context - add markers so it can be removed
        result[out_pos++] = '\x03';
        result[out_pos++] = '\x03';
    }

    return (ssize_t)out_pos;
}

// Expand command substitutions in a string
char *cmdsub_expand(const char *str) {
    if (!str || !has_cmdsub(str)) return NULL;

    char *result = malloc(MAX_CMDSUB_LENGTH);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;
    int in_dquote = 0;   // Track double-quote context for backticks
    int in_squote = 0;   // Track single-quote context

    while (*p && out_pos < MAX_CMDSUB_LENGTH - 1) {
        // Track quote state for backticks (which don't get \x02 markers)
        if (*p == '"' && !in_squote) {
            in_dquote = !in_dquote;
            result[out_pos++] = *p++;
            continue;
        }
        if (*p == '\'' && !in_dquote) {
            in_squote = !in_squote;
            result[out_pos++] = *p++;
            continue;
        }
        // Handle SOH marker with protected backslash (from single quotes: \$ or \`)
        // Pass through to varexpand which will output \$ or \` literally
        if (*p == '\x01' && *(p + 1) == '\\' && (*(p + 2) == '$' || *(p + 2) == '`')) {
            if (out_pos < MAX_CMDSUB_LENGTH - 3) {
                result[out_pos++] = *p++;  // marker
                result[out_pos++] = *p++;  // backslash
                result[out_pos++] = *p++;  // $ or `
            } else {
                p += 3;
            }
            continue;
        }
        // Handle SOH marker (single-quoted dollar sign from parser)
        // Pass through to varexpand which will output literal $
        if (*p == '\x01' && *(p + 1) == '$') {
            if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                result[out_pos++] = *p++;  // marker
                result[out_pos++] = *p++;  // $
            } else {
                p += 2;
            }
            continue;
        }
        // Handle escape sequences
        if (*p == '\\' && *(p + 1)) {
            if (*(p + 1) == '$' || *(p + 1) == '`') {
                // Escaped $ or ` - preserve the escape for varexpand to handle
                if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                    result[out_pos++] = '\\';
                    result[out_pos++] = *(p + 1);
                }
                p += 2;
                continue;
            }
            if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                result[out_pos++] = *p++;
                result[out_pos++] = *p++;
            } else {
                break;
            }
            continue;
        }

        // Skip $(( arithmetic - let arith_expand handle it later
        if (*p == '$' && *(p + 1) == '(' && *(p + 2) == '(') {
            // Copy $(( literally - arithmetic expansion handles this
            result[out_pos++] = *p++;
            if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                result[out_pos++] = *p++;
            }
            if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                result[out_pos++] = *p++;
            }
            // Copy until matching ))
            int depth = 1;
            while (*p && depth > 0 && out_pos < MAX_CMDSUB_LENGTH - 1) {
                if (*p == '(' && *(p + 1) == '(') {
                    depth++;
                    result[out_pos++] = *p++;
                    if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                        result[out_pos++] = *p++;
                    }
                } else if (*p == ')' && *(p + 1) == ')') {
                    depth--;
                    result[out_pos++] = *p++;
                    if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                        result[out_pos++] = *p++;
                    }
                } else {
                    result[out_pos++] = *p++;
                }
            }
            continue;
        }

        // Handle \x02 marker (indicates $ is in double-quoted context)
        // Consume the marker and set flag for next $ expansion
        if (*p == '\x02' && *(p + 1) == '$') {
            p++;  // Skip the marker, process $ next iteration with quoted flag
            // Check if it's command substitution
            if (*(p + 1) == '(') {
                p += 2;  // Skip $(
                const char *end = find_closing_paren(p);
                if (!end) {
                    if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                        result[out_pos++] = '$';
                        result[out_pos++] = '(';
                    }
                    continue;
                }
                // Process with in_quoted=1 to protect glob chars
                ssize_t new_pos = process_substitution(p, end - p, result, out_pos, 1);
                if (new_pos < 0) {
                    free(result);
                    return NULL;
                }
                out_pos = (size_t)new_pos;
                p = end + 1;
                continue;
            }
            // Not command substitution, pass marker through for varexpand
            result[out_pos++] = '\x02';
            continue;
        }

        // Handle $(...) syntax (command substitution) - unquoted context
        if (*p == '$' && *(p + 1) == '(') {
            p += 2;

            const char *end = find_closing_paren(p);
            if (!end) {
                if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                    result[out_pos++] = '$';
                    result[out_pos++] = '(';
                }
                continue;
            }

            ssize_t new_pos = process_substitution(p, end - p, result, out_pos, 0);
            if (new_pos < 0) {
                free(result);
                return NULL;
            }
            out_pos = (size_t)new_pos;
            p = end + 1;
            continue;
        }

        // Handle `...` syntax (backticks)
        // Backticks in single quotes are literal, not command substitution
        // Backticks in double quotes should protect glob chars in output
        if (*p == '`' && !in_squote) {
            p++;

            const char *end = find_closing_backtick(p);
            if (!end) {
                if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                    result[out_pos++] = '`';
                }
                continue;
            }

            ssize_t new_pos = process_substitution(p, end - p, result, out_pos, in_dquote);
            if (new_pos < 0) {
                free(result);
                return NULL;
            }
            out_pos = (size_t)new_pos;
            p = end + 1;
            continue;
        }

        // Regular character
        result[out_pos++] = *p++;
    }

    result[out_pos] = '\0';
    return result;
}

// Expand command substitutions in all arguments
int cmdsub_args(char **args) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (has_cmdsub(args[i])) {
            char *expanded = cmdsub_expand(args[i]);
            if (expanded) {
                args[i] = expanded;
            }
        }
    }

    return 0;
}
