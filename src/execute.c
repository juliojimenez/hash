#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include "hash.h"
#include "execute.h"
#include "builtins.h"
#include "config.h"
#include "parser.h"
#include "expand.h"
#include "varexpand.h"
#include "cmdsub.h"
#include "redirect.h"
#include "jobs.h"
#include "safe_string.h"
#include "script.h"

// Global to store last exit code
int last_command_exit_code = 0;

// Launch an external program
static int launch(char **args, const char *cmd_string) {
    pid_t pid;
    int status;

    // Parse redirections
    RedirInfo *redir = redirect_parse(args);

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
            exit(EXIT_FAILURE);
        }

        // Execute command
        if (execvp(exec_args[0], exec_args) == -1) {
            if (!script_state.silent_errors) {
                perror(HASH_NAME);
            }
        }
        exit(EXIT_FAILURE);
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

        // Give terminal control to child process group
        tcsetpgrp(STDIN_FILENO, pid);

        // Wait for child, but also handle stopped state
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

        // Take back terminal control
        tcsetpgrp(STDIN_FILENO, getpgrp());

        // Handle the result
        if (WIFEXITED(status)) {
            last_command_exit_code = WEXITSTATUS(status);
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

    // Expand variables in all arguments
    varexpand_args(args, last_command_exit_code);

    // Track which args were expanded by varexpand
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
        }
    }

    int result = 1;  // Default: continue shell

    // Check if command is an alias
    const char *alias_value = config_get_alias(args[0]);
    if (alias_value) {
        // Expand alias by parsing the alias value
        char *alias_line = strdup(alias_value);
        if (!alias_line) {
            last_command_exit_code = 1;
            free_expanded_args(expanded_args, expanded_count);
            return 1;
        }

        char **alias_args = parse_line(alias_line);
        if (!alias_args) {
            free(alias_line);
            last_command_exit_code = 1;
            free_expanded_args(expanded_args, expanded_count);
            return 1;
        }

        // If original command had arguments, we need to append them
        // Count original args (excluding command name)
        int orig_arg_count = 0;
        for (int j = 1; args[j] != NULL; j++) {
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
                    combined_args[alias_arg_count + i] = args[i + 1];
                }
                combined_args[alias_arg_count + orig_arg_count] = NULL;

                // Execute with combined args
                result = execute(combined_args);

                free(combined_args);
                free(alias_args);
                free(alias_line);
                free_expanded_args(expanded_args, expanded_count);
                return result;
            }
        }

        // No original args, just execute alias
        result = execute(alias_args);
        free(alias_args);
        free(alias_line);
        free_expanded_args(expanded_args, expanded_count);
        return result;
    }

    // Check for redirections
    RedirInfo *redir = redirect_parse(args);
    char **exec_args = redir ? redir->args : args;

    // Try built-in commands first
    result = try_builtin(exec_args);
    if (result != -1) {
        // If there were redirections for a builtin, we need to handle them
        // Run builtin in a child process if redirections are present
        if (redir && redir->count > 0) {
            // Need to re-execute with redirections in a child
            pid_t pid = fork();
            if (pid == 0) {
                // Child process - apply redirections and run builtin
                if (redirect_apply(redir) != 0) {
                    exit(EXIT_FAILURE);
                }
                // Re-run the builtin (result already computed, but we need output redirected)
                int builtin_result = try_builtin(exec_args);
                redirect_free(redir);
                exit(builtin_result == 0 ? EXIT_SUCCESS :
                     (builtin_result == 1 ? EXIT_SUCCESS : EXIT_FAILURE));
            } else if (pid > 0) {
                // Parent - wait for child
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    last_command_exit_code = WEXITSTATUS(status);
                }
            }
            redirect_free(redir);
            free_expanded_args(expanded_args, expanded_count);
            return 1;
        }
        redirect_free(redir);
        free_expanded_args(expanded_args, expanded_count);
        return result;
    }
    redirect_free(redir);

    // Build command string for job display
    char *cmd_string = build_cmd_string(args);

    // Launch external program
    result = launch(args, cmd_string);

    free(cmd_string);
    free_expanded_args(expanded_args, expanded_count);

    return result;
}

// Get last exit code
int execute_get_last_exit_code(void) {
    return last_command_exit_code;
}
