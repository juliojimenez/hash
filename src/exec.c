#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exec.h"
#include "arena.h"
#include "arith.h"
#include "ast_parser.h"
#include "cmdsub.h"
#include "config.h"
#include "execute.h"
#include "expand.h"
#include "hash.h"
#include "ifs.h"
#include "jobs.h"
#include "parser.h"
#include "script.h"
#include "shellvar.h"
#include "trap.h"
#include "varexpand.h"

#define EXEC_MAX_REDIRS 32

// ============================================================================
// Status mapping
// ============================================================================

// execute() returns 1 to keep going, 0 to stop, -2/-3/-4 for return/break/continue
static ExecStatus status_from_execute(int rc) {
    switch (rc) {
        case -2:
            return EXEC_RETURN;
        case -3:
            return EXEC_BREAK;
        case -4:
            return EXEC_CONTINUE;
        case 0:
            if (script_state.exit_requested) {
                return EXEC_EXIT;
            }
            if (script_get_break_pending() > 0) {
                return EXEC_BREAK;
            }
            if (script_get_continue_pending() > 0) {
                return EXEC_CONTINUE;
            }
            return EXEC_EXIT;
        default:
            return EXEC_OK;
    }
}

// ============================================================================
// Word expansion
// ============================================================================

static char *join_words(char **items, size_t count) {
    size_t len = 1;
    for (size_t i = 0; i < count; i++) {
        len += strlen(items[i]) + 1;
    }
    char *line = malloc(len);
    if (!line) {
        return NULL;
    }
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            line[pos++] = ' ';
        }
        size_t n = strlen(items[i]);
        memcpy(line + pos, items[i], n);
        pos += n;
    }
    line[pos] = '\0';
    return line;
}

static void free_string_list(char **list, int count) {
    if (!list) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

static char **copy_pointer_array(char **src, int count) {
    char **copy = malloc(((size_t)count + 1) * sizeof(char *));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, (size_t)count * sizeof(char *));
    copy[count] = NULL;
    return copy;
}

// Remove the internal IFS-split markers left by expansion when no splitting
// takes place (redirection targets, here-documents)
static void strip_ifs_markers(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r != '\x03') {
            *w++ = *r;
        }
    }
    *w = '\0';
}

// Expand raw words the way command arguments are expanded: tilde, command
// substitution, arithmetic, parameters, then optionally field splitting and
// pathname expansion, then quote removal. Returns a malloc'd NULL-terminated
// array of malloc'd strings and sets *count_out; NULL when out of memory.
static char **expand_words(char **raw, size_t raw_count, bool split, bool glob, int *count_out) {
    *count_out = 0;
    char *line = join_words(raw, raw_count);
    if (!line) {
        return NULL;
    }
    ParseResult parsed = parse_line(line);
    free(line);
    if (!parsed.tokens) {
        return calloc(1, sizeof(char *));
    }

    char **args = parsed.tokens;
    int count = 0;
    while (args[count]) {
        count++;
    }
    char **originals = copy_pointer_array(args, count);
    if (!originals) {
        parse_result_free(&parsed);
        return NULL;
    }

    expand_tilde(args);
    cmdsub_args(args);
    arith_args(args);
    varexpand_args(args, last_command_exit_code);

    char **current = args;
    int current_count = count;
    bool current_owned = false;

    if (split) {
        char **copy = copy_pointer_array(current, current_count);
        if (copy) {
            char **before = copy;
            int n = current_count;
            if (ifs_split_args(&copy, &n) == 0 && copy != before) {
                free(before);
                current = copy;
                current_count = n;
                current_owned = true;
            } else {
                free(copy);
            }
        }
    }

    if (glob) {
        bool has_globs = false;
        for (int i = 0; i < current_count; i++) {
            if (has_glob_chars(current[i])) {
                has_globs = true;
                break;
            }
        }
        if (has_globs) {
            char **copy = copy_pointer_array(current, current_count);
            if (copy) {
                char **before = copy;
                int n = current_count;
                if (expand_glob(&copy, &n) == 0 && copy != before) {
                    free(before);
                    if (current_owned) {
                        free_string_list(current, current_count);
                    }
                    current = copy;
                    current_count = n;
                    current_owned = true;
                } else {
                    free(copy);
                }
            }
        }
    }

    char **result = malloc(((size_t)current_count + 1) * sizeof(char *));
    if (result) {
        for (int i = 0; i < current_count; i++) {
            result[i] = strdup(current[i]);
            if (result[i]) {
                strip_quote_markers(result[i]);
                if (!split) {
                    strip_ifs_markers(result[i]);
                }
            }
        }
        result[current_count] = NULL;
        *count_out = current_count;
    }

    if (current_owned) {
        free_string_list(current, current_count);
    }
    for (int i = 0; i < count; i++) {
        if (args[i] != originals[i]) {
            free(args[i]);
        }
    }
    free(originals);
    parse_result_free(&parsed);
    return result;
}

