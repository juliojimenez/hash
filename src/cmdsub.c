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
#include "ast_parser.h"
#include "ast.h"
#include "arena.h"
#include "safe_string.h"
#include "arith.h"
#include "script.h"
#include "trap.h"
#include "hash.h"
#include "utils.h"

#define INITIAL_BUF_SIZE 65536

// Dynamic buffer for building command substitution results
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} CmdSubBuf;

static int cmdsub_buf_ensure(CmdSubBuf *buf, size_t additional) {
    size_t needed = buf->len + additional;
    if (needed < buf->cap) {
        return 0;
    }
    size_t new_cap = buf->cap;
    while (new_cap <= needed) {
        new_cap *= 2;
    }
    char *new_data = realloc(buf->data, new_cap);
    if (!new_data) {
        return -1;
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

static void cmdsub_buf_putc(CmdSubBuf *buf, char c) {
    if (cmdsub_buf_ensure(buf, 1) == 0) {
        buf->data[buf->len++] = c;
    }
}

// Helper to check if a character needs protection in quoted context
// This includes glob chars, redirect operators, quote chars, and other special chars
static int needs_quote_protection(char c) {
    return char_in_string(c,
            "*?[" // glob chars
            "<>|&" // redirect/pipe operators
            "\"'\\~" // quotes, backslash, tilde
            );
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

// Is the program one simple command (one list item, one pipeline, one command)?
static bool cmdsub_is_single_simple(const char *cmd) {
    Arena arena;
    arena_init(&arena, 0);
    AstParseResult parsed = parse_program(cmd, &arena);
    bool single = false;
    if (parsed.status == PARSE_OK && parsed.root && parsed.root->type == NODE_LIST &&
        parsed.root->u.list.items.count == 1) {
        const Node *item = parsed.root->u.list.items.items[0].node;
        single = !parsed.root->u.list.items.items[0].async &&
                 item->type == NODE_PIPELINE && item->u.pipeline.commands.count == 1 &&
                 !item->u.pipeline.negated && item->u.pipeline.commands.items[0]->type == NODE_SIMPLE;
    }
    arena_free(&arena);
    return single;
}

static void child_process(const char *cmd, const int pipefd[2]) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    // Disable interactive mode in subshell to prevent job control issues
    is_interactive = false;

    // Reset traps for subshell - inherited traps should not execute
    trap_reset_for_subshell();
    // POSIX: break/continue only affect loops in this subshell
    script_reset_for_subshell();

    // Let execute() exec an external command directly instead of fork+exec,
    // so $PPID is right for it, but only when the captured program is that one
    // simple command: an exec in the middle of `$(/bin/echo a; echo b)` would
    // replace this subshell and drop everything after it (it did until #355).
    exec_directly_in_child = cmdsub_is_single_simple(cmd);

    // Use hash's own script execution to preserve function definitions
    int result = script_execute_string(cmd);
    fflush(stdout);  // Ensure output is flushed before exit
    result = trap_exit_status(result);  // EXIT trap runs before the subshell ends
    fflush(stdout);  // Flush any trap output
    _exit(result & 0xFF);
}

static char *get_child_output(pid_t pid, const int pipefd[2]) {
    close(pipefd[1]);

    size_t buf_size = INITIAL_BUF_SIZE;
    char *output = malloc(buf_size);
    if (!output) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipefd[0], output + total_read,
                              buf_size - total_read - 1)) > 0) {
        total_read += bytes_read;
        if (total_read >= buf_size - 1) {
            size_t new_size = buf_size * 2;
            char *new_output = realloc(output, new_size);
            if (!new_output) {
                break;
            }
            output = new_output;
            buf_size = new_size;
        }
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
        // child_process calls _exit()
        child_process(cmd, pipefd);
    }

    return get_child_output(pid, pipefd);
}

