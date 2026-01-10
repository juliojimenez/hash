#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include "hash.h"
#include "execute.h"
#include "builtins.h"
#include "config.h"
#include "parser.h"
#include "expand.h"
#include "varexpand.h"
#include "cmdsub.h"
#include "arith.h"
#include "redirect.h"
#include "jobs.h"
#include "safe_string.h"
#include "script.h"

// Global to store last exit code
int last_command_exit_code = 0;

// Debug flag - set to 1 to enable exit code tracing
#define DEBUG_EXIT_CODE 0

// Launch an external program
static int launch(char **args, const char *cmd_string) {
    pid_t pid;
    int status;

    // Parse redirections
    RedirInfo *redir = redirect_parse(args);

    // Set heredoc content if pending
    const char *heredoc = script_get_pending_heredoc();
    if (heredoc && redir) {
        redirect_set_heredoc_content(redir, heredoc);
    }

    // Use cleaned args (or original if no redirections)
    char **exec_args = redir ? redir->args : args;

    pid = fork();
    if (pid == 0) {
        // Child process

        // Put child in its own process group
        setpgid(0, 0);

        // Restore default signal handlers in child
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        // Apply redirections
        if (redir && redirect_apply(redir) != 0) {
            _exit(EXIT_FAILURE);
        }

        // Execute command
        if (execvp(exec_args[0], exec_args) == -1) {
            if (!script_state.silent_errors) {
                perror(HASH_NAME);
            }
        }
        // Use _exit() instead of exit() to avoid flushing parent's stdio buffers
        // This prevents file position corruption when reading scripts
        _exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Fork error
        if (!script_state.silent_errors) {
            perror(HASH_NAME);
        }
        last_command_exit_code = 1;
    } else {
        // Parent process

        // Put child in its own process group
        setpgid(pid, pid);

        // Block SIGCHLD while waiting for foreground process
        // This prevents the SIGCHLD handler from reaping our child
        sigset_t block_mask, old_mask;
        sigemptyset(&block_mask);
        sigaddset(&block_mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

        // Give terminal control to child process group
        tcsetpgrp(STDIN_FILENO, pid);

        // Wait for child, but also handle stopped state
        pid_t wpid;
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (wpid == -1 && errno == EINTR);

        // Take back terminal control
        tcsetpgrp(STDIN_FILENO, getpgrp());

        // Restore SIGCHLD handling
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        // Handle the result
        if (wpid > 0) {
            if (WIFEXITED(status)) {
                last_command_exit_code = WEXITSTATUS(status);
#if DEBUG_EXIT_CODE
                fprintf(stderr, "DEBUG: launch() WEXITSTATUS=%d for '%s'\n", last_command_exit_code, exec_args[0]);
#endif
            } else if (WIFSIGNALED(status)) {
                last_command_exit_code = 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                // Process was stopped (Ctrl+Z)
                // Add to job table
                int job_id = jobs_add(pid, cmd_string ? cmd_string : exec_args[0]);

                // Update job state to stopped
                Job *job = jobs_get(job_id);
                if (job) {
                    job->state = JOB_STOPPED;
                }

                // Print notification
                printf("\n[%d]+  Stopped                 %s\n", job_id, cmd_string ? cmd_string : exec_args[0]);

                last_command_exit_code = 128 + WSTOPSIG(status);
            }
        } else {
            // waitpid failed - this shouldn't happen normally
            // The child may have been reaped unexpectedly
            last_command_exit_code = 1;
        }
    }

    // Clean up
    redirect_free(redir);

    return 1;
}

// Build command string from args for display
static char *build_cmd_string(char **args) {
    if (!args || !args[0]) return NULL;

    size_t total_len = 0;
    for (int i = 0; args[i] != NULL; i++) {
        total_len += strlen(args[i]) + 1;  // +1 for space or null
    }

    char *cmd = malloc(total_len);
    if (!cmd) return NULL;

    cmd[0] = '\0';
    for (int i = 0; args[i] != NULL; i++) {
        if (i > 0) safe_strcat(cmd, " ", total_len);
        safe_strcat(cmd, args[i], total_len);
    }

    return cmd;
}

// Free expanded arguments
static void free_expanded_args(char **expanded_args, int count) {
    for (int i = 0; i < count; i++) {
        free(expanded_args[i]);
    }
}

// Free glob-expanded array (all strings and the array itself)
static void free_glob_args(char **glob_args, int count) {
    if (!glob_args) return;
    for (int i = 0; i < count; i++) {
        free(glob_args[i]);
    }
    free(glob_args);
}

// Execute command (built-in or external)
int execute(char **args) {
    if (args[0] == NULL) {
        // Empty command
        last_command_exit_code = 0;
        return 1;
    }

    // Track original args to know which ones we allocated
    // We need to free expanded args later
    char *expanded_args[MAX_ARGS];
    int expanded_count = 0;

    // Count args and save original pointers
    char *original_ptrs[MAX_ARGS];
    int arg_count = 0;
    for (int i = 0; args[i] != NULL && i < MAX_ARGS - 1; i++) {
        original_ptrs[i] = args[i];
        arg_count++;
    }

    // Expand tilde in all arguments
    expand_tilde(args);

    // Track which args were expanded by tilde
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
            original_ptrs[i] = args[i];  // Update for next expansion check
        }
    }

    // Expand command substitutions in all arguments
    cmdsub_args(args);

    // Track which args were expanded by cmdsub
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
            original_ptrs[i] = args[i];
        }
    }

    // Expand arithmetic substitutions in all arguments
    arith_args(args);

    // Track which args were expanded by arith
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
            original_ptrs[i] = args[i];
        }
    }

    // Clear varexpand error flag before expansion
    varexpand_clear_error();

    // Expand variables in all arguments
    varexpand_args(args, last_command_exit_code);

    // Check for unset variable error (set -u)
    if (varexpand_had_error()) {
        // Free any expanded args before returning
        for (int i = 0; i < arg_count; i++) {
            if (args[i] != original_ptrs[i]) {
                free(args[i]);
            }
        }
        last_command_exit_code = 1;
        return 0;  // Return 0 to signal script should exit
    }

    // Track which args were expanded by varexpand
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
        }
    }

    // Glob (pathname) expansion - may change the number of arguments
    // expand_glob creates a new array with all strings strdup'd
    char **glob_args = NULL;
    int glob_arg_count = arg_count;
    bool glob_expanded = false;

    // Check if any argument has glob characters
    bool has_globs = false;
    for (int i = 0; i < arg_count; i++) {
        if (has_glob_chars(args[i])) {
            has_globs = true;
            break;
        }
    }

    if (has_globs) {
        // Create a copy of args pointers for expand_glob
        glob_args = malloc((arg_count + 1) * sizeof(char *));
        if (glob_args) {
            for (int i = 0; i < arg_count; i++) {
                glob_args[i] = args[i];
            }
            glob_args[arg_count] = NULL;

            char **old_glob_args = glob_args;
            if (expand_glob(&glob_args, &glob_arg_count) == 0 && glob_args != old_glob_args) {
                glob_expanded = true;
                // expand_glob created a new array - free the temp one we made
                free(old_glob_args);
            } else {
                // No expansion happened - free our temp array
                free(glob_args);
                glob_args = NULL;
            }
        }
    }

    // Use expanded args if glob expansion happened, otherwise use original args
    char **exec_input = glob_expanded ? glob_args : args;

    // Check for variable assignment (VAR=VALUE with no command following)
    // Must have = and start with valid variable name character
    if (args[0] && args[1] == NULL) {
        char *equals = strchr(args[0], '=');
        if (equals && equals != args[0]) {
            // Check if it's a valid variable name before the =
            int valid = 1;
            for (char *p = args[0]; p < equals; p++) {
                if (p == args[0]) {
                    if (!isalpha(*p) && *p != '_') {
                        valid = 0;
                        break;
                    }
                } else {
                    if (!isalnum(*p) && *p != '_') {
                        valid = 0;
                        break;
                    }
                }
            }
            if (valid) {
                // This is a variable assignment
                *equals = '\0';
                const char *name = args[0];
                const char *value = equals + 1;

                // Expand tildes in the value (for PATH-like assignments)
                char *tilde_expanded = expand_tilde_in_assignment(value);
                if (tilde_expanded) {
                    setenv(name, tilde_expanded, 1);
                    free(tilde_expanded);
                } else {
                    setenv(name, value, 1);
                }

                *equals = '=';  // Restore in case it's in shared memory
                last_command_exit_code = 0;
                if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
                free_expanded_args(expanded_args, expanded_count);
                return 1;
            }
        }
    }

    int result = 1;  // Default: continue shell

    // Check if command is an alias (use original args[0] for alias lookup)
    const char *alias_value = config_get_alias(exec_input[0]);
    if (alias_value) {
        // Expand alias by parsing the alias value
        char *alias_line = strdup(alias_value);
        if (!alias_line) {
            last_command_exit_code = 1;
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            return 1;
        }

        char **alias_args = parse_line(alias_line);
        if (!alias_args) {
            free(alias_line);
            last_command_exit_code = 1;
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            return 1;
        }

        // If original command had arguments, we need to append them
        // Count original args (excluding command name)
        int orig_arg_count = 0;
        for (int j = 1; exec_input[j] != NULL; j++) {
            orig_arg_count++;
        }

        if (orig_arg_count > 0) {
            // Count alias args
            int alias_arg_count = 0;
            while (alias_args[alias_arg_count] != NULL) {
                alias_arg_count++;
            }

            // Create new args array with alias + original args
            char **combined_args = malloc((alias_arg_count + orig_arg_count + 1) * sizeof(char*));
            if (combined_args) {
                // Copy alias args
                for (int i = 0; i < alias_arg_count; i++) {
                    combined_args[i] = alias_args[i];
                }
                // Append original args (skip command name)
                for (int i = 0; i < orig_arg_count; i++) {
                    combined_args[alias_arg_count + i] = exec_input[i + 1];
                }
                combined_args[alias_arg_count + orig_arg_count] = NULL;

                // Execute with combined args
                result = execute(combined_args);

                free(combined_args);
                free(alias_args);
                free(alias_line);
                if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
                free_expanded_args(expanded_args, expanded_count);
                return result;
            }
        }

        // No original args, just execute alias
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: Executing alias '%s' -> '%s'\n", exec_input[0], alias_value);
        fprintf(stderr, "DEBUG: Before recursive execute, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        result = execute(alias_args);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After recursive execute, last_command_exit_code=%d, result=%d\n", last_command_exit_code, result);
#endif
        free(alias_args);
        free(alias_line);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After freeing alias stuff, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After free_expanded_args, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        return result;
    }

    // Check for redirections
    RedirInfo *redir = redirect_parse(exec_input);

    // Set heredoc content if pending
    const char *heredoc = script_get_pending_heredoc();
    if (heredoc && redir) {
        redirect_set_heredoc_content(redir, heredoc);
    }

    char **exec_args = redir ? redir->args : exec_input;

    // Check if this is a builtin first (without executing it)
    int is_builtin_cmd = exec_args[0] ? is_builtin(exec_args[0]) : 0;

    // Check if this is a flow-control builtin that must NOT run in a child process
    // These builtins affect the shell's execution flow and their return values matter
    bool is_flow_control = false;
    if (exec_args[0]) {
        is_flow_control = (strcmp(exec_args[0], "break") == 0 ||
                          strcmp(exec_args[0], "continue") == 0 ||
                          strcmp(exec_args[0], "return") == 0 ||
                          strcmp(exec_args[0], "exit") == 0);
    }

    // If it's a builtin with redirections (but NOT flow control), run in child process
    if (is_builtin_cmd && redir && redir->count > 0 && !is_flow_control) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process - apply redirections and run builtin
            if (redirect_apply(redir) != 0) {
                _exit(EXIT_FAILURE);
            }
            try_builtin(exec_args);
            // The builtin sets last_command_exit_code, use that as exit code
            redirect_free(redir);
            // Use _exit() to avoid flushing parent's stdio buffers
            _exit(last_command_exit_code);
        } else if (pid > 0) {
            // Parent - wait for child
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                last_command_exit_code = WEXITSTATUS(status);
            }
        }
        redirect_free(redir);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        return 1;
    }

    // For flow-control builtins with redirections, handle in same process
    // Save and restore file descriptors
    int saved_fds[3] = {-1, -1, -1};  // stdin, stdout, stderr
    if (is_flow_control && redir && redir->count > 0) {
        // Save current file descriptors
        saved_fds[0] = dup(STDIN_FILENO);
        saved_fds[1] = dup(STDOUT_FILENO);
        saved_fds[2] = dup(STDERR_FILENO);
        // Apply redirections
        redirect_apply(redir);
    }

    // Try built-in commands (no redirections, or will be handled below)
    result = try_builtin(exec_args);

    // Restore file descriptors if we saved them for flow-control builtins
    if (saved_fds[0] != -1 || saved_fds[1] != -1 || saved_fds[2] != -1) {
        if (saved_fds[0] != -1) { dup2(saved_fds[0], STDIN_FILENO); close(saved_fds[0]); }
        if (saved_fds[1] != -1) { dup2(saved_fds[1], STDOUT_FILENO); close(saved_fds[1]); }
        if (saved_fds[2] != -1) { dup2(saved_fds[2], STDERR_FILENO); close(saved_fds[2]); }
    }

    if (result != -1) {
        redirect_free(redir);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        return result;
    }
    redirect_free(redir);

    // Check for user-defined functions
    ShellFunction *func = script_get_function(exec_input[0]);
    if (func) {
        // Count arguments (including function name as $0)
        int argc = 0;
        while (exec_input[argc]) argc++;

        result = script_execute_function(func, argc, exec_input);
        last_command_exit_code = result;
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        return result;
    }

    // Build command string for job display
    char *cmd_string = build_cmd_string(exec_input);

    // Launch external program
    result = launch(exec_input, cmd_string);

    free(cmd_string);
    if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
    free_expanded_args(expanded_args, expanded_count);

    return result;
}

// Get last exit code
int execute_get_last_exit_code(void) {
    return last_command_exit_code;
}