// ============================================================================
// Redirections (applied to the current process, then undone)
// ============================================================================

typedef struct {
    int fd;
    int saved;      // Duplicate of the previous descriptor, -1 if it was closed
} SavedFd;

// Set when expanding a redirection target or here-document failed (for
// example ${x?msg} on an unset x): fatal for a non-interactive shell
static bool redirect_expansion_failed = false;

static void restore_redirs(SavedFd *saved, int count) {
    fflush(stdout);
    fflush(stderr);
    for (int i = count - 1; i >= 0; i--) {
        if (saved[i].saved >= 0) {
            dup2(saved[i].saved, saved[i].fd);
            close(saved[i].saved);
        } else {
            close(saved[i].fd);
        }
    }
}

// A redirection target expands to exactly one word
static char *expand_target(const char *raw) {
    char *words[1];
    words[0] = (char *)raw;
    int count = 0;
    varexpand_clear_error();
    char **expanded = expand_words(words, 1, false, false, &count);
    if (varexpand_had_error()) {
        redirect_expansion_failed = true;
        if (expanded) {
            free_string_list(expanded, count);
        }
        return NULL;
    }
    if (!expanded) {
        return NULL;
    }
    char *result = NULL;
    if (count == 1) {
        result = expanded[0];
        expanded[0] = NULL;
    } else {
        fprintf(stderr, "%s: %s: ambiguous redirect\n", HASH_NAME, raw);
    }
    free_string_list(expanded, count);
    return result;
}

// In an unquoted here-document a backslash before a newline joins the lines;
// a doubled backslash stays for the expander to reduce
static char *join_heredoc_continuations(const char *body) {
    char *out = malloc(strlen(body) + 1);
    if (!out) {
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; body[i]; i++) {
        if (body[i] == '\\' && body[i + 1] == '\\') {
            out[o++] = body[i];
            out[o++] = body[i + 1];
            i++;
        } else if (body[i] == '\\' && body[i + 1] == '\n') {
            i++;
        } else {
            out[o++] = body[i];
        }
    }
    out[o] = '\0';
    return out;
}

// Here-document content as a readable descriptor (expanded unless quoted)
static int heredoc_fd(const Redirect *r) {
    const char *content = r->heredoc_body ? r->heredoc_body : "";
    char *expanded = NULL;
    char *joined = NULL;
    if (!r->heredoc_quoted) {
        joined = join_heredoc_continuations(content);
        const char *source = joined ? joined : content;
        char *after_cmdsub = cmdsub_expand(source);
        char *after_arith = arith_expand(after_cmdsub ? after_cmdsub : source);
        varexpand_clear_error();
        expanded = varexpand_expand(after_arith ? after_arith : (after_cmdsub ? after_cmdsub : source),
                                    last_command_exit_code);
        free(after_arith);
        free(after_cmdsub);
        if (varexpand_had_error()) {
            redirect_expansion_failed = true;
            free(expanded);
            free(joined);
            return -1;
        }
        if (expanded) {
            strip_ifs_markers(expanded);
            strip_quote_markers(expanded);
            content = expanded;
        }
    }
    int fd = -1;
    FILE *tmp = tmpfile();
    if (tmp) {
        fputs(content, tmp);
        fflush(tmp);
        rewind(tmp);
        fd = dup(fileno(tmp));
        fclose(tmp);
    }
    if (fd < 0) {
        fprintf(stderr, "%s: cannot create here-document: %s\n", HASH_NAME, strerror(errno));
    }
    free(expanded);
    free(joined);
    return fd;
}

// Status after a failed redirection: an expansion error ends a
// non-interactive shell, anything else just fails the command
static ExecStatus redirect_failure_status(bool special_builtin) {
    last_command_exit_code = 1;
    bool fatal = redirect_expansion_failed || special_builtin;
    redirect_expansion_failed = false;
    return (fatal && !is_interactive) ? EXEC_EXIT : EXEC_OK;
}

static bool all_digits(const char *s) {
    if (!*s) {
        return false;
    }
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) {
            return false;
        }
    }
    return true;
}

