#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include "cmdsub.h"
#include "safe_string.h"

#define MAX_CMDSUB_LENGTH 8192
#define MAX_CMD_OUTPUT 65536

// Execute a command and capture its output
static char *execute_and_capture(const char *cmd) {
    if (!cmd || *cmd == '\0') return strdup("");

    // Create pipe for capturing output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close read end

        // Redirect stdout to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Execute command using /bin/sh -c
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        // If exec fails
        _exit(127);
    }

    // Parent process
    close(pipefd[1]);  // Close write end

    // Read output from child
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

    // Wait for child to finish
    waitpid(pid, NULL, 0);

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
            p += 2;  // Skip escaped characters
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
            p += 2;  // Skip escaped backtick
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
        if (*p == '\\' && *(p + 1)) {
            // Escaped $ or ` needs processing (to remove the backslash)
            if (*(p + 1) == '$' || *(p + 1) == '`') {
                return 1;
            }
            p += 2;
            continue;
        }
        if (*p == '$' && *(p + 1) == '(') return 1;
        if (*p == '`') return 1;
        p++;
    }
    return 0;
}

// Expand command substitutions in a string
char *cmdsub_expand(const char *str) {
    if (!str || !has_cmdsub(str)) return NULL;

    char *result = malloc(MAX_CMDSUB_LENGTH);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    while (*p && out_pos < MAX_CMDSUB_LENGTH - 1) {
        // Handle escape sequences
        if (*p == '\\' && *(p + 1)) {
            if (*(p + 1) == '$' || *(p + 1) == '`') {
                // Escaped $ or ` - keep the character (remove backslash)
                if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                    result[out_pos++] = *(p + 1);
                }
                p += 2;
                continue;
            }
            // Other escapes - keep both characters
            if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                result[out_pos++] = *p++;
                result[out_pos++] = *p++;
            } else {
                break;
            }
            continue;
        }

        // Handle $(...) syntax
        if (*p == '$' && *(p + 1) == '(') {
            p += 2;  // Skip $(

            const char *end = find_closing_paren(p);
            if (!end) {
                // No closing paren - copy literally
                if (out_pos < MAX_CMDSUB_LENGTH - 2) {
                    result[out_pos++] = '$';
                    result[out_pos++] = '(';
                }
                continue;
            }

            // Extract command
            size_t cmd_len = end - p;
            char *cmd = malloc(cmd_len + 1);
            if (!cmd) {
                free(result);
                return NULL;
            }
            memcpy(cmd, p, cmd_len);
            cmd[cmd_len] = '\0';

            // Execute and capture output
            char *output = execute_and_capture(cmd);
            free(cmd);

            if (output) {
                size_t output_len = strlen(output);
                size_t space = MAX_CMDSUB_LENGTH - 1 - out_pos;
                size_t to_copy = (output_len < space) ? output_len : space;
                memcpy(result + out_pos, output, to_copy);
                out_pos += to_copy;
                free(output);
            }

            p = end + 1;  // Skip past closing )
            continue;
        }

        // Handle `...` syntax (backticks)
        if (*p == '`') {
            p++;  // Skip opening backtick

            const char *end = find_closing_backtick(p);
            if (!end) {
                // No closing backtick - copy literally
                if (out_pos < MAX_CMDSUB_LENGTH - 1) {
                    result[out_pos++] = '`';
                }
                continue;
            }

            // Extract command
            size_t cmd_len = end - p;
            char *cmd = malloc(cmd_len + 1);
            if (!cmd) {
                free(result);
                return NULL;
            }
            memcpy(cmd, p, cmd_len);
            cmd[cmd_len] = '\0';

            // Execute and capture output
            char *output = execute_and_capture(cmd);
            free(cmd);

            if (output) {
                size_t output_len = strlen(output);
                size_t space = MAX_CMDSUB_LENGTH - 1 - out_pos;
                size_t to_copy = (output_len < space) ? output_len : space;
                memcpy(result + out_pos, output, to_copy);
                out_pos += to_copy;
                free(output);
            }

            p = end + 1;  // Skip past closing `
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
        // Check if this argument contains command substitution
        if (has_cmdsub(args[i])) {
            char *expanded = cmdsub_expand(args[i]);
            if (expanded) {
                // Replace argument with expanded version
                args[i] = expanded;
            }
        }
    }

    return 0;
}