// Find matching closing parenthesis, handling nesting
// The closing parenthesis of a $( ... ) whose body starts at `start`. Parentheses
// inside quotes or comments do not count, as in the tokenizer (skip_cmdsub_body).
static const char *find_closing_paren(const char *start) {
    int depth = 1;
    const char *p = start;
    bool word_start = true;

    while (*p && depth > 0) {
        char c = *p;
        if (c == '#' && word_start) {
            while (*p && *p != '\n') p++;
            continue;
        }
        word_start = (c == ' ' || c == '\t' || c == '\n' || c == '(' || c == ')' ||
                      c == ';' || c == '|' || c == '&');
        if (c == '\\' && *(p + 1)) {
            p += 2;
            continue;
        }
        if (c == '\'') {
            p++;
            while (*p && *p != '\'') p++;
            if (*p) p++;
            continue;
        }
        if (c == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1)) p++;
                p++;
            }
            if (*p) p++;
            continue;
        }
        if (c == '(') depth++;
        else if (c == ')') depth--;
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
        if (*p == '\x01' && *(p + 1) == '\\' && char_in_string(*(p + 2), "$`")) {
            return 1;
        }
        // Check for SOH marker (single-quoted dollar sign)
        if (*p == '\x01' && *(p + 1) == '$') {
            return 1;
        }
        if (*p == '\\' && *(p + 1)) {
            if (char_in_string(*(p + 1), "$`")) {
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
// Returns 0 on success, -1 on allocation failure
static int process_substitution(const char *cmd_start, size_t cmd_len,
                                CmdSubBuf *buf, int in_quoted) {
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
        // Pre-allocate space: worst case is 2x output (every char gets a marker)
        if (cmdsub_buf_ensure(buf, output_len * 2 + 4) < 0) {
            free(output);
            return -1;
        }
        // Copy output to result
        // - If in_quoted: protect special chars with \x01 markers
        // - If not in_quoted: wrap with \x03 markers for IFS splitting
        if (!in_quoted) {
            buf->data[buf->len++] = '\x03';  // Start IFS split marker
        }
        for (size_t i = 0; i < output_len; i++) {
            char c = output[i];
            if (char_in_string(c, "$`") || is_marker_byte(c)) {
                // Always protect $ and ` from further expansion
                // (POSIX: command substitution output is not re-scanned
                // for variable expansion or nested command substitution),
                // and a real marker-range byte, which travels as \x01 + byte
                buf->data[buf->len++] = '\x01';
            } else if (in_quoted && needs_quote_protection(c)) {
                // Protect special character with \x01 marker
                buf->data[buf->len++] = '\x01';
            }
            buf->data[buf->len++] = c;
        }
        if (!in_quoted) {
            buf->data[buf->len++] = '\x03';  // End IFS split marker
        }
        free(output);
    } else if (!in_quoted) {
        // Empty output in unquoted context - add markers so it can be removed
        if (cmdsub_buf_ensure(buf, 2) < 0) {
            return -1;
        }
        buf->data[buf->len++] = '\x03';
        buf->data[buf->len++] = '\x03';
    }

    return 0;
}

// Returns true if handled
static bool handle_soh_marker(const char **read_char, CmdSubBuf *buf) {
    const char *p = *read_char;

    // A marked backslash (\x01 \\) is a literal byte that escapes nothing: it
    // is copied as a pair below, and a ` or $( after it is a command
    // substitution like any other ('a\'`echo q` and \\`echo q` give a\q and
    // \q; the three used to be copied together and the command left as text)

    // Handle SOH marker (single-quoted dollar sign from parser)
    // Pass through to varexpand which will output literal $
    if (*p == '\x01' && *(p + 1) == '$') {
        if (cmdsub_buf_ensure(buf, 2) == 0) {
            buf->data[buf->len++] = *p++;  // marker
            buf->data[buf->len++] = *p++;  // $
        } else {
            p += 2;
        }
        goto handled;
    }
    // Any other protected byte: keep the pair whole, so the byte after the
    // marker is not read as a marker or an expansion start
    if (*p == '\x01' && *(p + 1)) {
        if (cmdsub_buf_ensure(buf, 2) == 0) {
            buf->data[buf->len++] = *p++;
            buf->data[buf->len++] = *p++;
        } else {
            p += 2;
        }
        goto handled;
    }

    return false;

handled:
    *read_char = p;
    return true;
}

// Returns true if handled
static bool handle_arith_expr(const char **read_char, CmdSubBuf *buf) {
    const char *p = *read_char;

    if (*p == '$' && *(p + 1) == '(' && *(p + 2) == '(') {
        // Copy $(( literally - arithmetic expansion handles this
        cmdsub_buf_putc(buf, *p++);
        cmdsub_buf_putc(buf, *p++);
        cmdsub_buf_putc(buf, *p++);
        // Copy until matching ))
        int depth = 1;
        while (*p && depth > 0) {
            if (*p == '(' && *(p + 1) == '(') {
                depth++;
                cmdsub_buf_putc(buf, *p++);
                cmdsub_buf_putc(buf, *p++);
            } else if (*p == ')' && *(p + 1) == ')') {
                depth--;
                cmdsub_buf_putc(buf, *p++);
                cmdsub_buf_putc(buf, *p++);
            } else {
                cmdsub_buf_putc(buf, *p++);
            }
        }
        goto handled;
    }

    return false;

handled:
    *read_char = p;
    return true;
}

// Expand command substitutions in a string
char *cmdsub_expand(const char *str) {
    if (!str || !has_cmdsub(str)) return NULL;

    size_t str_len = strlen(str);
    size_t initial_cap = str_len * 2 + 256;
    if (initial_cap < INITIAL_BUF_SIZE) {
        initial_cap = INITIAL_BUF_SIZE;
    }

    CmdSubBuf buf;
    buf.data = malloc(initial_cap);
    if (!buf.data) return NULL;
    buf.len = 0;
    buf.cap = initial_cap;

    const char *p = str;
    int in_dquote = 0;   // Track double-quote context for backticks
    int in_squote = 0;   // Track single-quote context

    while (*p) {
        // Track quote state for backticks (which don't get \x02 markers)
        if (*p == '"' && !in_squote) {
            in_dquote = !in_dquote;
            cmdsub_buf_putc(&buf, *p++);
            continue;
        }
        if (*p == '\'' && !in_dquote) {
            in_squote = !in_squote;
            cmdsub_buf_putc(&buf, *p++);
            continue;
        }

        if (handle_soh_marker(&p, &buf)) {
            continue;
        }

        // Handle escape sequences
        if (*p == '\\' && *(p + 1)) {
            if (char_in_string(*(p + 1), "$`")) {
                // Escaped $ or ` - preserve the escape for varexpand to handle
                if (cmdsub_buf_ensure(&buf, 2) == 0) {
                    buf.data[buf.len++] = '\\';
                    buf.data[buf.len++] = *(p + 1);
                }
                p += 2;
                continue;
            }
            if (cmdsub_buf_ensure(&buf, 2) == 0) {
                buf.data[buf.len++] = *p++;
                buf.data[buf.len++] = *p++;
            } else {
                break;
            }
            continue;
        }

        // Skip $(( arithmetic - let arith_expand handle it later
        if (handle_arith_expr(&p, &buf)) {
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
                    if (cmdsub_buf_ensure(&buf, 2) == 0) {
                        buf.data[buf.len++] = '$';
                        buf.data[buf.len++] = '(';
                    }
                    continue;
                }
                // Process with in_quoted=1 to protect glob chars
                if (process_substitution(p, end - p, &buf, 1) < 0) {
                    free(buf.data);
                    return NULL;
                }
                p = end + 1;
                continue;
            }
            // Not command substitution, pass marker through for varexpand
            cmdsub_buf_putc(&buf, '\x02');
            continue;
        }

        // Handle $(...) syntax (command substitution) - unquoted context
        if (*p == '$' && *(p + 1) == '(') {
            p += 2;

            const char *end = find_closing_paren(p);
            if (!end) {
                if (cmdsub_buf_ensure(&buf, 2) == 0) {
                    buf.data[buf.len++] = '$';
                    buf.data[buf.len++] = '(';
                }
                continue;
            }

            if (process_substitution(p, end - p, &buf, 0) < 0) {
                free(buf.data);
                return NULL;
            }
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
                cmdsub_buf_putc(&buf, '`');
                continue;
            }

            if (process_substitution(p, end - p, &buf, in_dquote) < 0) {
                free(buf.data);
                return NULL;
            }
            p = end + 1;
            continue;
        }

        // Regular character
        cmdsub_buf_putc(&buf, *p++);
    }

    // Null-terminate
    if (cmdsub_buf_ensure(&buf, 1) < 0) {
        free(buf.data);
        return NULL;
    }
    buf.data[buf.len] = '\0';
    return buf.data;
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