// Descriptor to install at r->fd; -1 on error (reported). Sets *close_only
// for the N>&- and N<&- forms.
static int open_redirect_source(const Redirect *r, bool *close_only) {
    *close_only = false;
    if (r->op == TOKEN_DLESS || r->op == TOKEN_DLESSDASH) {
        return heredoc_fd(r);
    }
    char *target = expand_target(r->target);
    if (!target) {
        return -1;
    }
    int fd = -1;
    switch (r->op) {
        case TOKEN_LESS:
            fd = open(target, O_RDONLY);
            break;
        case TOKEN_GREAT:
            fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            break;
        case TOKEN_DGREAT:
            fd = open(target, O_WRONLY | O_CREAT | O_APPEND, 0644);
            break;
        case TOKEN_LESSGREAT:
            fd = open(target, O_RDWR | O_CREAT, 0644);
            break;
        case TOKEN_LESSAND:
        case TOKEN_GREATAND:
            if (strcmp(target, "-") == 0) {
                *close_only = true;
            } else if (all_digits(target)) {
                fd = dup(atoi(target));
                if (fd < 0) {
                    fprintf(stderr, "%s: %s: %s\n", HASH_NAME, target, strerror(EBADF));
                }
            } else {
                fprintf(stderr, "%s: %s: ambiguous redirect\n", HASH_NAME, target);
            }
            free(target);
            return fd;
        default:
            break;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s: %s\n", HASH_NAME, target, strerror(errno));
    }
    free(target);
    return fd;
}

// Apply a node's redirections to this process, remembering what to restore.
// On failure the error has been printed and the caller restores and skips.
static bool apply_redirs(const RedirVec *redirs, SavedFd *saved, int *saved_count) {
    *saved_count = 0;
    redirect_expansion_failed = false;
    fflush(stdout);
    fflush(stderr);
    for (size_t i = 0; i < redirs->count && *saved_count < EXEC_MAX_REDIRS; i++) {
        const Redirect *r = &redirs->items[i];
        bool close_only = false;
        int src = open_redirect_source(r, &close_only);
        if (src < 0 && !close_only) {
            return false;
        }
        SavedFd *slot = &saved[*saved_count];
        slot->fd = r->fd;
        slot->saved = fcntl(r->fd, F_DUPFD_CLOEXEC, 10);
        (*saved_count)++;
        if (close_only) {
            close(r->fd);
        } else if (src != r->fd) {
            dup2(src, r->fd);
            close(src);
        }
    }
    return true;
}

// Run a compound command's core with its redirections in place
static ExecStatus with_redirs(const Node *node, ExecStatus (*core)(const Node *)) {
    SavedFd saved[EXEC_MAX_REDIRS];
    int count = 0;
    if (node->redirs.count > 0 && !apply_redirs(&node->redirs, saved, &count)) {
        restore_redirs(saved, count);
        return redirect_failure_status(false);
    }
    ExecStatus st = core(node);
    restore_redirs(saved, count);
    return st;
}

// ============================================================================
// Simple commands
// ============================================================================

// Redirection as a token for the one-command executor (used for `exec`,
// whose redirections must outlive the command)
static char *redirect_token(const Redirect *r) {
    char buf[32];
    const char *op = ast_redirect_op_name(r->op);
    bool input = (r->op == TOKEN_LESS || r->op == TOKEN_DLESS || r->op == TOKEN_DLESSDASH ||
                  r->op == TOKEN_LESSAND || r->op == TOKEN_LESSGREAT);
    int default_fd = input ? 0 : 1;
    if (r->fd == default_fd) {
        snprintf(buf, sizeof(buf), "%s", op);
    } else {
        snprintf(buf, sizeof(buf), "%d%s", r->fd, op);
    }
    size_t len = strlen(buf) + strlen(r->target) + 2;
    char *token = malloc(len);
    if (!token) {
        return NULL;
    }
    if (r->op == TOKEN_LESSAND || r->op == TOKEN_GREATAND) {
        snprintf(token, len, "%s%s", buf, r->target);     // 2>&1 is one token
    } else {
        snprintf(token, len, "%s %s", buf, r->target);
    }
    return token;
}

// POSIX special builtins: a redirection error on one of these is fatal to a
// non-interactive shell, and `exec` with only redirections keeps them
static bool is_special_builtin_name(const char *name) {
    static const char *const names[] = {":", ".", "break", "continue", "eval", "exec", "exit", "export",
                                        "readonly", "return", "set", "shift", "times", "trap", "unset", NULL};
    for (int i = 0; names[i]; i++) {
        if (strcmp(name, names[i]) == 0) {
            return true;
        }
    }
    return false;
}

// `exec` (also via `command exec`) with no command word: its redirections
// apply to the shell itself and must stay in place
static bool is_bare_exec(const WordVec *words) {
    if (words->count == 1 && strcmp(words->items[0], "exec") == 0) {
        return true;
    }
    return words->count == 2 && strcmp(words->items[0], "command") == 0 &&
           strcmp(words->items[1], "exec") == 0;
}

// Keep applied redirections: just drop the saved copies of the old descriptors
static void forget_saved_redirs(SavedFd *saved, int count) {
    for (int i = 0; i < count; i++) {
        if (saved[i].saved >= 0) {
            close(saved[i].saved);
        }
    }
}

static ExecStatus exec_simple(const Node *n) {
    const WordVec *assignments = &n->u.simple.assignments;
    const WordVec *words = &n->u.simple.words;
    bool permanent = is_bare_exec(words);
    bool is_exec_with_command = !permanent && words->count > 1 && strcmp(words->items[0], "exec") == 0;

    SavedFd saved[EXEC_MAX_REDIRS];
    int saved_count = 0;
    if (!is_exec_with_command && n->redirs.count > 0) {
        if (!apply_redirs(&n->redirs, saved, &saved_count)) {
            restore_redirs(saved, saved_count);
            bool special = words->count > 0 && is_special_builtin_name(words->items[0]);
            return redirect_failure_status(special);
        }
    }

    // Rebuild the command text for the one-command executor
    size_t redir_tokens = is_exec_with_command ? n->redirs.count : 0;
    size_t total = assignments->count + words->count + redir_tokens;
    char **parts = malloc((total ? total : 1) * sizeof(char *));
    char **owned = calloc(redir_tokens ? redir_tokens : 1, sizeof(char *));
    if (!parts || !owned) {
        free(parts);
        free(owned);
        restore_redirs(saved, saved_count);
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    size_t k = 0;
    for (size_t i = 0; i < assignments->count; i++) {
        parts[k++] = assignments->items[i];
    }
    for (size_t i = 0; i < words->count; i++) {
        parts[k++] = words->items[i];
    }
    for (size_t i = 0; i < redir_tokens; i++) {
        owned[i] = redirect_token(&n->redirs.items[i]);
        parts[k++] = owned[i] ? owned[i] : "";
    }
    char *line = join_words(parts, k);

    int rc = 1;
    if (line) {
        ParseResult parsed = parse_line(line);
        if (parsed.tokens && parsed.tokens[0]) {
            rc = execute(parsed.tokens);
        } else {
            last_command_exit_code = 0;
        }
        parse_result_free(&parsed);
    } else {
        last_command_exit_code = 1;
    }
    fflush(stdout);
    fflush(stderr);

    free(line);
    for (size_t i = 0; i < redir_tokens; i++) {
        free(owned[i]);
    }
    free(owned);
    free(parts);
    if (permanent) {
        forget_saved_redirs(saved, saved_count);
    } else {
        restore_redirs(saved, saved_count);
    }
    return status_from_execute(rc);
}

// ============================================================================
// Processes: pipelines, subshells, background jobs
// ============================================================================

static int wait_for_pid(pid_t pid) {
    int status;
    pid_t w;
    do {
        w = waitpid(pid, &status, 0);
    } while (w == -1 && errno == EINTR);
    if (w <= 0) {
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

// Child side of a fork: run the node and leave with its status
static void run_in_child_and_exit(const Node *node, bool direct_exec) {
    trap_reset_for_subshell();
    script_reset_for_subshell();
    if (direct_exec && node->type == NODE_SIMPLE) {
        exec_directly_in_child = true;
    }
    (void)exec_node(node);
    fflush(stdout);
    fflush(stderr);
    _exit(last_command_exit_code & 0xFF);
}

static ExecStatus exec_pipeline(const Node *n) {
    const NodeVec *cmds = &n->u.pipeline.commands;
    ExecStatus st = EXEC_OK;

    if (cmds->count == 1) {
        st = exec_node(cmds->items[0]);
    } else {
        int count = (int)cmds->count;
        int (*pipes)[2] = malloc((size_t)(count - 1) * sizeof(int[2]));
        pid_t *pids = malloc((size_t)count * sizeof(pid_t));
        if (!pipes || !pids) {
            free(pipes);
            free(pids);
            last_command_exit_code = 1;
            return EXEC_OK;
        }
        for (int i = 0; i < count - 1; i++) {
            if (pipe(pipes[i]) == -1) {
                perror(HASH_NAME);
                for (int j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(pipes);
                free(pids);
                last_command_exit_code = 1;
                return EXEC_OK;
            }
        }
        fflush(stdout);
        fflush(stderr);
        for (int i = 0; i < count; i++) {
            pids[i] = fork();
            if (pids[i] == -1) {
                perror(HASH_NAME);
                continue;
            }
            if (pids[i] == 0) {
                if (i > 0) {
                    dup2(pipes[i - 1][0], STDIN_FILENO);
                }
                if (i < count - 1) {
                    dup2(pipes[i][1], STDOUT_FILENO);
                }
                for (int j = 0; j < count - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                run_in_child_and_exit(cmds->items[i], true);
            }
        }
        for (int j = 0; j < count - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }

        sigset_t block_mask;
        sigset_t old_mask;
        sigemptyset(&block_mask);
        sigaddset(&block_mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &block_mask, &old_mask);
        int exit_code = 1;
        for (int i = 0; i < count; i++) {
            if (pids[i] <= 0) {
                continue;
            }
            int code = wait_for_pid(pids[i]);
            if (i == count - 1) {
                exit_code = code;
            }
        }
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        free(pipes);
        free(pids);
        last_command_exit_code = exit_code;
    }

    if (n->u.pipeline.negated && st == EXEC_OK) {
        last_command_exit_code = (last_command_exit_code == 0) ? 1 : 0;
    }
    return st;
}

static ExecStatus exec_subshell(const Node *n) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == -1) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    if (pid == 0) {
        trap_reset_for_subshell();
        script_reset_for_subshell();
        SavedFd saved[EXEC_MAX_REDIRS];
        int count = 0;
        if (n->redirs.count > 0 && !apply_redirs(&n->redirs, saved, &count)) {
            fflush(stderr);
            _exit(1);
        }
        (void)exec_node(n->u.group.body);
        int trap_exit = trap_execute_exit();
        fflush(stdout);
        fflush(stderr);
        _exit(((trap_exit >= 0) ? trap_exit : last_command_exit_code) & 0xFF);
    }
    last_command_exit_code = wait_for_pid(pid);
    return EXEC_OK;
}

// Trimmed source text of a node, for the job table
static char *node_text(const Node *n) {
    const char *s = n->src;
    size_t len = n->src_len;
    while (len > 0 && isspace((unsigned char)*s)) {
        s++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    char *text = malloc(len + 1);
    if (!text) {
        return NULL;
    }
    memcpy(text, s, len);
    text[len] = '\0';
    return text;
}

static ExecStatus exec_background(const Node *n) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == -1) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull != STDIN_FILENO) {
                close(devnull);
            }
        }
        // A background job is already a subshell: run `( ... ) &` in this
        // process so traps set inside apply to the process `kill $!` targets
        const Node *inner = n;
        while (inner->type == NODE_PIPELINE && inner->u.pipeline.commands.count == 1 &&
               !inner->u.pipeline.negated) {
            inner = inner->u.pipeline.commands.items[0];
        }
        if (inner->type == NODE_SUBSHELL) {
            trap_reset_for_subshell();
            script_reset_for_subshell();
            SavedFd saved[EXEC_MAX_REDIRS];
            int count = 0;
            if (inner->redirs.count > 0 && !apply_redirs(&inner->redirs, saved, &count)) {
                fflush(stderr);
                _exit(1);
            }
            (void)exec_node(inner->u.group.body);
            int trap_exit = trap_execute_exit();
            fflush(stdout);
            fflush(stderr);
            _exit(((trap_exit >= 0) ? trap_exit : last_command_exit_code) & 0xFF);
        }
        run_in_child_and_exit(n, false);
    }
    setpgid(pid, pid);
    char *text = node_text(n);
    int job_id = jobs_add(pid, text ? text : "");
    free(text);
    if (job_id > 0 && isatty(STDIN_FILENO) && shell_option_monitor()) {
        printf("[%d] %d\n", job_id, (int)pid);
    }
    jobs_set_last_bg_pid(pid);
    last_command_exit_code = 0;
    return EXEC_OK;
}

// ============================================================================
// Lists and and-or
// ============================================================================

// set -e: a failing command exits the shell unless it is tested by a
// condition, negated with !, or not the last command of an and-or list
static bool errexit_triggers(const Node *node) {
    if (!shell_option_errexit() || last_command_exit_code == 0) {
        return false;
    }
    if (script_get_in_condition()) {
        return false;
    }
    if (node->type == NODE_PIPELINE && node->u.pipeline.negated) {
        return false;
    }
    return true;
}

static ExecStatus exec_and_or(const Node *n, bool last_position) {
    const Node *left = n->u.and_or.left;
    ExecStatus st = (left->type == NODE_AND_OR) ? exec_and_or(left, false) : exec_node(left);
    if (st != EXEC_OK) {
        return st;
    }
    bool succeeded = (last_command_exit_code == 0);
    if ((n->u.and_or.op == TOKEN_AND_IF && !succeeded) || (n->u.and_or.op == TOKEN_OR_IF && succeeded)) {
        return EXEC_OK;
    }
    st = exec_node(n->u.and_or.right);
    if (st != EXEC_OK) {
        return st;
    }
    if (last_position && errexit_triggers(n->u.and_or.right)) {
        script_state.exit_requested = true;
        return EXEC_EXIT;
    }
    return EXEC_OK;
}

static ExecStatus exec_list(const Node *n) {
    const ListItemVec *items = &n->u.list.items;
    for (size_t i = 0; i < items->count; i++) {
        const ListItem *item = &items->items[i];
        ExecStatus st;
        if (item->async) {
            st = exec_background(item->node);
        } else if (item->node->type == NODE_AND_OR) {
            st = exec_and_or(item->node, true);
        } else {
            st = exec_node(item->node);
            if (st == EXEC_OK && errexit_triggers(item->node)) {
                script_state.exit_requested = true;
                st = EXEC_EXIT;
            }
        }
        if (st != EXEC_OK) {
            return st;
        }
    }
    return EXEC_OK;
}

// ============================================================================
// Compound commands
// ============================================================================

static ExecStatus exec_brace_core(const Node *n) {
    return exec_node(n->u.group.body);
}

static ExecStatus exec_condition(const Node *condition) {
    script_set_in_condition(true);
    ExecStatus st = exec_node(condition);
    script_set_in_condition(false);
    return st;
}

static ExecStatus exec_if_core(const Node *n) {
    const IfClauseVec *clauses = &n->u.if_clause.clauses;
    for (size_t i = 0; i < clauses->count; i++) {
        ExecStatus st = exec_condition(clauses->items[i].condition);
        if (st != EXEC_OK) {
            return st;
        }
        if (last_command_exit_code == 0) {
            return exec_node(clauses->items[i].body);
        }
    }
    if (n->u.if_clause.else_body) {
        return exec_node(n->u.if_clause.else_body);
    }
    last_command_exit_code = 0;     // POSIX: no branch taken
    return EXEC_OK;
}

// Interpret a loop body's status. Returns true when the loop must stop;
// break/continue aimed at this loop are consumed, deeper ones travel on.
static bool loop_body_done(ExecStatus *st) {
    switch (*st) {
        case EXEC_BREAK: {
            int levels = script_get_break_pending();
            if (levels > 1) {
                script_set_break_pending(levels - 1);
                return true;
            }
            script_set_break_pending(0);
            *st = EXEC_OK;
            return true;
        }
        case EXEC_CONTINUE: {
            int levels = script_get_continue_pending();
            if (levels > 1) {
                script_set_continue_pending(levels - 1);
                return true;
            }
            script_set_continue_pending(0);
            *st = EXEC_OK;
            return false;
        }
        case EXEC_OK:
            return false;
        default:
            return true;
    }
}

// Positional parameters $1.. as a fresh list (for `for name` without `in`)
static char **positional_values(int *count_out) {
    int count = 0;
    while (script_get_positional_param(count + 1)) {
        count++;
    }
    char **values = malloc(((size_t)count + 1) * sizeof(char *));
    if (!values) {
        *count_out = 0;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        values[i] = strdup(script_get_positional_param(i + 1));
    }
    values[count] = NULL;
    *count_out = count;
    return values;
}

static ExecStatus exec_for_core(const Node *n) {
    const char *var = n->u.for_clause.var;
    int count = 0;
    char **values;
    if (n->u.for_clause.has_words) {
        values = expand_words(n->u.for_clause.words.items, n->u.for_clause.words.count, true, true, &count);
    } else {
        values = positional_values(&count);
    }
    if (!values) {
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    script_push_context(CTX_FOR);
    ExecStatus st = EXEC_OK;
    int body_status = 0;
    for (int i = 0; i < count; i++) {
        // The body may have made the variable readonly
        if (shellvar_is_readonly(var)) {
            fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, var);
            script_pop_context();
            free_string_list(values, count);
            last_command_exit_code = 1;
            return is_interactive ? EXEC_OK : EXEC_EXIT;
        }
        shellvar_set(var, values[i]);
        setenv(var, values[i], 1);
        st = exec_node(n->u.for_clause.body);
        body_status = last_command_exit_code;
        if (loop_body_done(&st)) {
            break;
        }
    }
    script_pop_context();
    free_string_list(values, count);
    if (st == EXEC_OK) {
        last_command_exit_code = body_status;
    }
    return st;
}

static ExecStatus exec_loop_core(const Node *n) {
    bool until = (n->type == NODE_UNTIL);
    script_push_context(until ? CTX_UNTIL : CTX_WHILE);
    ExecStatus st = EXEC_OK;
    int body_status = 0;
    for (;;) {
        st = exec_condition(n->u.loop.condition);
        if (st != EXEC_OK) {
            if (loop_body_done(&st)) {
                break;
            }
            continue;
        }
        bool run_body = (last_command_exit_code == 0) != until;
        if (!run_body) {
            break;
        }
        st = exec_node(n->u.loop.body);
        body_status = last_command_exit_code;
        if (loop_body_done(&st)) {
            break;
        }
    }
    script_pop_context();
    if (st == EXEC_OK) {
        last_command_exit_code = body_status;
    }
    return st;
}

static ExecStatus exec_case_core(const Node *n) {
    char *word = script_expand_case_word(n->u.case_clause.word);
    if (!word) {
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    ExecStatus st = EXEC_OK;
    bool matched = false;
    const CaseItemVec *items = &n->u.case_clause.items;
    for (size_t i = 0; i < items->count && !matched; i++) {
        const CaseItem *item = &items->items[i];
        for (size_t j = 0; j < item->patterns.count; j++) {
            char *pattern = script_expand_case_pattern(item->patterns.items[j]);
            bool hit = pattern && fnmatch(pattern, word, 0) == 0;
            free(pattern);
            if (hit) {
                matched = true;
                st = exec_node(item->body);
                break;
            }
        }
    }
    free(word);
    if (!matched) {
        last_command_exit_code = 0;
    }
    return st;
}

static ExecStatus exec_function_def(const Node *n) {
    const Node *body = n->u.function.body;
    char *text = malloc(body->src_len + 1);
    if (!text) {
        last_command_exit_code = 1;
        return EXEC_OK;
    }
    memcpy(text, body->src, body->src_len);
    text[body->src_len] = '\0';
    script_define_function(n->u.function.name, text);
    free(text);
    last_command_exit_code = 0;
    return EXEC_OK;
}

// ============================================================================
// Function calls (installed into execute() as the function runner)
// ============================================================================

static int run_function(const ShellFunction *func, int argc, char **argv) {
    if (!func || !func->body) {
        return 1;
    }
    char **old_params = script_state.positional_params;
    int old_count = script_state.positional_count;
    bool old_exit_requested = script_state.exit_requested;
    bool old_in_script = script_state.in_script;
    bool old_direct = exec_directly_in_child;
    bool dynamic_scoping = shell_option_nonlexicalctrl();
    int old_break = script_get_break_pending();
    int old_continue = script_get_continue_pending();
    if (!dynamic_scoping) {
        script_set_break_pending(0);
        script_set_continue_pending(0);
    }

    script_state.positional_params = argv;
    script_state.positional_count = argc;
    script_state.exit_requested = false;
    script_state.in_script = true;      // `return` is allowed here
    exec_directly_in_child = false;     // The body has more than one command to run
    script_state.function_call_depth++;

    Arena arena;
    arena_init(&arena, 0);
    AstParseResult parsed = parse_program(func->body, &arena);
    ExecStatus st = EXEC_OK;
    if (parsed.status == PARSE_OK) {
        st = exec_node(parsed.root);
    } else {
        fprintf(stderr, "%s: %s: %s\n", HASH_NAME, func->name,
                parsed.message ? parsed.message : "syntax error");
        last_command_exit_code = 2;
    }
    arena_free(&arena);

    script_state.function_call_depth--;
    exec_directly_in_child = old_direct;
    script_state.in_script = old_in_script;
    script_state.positional_params = old_params;
    script_state.positional_count = old_count;

    bool exit_called = script_state.exit_requested || st == EXEC_EXIT;
    if (exit_called) {
        if (!dynamic_scoping) {
            script_set_break_pending(old_break);
            script_set_continue_pending(old_continue);
        }
        script_state.exit_requested = true;
        return 0;
    }
    if (dynamic_scoping && (script_get_break_pending() > 0 || script_get_continue_pending() > 0)) {
        return 0;
    }
    if (!dynamic_scoping) {
        script_set_break_pending(old_break);
        script_set_continue_pending(old_continue);
    }
    script_set_return_pending(false);
    script_state.exit_requested = old_exit_requested;
    return 1;
}

// ============================================================================
// Entry points
// ============================================================================

ExecStatus exec_node(const Node *node) {
    if (!node) {
        return EXEC_OK;
    }
    switch (node->type) {
        case NODE_LIST:
            return exec_list(node);
        case NODE_AND_OR:
            return exec_and_or(node, false);
        case NODE_PIPELINE:
            return exec_pipeline(node);
        case NODE_SIMPLE:
            return exec_simple(node);
        case NODE_SUBSHELL:
            return exec_subshell(node);
        case NODE_BRACE:
            return with_redirs(node, exec_brace_core);
        case NODE_IF:
            return with_redirs(node, exec_if_core);
        case NODE_FOR:
            return with_redirs(node, exec_for_core);
        case NODE_WHILE:
        case NODE_UNTIL:
            return with_redirs(node, exec_loop_core);
        case NODE_CASE:
            return with_redirs(node, exec_case_core);
        case NODE_FUNCTION:
            return exec_function_def(node);
    }
    return EXEC_OK;
}

static void install_hooks(void) {
    execute_set_function_runner(run_function);
}

ExecStatus exec_string(const char *src, bool *incomplete) {
    install_hooks();

    Arena arena;
    arena_init(&arena, 0);
    AstParseResult parsed = parse_program(src, &arena);
    ExecStatus st = EXEC_OK;
    if (incomplete) {
        *incomplete = (parsed.status == PARSE_INCOMPLETE);
    }
    if (parsed.status == PARSE_OK) {
        st = exec_node(parsed.root);
    } else if (parsed.status == PARSE_ERROR || !incomplete) {
        fprintf(stderr, "%s: %s\n", HASH_NAME, parsed.message ? parsed.message : "syntax error");
        last_command_exit_code = 2;
    }
    arena_free(&arena);
    return st;
}

int exec_string_status(const char *src) {
    bool old_in_script = script_state.in_script;
    script_state.in_script = true;
    bool incomplete = false;
    (void)exec_string(src, &incomplete);
    if (incomplete) {
        fprintf(stderr, "%s: syntax error: unexpected end of file\n", HASH_NAME);
        last_command_exit_code = 2;
    }
    script_state.in_script = old_in_script;
    return last_command_exit_code;
}

// ============================================================================
// Line feeder
// ============================================================================

void exec_feeder_init(ExecFeeder *feeder) {
    feeder->text = NULL;
    feeder->len = 0;
    feeder->cap = 0;
}

void exec_feeder_free(ExecFeeder *feeder) {
    free(feeder->text);
    exec_feeder_init(feeder);
}

void exec_feeder_reset(ExecFeeder *feeder) {
    feeder->len = 0;
    if (feeder->text) {
        feeder->text[0] = '\0';
    }
}

bool exec_feeder_pending(const ExecFeeder *feeder) {
    return feeder->len > 0;
}

static bool feeder_append(ExecFeeder *feeder, const char *line) {
    size_t n = strlen(line);
    if (feeder->len + n + 2 > feeder->cap) {
        size_t new_cap = feeder->cap ? feeder->cap * 2 : 256;
        while (new_cap < feeder->len + n + 2) {
            new_cap *= 2;
        }
        char *fresh = realloc(feeder->text, new_cap);
        if (!fresh) {
            return false;
        }
        feeder->text = fresh;
        feeder->cap = new_cap;
    }
    memcpy(feeder->text + feeder->len, line, n);
    feeder->len += n;
    feeder->text[feeder->len++] = '\n';
    feeder->text[feeder->len] = '\0';
    return true;
}

FeedResult exec_feed_line(ExecFeeder *feeder, const char *line, ExecStatus *status) {
    *status = EXEC_OK;
    if (!feeder_append(feeder, line)) {
        fprintf(stderr, "%s: out of memory\n", HASH_NAME);
        exec_feeder_reset(feeder);
        last_command_exit_code = 1;
        return FEED_SYNTAX_ERROR;
    }
    install_hooks();

    Arena arena;
    arena_init(&arena, 0);
    AstParseResult parsed = parse_program(feeder->text, &arena);
    FeedResult result;
    if (parsed.status == PARSE_INCOMPLETE) {
        result = FEED_NEED_MORE;
    } else if (parsed.status == PARSE_ERROR) {
        fprintf(stderr, "%s: %s\n", HASH_NAME, parsed.message ? parsed.message : "syntax error");
        last_command_exit_code = 2;
        exec_feeder_reset(feeder);
        result = FEED_SYNTAX_ERROR;
    } else {
        // The nodes view the text they were parsed from (a background job
        // records its command from it), so the program keeps the text while
        // it runs and the feeder starts over with an empty one
        char *program = feeder->text;
        exec_feeder_init(feeder);
        *status = exec_node(parsed.root);
        free(program);
        result = FEED_RAN;
    }
    arena_free(&arena);
    return result;
}
