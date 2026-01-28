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
#include "shellvar.h"
#include "ifs.h"
#include "syslimits.h"

// Global to store last exit code
int last_command_exit_code = 0;

// Debug flag - set to 1 to enable exit code tracing
#define DEBUG_EXIT_CODE 0

// Launch an external program
static int launch(char **args, const char *cmd_string) {
    if (!args || !args[0]) return 1;

    pid_t pid;
    int status;

    // Parse redirections
    RedirInfo *redir = redirect_parse(args);

    // Set heredoc content if pending
    const char *heredoc = script_get_pending_heredoc();
    char *expanded_heredoc = NULL;
    if (heredoc && redir) {
        int heredoc_quoted = script_get_pending_heredoc_quoted();
        if (!heredoc_quoted) {
            // Expand heredoc content BEFORE fork so we can handle errors
            varexpand_clear_error();
            // Apply command substitution expansion
            char *cmdsub_result = cmdsub_expand(heredoc);
            const char *content = cmdsub_result ? cmdsub_result : heredoc;
            // Apply variable expansion
            char *var_result = varexpand_expand(content, last_command_exit_code);
            if (cmdsub_result) free(cmdsub_result);

            if (varexpand_had_error()) {
                // Expansion error (like ${x?z}) - exit non-interactive shell
                free(var_result);
                redirect_free(redir);
                last_command_exit_code = 1;
                return is_interactive ? 1 : 0;
            }
            expanded_heredoc = var_result;
            // Strip \x03 IFS markers from heredoc content (heredocs don't undergo IFS splitting)
            if (expanded_heredoc) {
                const char *read = expanded_heredoc;
                char *write = expanded_heredoc;
                while (*read) {
                    if (*read != '\x03') {
                        *write++ = *read;
                    }
                    read++;
                }
                *write = '\0';
            }
            redirect_set_heredoc_content(redir, expanded_heredoc ? expanded_heredoc : heredoc, 1);
        } else {
            redirect_set_heredoc_content(redir, heredoc, heredoc_quoted);
        }
    }

    // Use cleaned args (or original if no redirections)
    char **exec_args = redir ? redir->args : args;

    // Strip quote markers after redirect parsing (for external commands)
    strip_quote_markers_args(exec_args);

    // Check if arguments would exceed system ARG_MAX limit
    if (syslimits_check_exec_args(exec_args) != 0) {
        fprintf(stderr, "%s: argument list too long\n", HASH_NAME);
        free(expanded_heredoc);
        redirect_free(redir);
        last_command_exit_code = 126;
        return 1;
    }

    // Find and cache the command path before forking
    char *cmd_path = NULL;
    if (exec_args[0] && strchr(exec_args[0], '/') == NULL) {
        cmd_path = find_in_path(exec_args[0]);
    }

    pid = fork();
    if (pid == 0) {
        // Child process

        // Put child in its own process group (only in interactive mode)
        if (is_interactive) {
            setpgid(0, 0);
        }

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
        if (!exec_args || !exec_args[0]) {
            _exit(EXIT_FAILURE);
        }
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

        // Job control setup only in interactive mode
        sigset_t block_mask, old_mask;
        if (is_interactive) {
            // Put child in its own process group
            setpgid(pid, pid);

            // Block SIGCHLD while waiting for foreground process
            // This prevents the SIGCHLD handler from reaping our child
            sigemptyset(&block_mask);
            sigaddset(&block_mask, SIGCHLD);
            sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

            // Give terminal control to child process group
            tcsetpgrp(STDIN_FILENO, pid);
        }

        // Wait for child, but also handle stopped state
        pid_t wpid;
        do {
            wpid = waitpid(pid, &status, is_interactive ? WUNTRACED : 0);
        } while (wpid == -1 && errno == EINTR);

        if (is_interactive) {
            // Take back terminal control
            tcsetpgrp(STDIN_FILENO, getpgrp());

            // Restore SIGCHLD handling
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
        }

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

        // Add command to hash table if we found its path
        if (cmd_path) {
            cmd_hash_add(exec_args[0], cmd_path);
        }
    }

    // Clean up
    free(cmd_path);
    free(expanded_heredoc);
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

// Check if string is a valid variable assignment (VAR=VALUE)
// Returns pointer to '=' if valid, NULL otherwise
static char *is_var_assignment(const char *arg) {
    if (!arg) return NULL;

    char *equals = strchr(arg, '=');
    if (!equals || equals == arg) return NULL;

    // Check if characters before = form a valid variable name
    for (const char *p = arg; p < equals; p++) {
        if (p == arg) {
            if (!isalpha(*p) && *p != '_') return NULL;
        } else {
            if (!isalnum(*p) && *p != '_') return NULL;
        }
    }

    return equals;
}

// Check if string is a redirection operator or contains one
// Returns true if arg is or contains a redirection target
static bool is_redirection_arg(const char *arg) {
    if (!arg || arg[0] == '\x01') return false;  // Skip quoted args

    // Standalone operators
    if (strcmp(arg, "<") == 0 || strcmp(arg, ">") == 0 ||
        strcmp(arg, ">>") == 0 || strcmp(arg, "<<") == 0 ||
        strcmp(arg, "<<-") == 0 || strcmp(arg, "2>") == 0 ||
        strcmp(arg, "2>>") == 0 || strcmp(arg, "&>") == 0 ||
        strcmp(arg, "2>&1") == 0 || strcmp(arg, ">&2") == 0 ||
        strcmp(arg, "1>&2") == 0) {
        return false;  // Operators themselves don't need expansion
    }

    // Attached redirections: >file, <file, >>file, 2>file, etc.
    if (arg[0] == '<' || arg[0] == '>') return true;

    // N>file, N>>file patterns
    const char *p = arg;
    while (*p && isdigit(*p)) p++;
    if (p != arg && (*p == '>' || *p == '<')) return true;

    return false;
}

// Structure to store prefix assignments for restoration
#define MAX_PREFIX_VARS 64
typedef struct {
    char *name;
    char *old_env_value;    // Old value from environment
    char *old_shell_value;  // Old value from shell variable table
    bool env_was_set;
    bool shell_was_set;
} PrefixVar;

static PrefixVar prefix_vars[MAX_PREFIX_VARS];
static int prefix_var_count = 0;

// Save a variable's current value before prefix assignment (from both env and shell table)
static void save_prefix_var(const char *name) {
    if (prefix_var_count >= MAX_PREFIX_VARS) return;

    prefix_vars[prefix_var_count].name = strdup(name);

    // Save from environment
    const char *env_val = getenv(name);
    if (env_val) {
        prefix_vars[prefix_var_count].old_env_value = strdup(env_val);
        prefix_vars[prefix_var_count].env_was_set = true;
    } else {
        prefix_vars[prefix_var_count].old_env_value = NULL;
        prefix_vars[prefix_var_count].env_was_set = false;
    }

    // Save from shell variable table
    const char *shell_val = shellvar_get(name);
    if (shell_val) {
        prefix_vars[prefix_var_count].old_shell_value = strdup(shell_val);
        prefix_vars[prefix_var_count].shell_was_set = true;
    } else {
        prefix_vars[prefix_var_count].old_shell_value = NULL;
        prefix_vars[prefix_var_count].shell_was_set = false;
    }

    prefix_var_count++;
}

// Set prefix variable in both environment and shell table
static void set_prefix_var(const char *name, const char *value) {
    // Set in environment for child processes
    setenv(name, value, 1);
    // Set in shell variable table for shell expansion
    shellvar_set(name, value);
}

// Restore prefix variables to their original state
static void restore_prefix_vars(void) {
    for (int i = 0; i < prefix_var_count; i++) {
        // Restore environment
        if (prefix_vars[i].env_was_set) {
            setenv(prefix_vars[i].name, prefix_vars[i].old_env_value, 1);
            free(prefix_vars[i].old_env_value);
        } else {
            unsetenv(prefix_vars[i].name);
        }

        // Restore shell variable table
        if (prefix_vars[i].shell_was_set) {
            shellvar_set(prefix_vars[i].name, prefix_vars[i].old_shell_value);
            free(prefix_vars[i].old_shell_value);
        } else {
            shellvar_unset(prefix_vars[i].name);
        }

        free(prefix_vars[i].name);
    }
    prefix_var_count = 0;
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

    // Expand tilde in all arguments (for args starting with ~)
    expand_tilde(args);

    // Also expand tildes in assignment values BEFORE command substitution
    // This is the correct POSIX order: tilde expansion before cmdsub
    for (int i = 0; i < arg_count; i++) {
        const char *eq = is_var_assignment(args[i]);
        if (eq) {
            // This is an assignment - expand tildes in the value part
            const char *value = eq + 1;
            char *tilde_exp = expand_tilde_in_assignment(value);
            if (tilde_exp) {
                // Build new argument with expanded value
                size_t name_len = eq - args[i] + 1;  // includes '='
                size_t new_len = name_len + strlen(tilde_exp) + 1;
                char *new_arg = malloc(new_len);
                if (new_arg) {
                    memcpy(new_arg, args[i], name_len);
                    strcpy(new_arg + name_len, tilde_exp);
                    // Track if we need to free the original
                    if (args[i] != original_ptrs[i]) {
                        // Already expanded, free the old one
                        free(args[i]);
                    }
                    args[i] = new_arg;
                    expanded_args[expanded_count++] = new_arg;
                    original_ptrs[i] = new_arg;
                }
                free(tilde_exp);
            }
        }
    }

    // Track which args were expanded by tilde
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
            original_ptrs[i] = args[i];  // Update for next expansion check
        }
    }

    // Reset command substitution exit code tracker before expansion
    cmdsub_reset_exit_code();

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

    // POSIX evaluation order: expand redirections BEFORE variable assignments
    // This ensures ${x=redir} in redirections takes effect before ${x=assign} in assignments
    // First pass: expand only redirection arguments
    for (int i = 0; i < arg_count; i++) {
        const char *arg = args[i];
        if (arg == NULL) continue;
        if (is_redirection_arg(arg) && strchr(arg, '$') != NULL) {
            char *expanded = varexpand_expand(args[i], last_command_exit_code);
            if (expanded) {
                args[i] = expanded;
            }
        }
    }

    // Second pass: expand non-redirection arguments (assignments and command words)
    for (int i = 0; i < arg_count; i++) {
        const char *arg = args[i];
        if (arg == NULL) continue;
        if (!is_redirection_arg(original_ptrs[i]) && strchr(arg, '$') != NULL) {
            char *expanded = varexpand_expand(args[i], last_command_exit_code);
            if (expanded) {
                args[i] = expanded;
            }
        }
    }

    // Check for unset variable error (set -u)
    if (varexpand_had_error()) {
        // Free any expanded args before returning
        for (int i = 0; i < arg_count; i++) {
            if (args[i] != original_ptrs[i]) {
                free(args[i]);
            }
        }
        last_command_exit_code = 1;
        return is_interactive ? 1 : 0;  // Continue in interactive mode, exit in non-interactive
    }

    // Track which args were expanded by varexpand
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != original_ptrs[i]) {
            expanded_args[expanded_count++] = args[i];
        }
    }

    // IFS word splitting - may change the number of arguments
    // ifs_split_args processes \x03 markers from unquoted expansions
    char **ifs_args = NULL;
    int ifs_arg_count = arg_count;
    bool ifs_expanded = false;

    // Make a copy of args for ifs_split_args to potentially replace
    ifs_args = malloc((arg_count + 1) * sizeof(char *));
    if (ifs_args) {
        for (int i = 0; i < arg_count; i++) {
            ifs_args[i] = args[i];
        }
        ifs_args[arg_count] = NULL;

        char **old_ifs_args = ifs_args;
        if (ifs_split_args(&ifs_args, &ifs_arg_count) == 0 && ifs_args != old_ifs_args) {
            ifs_expanded = true;
            // ifs_split_args created a new array - free the temp one we made
            free(old_ifs_args);
            // Update args and arg_count for subsequent phases
            arg_count = ifs_arg_count;
        } else {
            // No splitting happened or same array returned - free our temp array
            free(ifs_args);
            ifs_args = NULL;
        }
    }

    // Use IFS-split args if expansion happened
    char **glob_input = ifs_expanded ? ifs_args : args;

    // Glob (pathname) expansion - may change the number of arguments
    // expand_glob creates a new array with all strings strdup'd
    char **glob_args = NULL;
    int glob_arg_count = arg_count;
    bool glob_expanded = false;

    // Check if any argument has glob characters
    bool has_globs = false;
    for (int i = 0; i < arg_count; i++) {
        if (has_glob_chars(glob_input[i])) {
            has_globs = true;
            break;
        }
    }

    if (has_globs) {
        // Create a copy of args pointers for expand_glob
        glob_args = malloc((arg_count + 1) * sizeof(char *));
        if (glob_args) {
            for (int i = 0; i < arg_count; i++) {
                glob_args[i] = glob_input[i];
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

    // Use expanded args if glob expansion happened, otherwise use IFS-split args (or original)
    char **exec_input = glob_expanded ? glob_args : glob_input;

    // Note: Don't strip quote markers here - they're needed for redirect parsing in launch()
    // Markers will be stripped in launch() after redirections are parsed

    // Handle variable assignments
    // Count leading VAR=VALUE assignments
    int prefix_count = 0;
    while (exec_input[prefix_count] && is_var_assignment(exec_input[prefix_count])) {
        prefix_count++;
    }

    if (prefix_count > 0 && exec_input[prefix_count] == NULL) {
        // Only variable assignments, no command - set variables in shell
        int assignment_failed = 0;
        for (int i = 0; i < prefix_count; i++) {
            char *equals = is_var_assignment(exec_input[i]);
            *equals = '\0';
            const char *name = exec_input[i];
            char *value = equals + 1;

            // Strip quote markers from value before storing
            strip_quote_markers(value);

            // Tilde expansion already happened BEFORE command substitution
            // (in the earlier expansion phase), so just use the value directly
            int result = shellvar_set(name, value);
            if (result < 0) {
                assignment_failed = 1;
                // In non-interactive mode, readonly assignment error should exit
                // In interactive mode, continue processing
                if (!is_interactive) {
                    *equals = '=';  // Restore
                    last_command_exit_code = 1;
                    if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
                    if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
                    free_expanded_args(expanded_args, expanded_count);
                    return 0;  // Signal to exit shell
                }
            }
            *equals = '=';  // Restore
        }
        // For variable-only assignments, POSIX says the exit code should be
        // the exit code of the last command substitution, or 0 if none/failed assignment
        if (assignment_failed) {
            last_command_exit_code = 1;
        } else {
            // Use the exit code from command substitution if any occurred
            last_command_exit_code = cmdsub_get_last_exit_code();
        }
        if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        return 1;
    }

    // If there are prefix assignments followed by a command,
    // set them temporarily for the command (both in environment and shell table)
    bool has_prefix_assignments = (prefix_count > 0 && exec_input[prefix_count] != NULL);
    if (has_prefix_assignments) {
        for (int i = 0; i < prefix_count; i++) {
            char *equals = is_var_assignment(exec_input[i]);
            *equals = '\0';
            const char *name = exec_input[i];
            char *value = equals + 1;

            // Strip quote markers from value before storing
            strip_quote_markers(value);

            // Save old value for restoration
            save_prefix_var(name);

            // Set in environment and shell table for visibility to command
            set_prefix_var(name, value);

            *equals = '=';  // Restore
        }
        // Shift exec_input to point to the actual command
        exec_input = &exec_input[prefix_count];
    }

    int result = 1;  // Default: continue shell

    // Check if command is an alias (use original args[0] for alias lookup)
    const char *alias_value = config_get_alias(exec_input[0]);
    if (alias_value) {
        // Expand alias by parsing the alias value
        char *alias_line = strdup(alias_value);
        if (!alias_line) {
            last_command_exit_code = 1;
            if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            restore_prefix_vars();
            return 1;
        }

        ParseResult alias_parsed = parse_line(alias_line);
        if (!alias_parsed.tokens) {
            free(alias_line);
            last_command_exit_code = 1;
            if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            restore_prefix_vars();
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
            while (alias_parsed.tokens[alias_arg_count] != NULL) {
                alias_arg_count++;
            }

            // Create new args array with alias + original args
            char **combined_args = malloc((alias_arg_count + orig_arg_count + 1) * sizeof(char*));
            if (combined_args) {
                // Copy alias args
                for (int i = 0; i < alias_arg_count; i++) {
                    combined_args[i] = alias_parsed.tokens[i];
                }
                // Append original args (skip command name)
                for (int i = 0; i < orig_arg_count; i++) {
                    combined_args[alias_arg_count + i] = exec_input[i + 1];
                }
                combined_args[alias_arg_count + orig_arg_count] = NULL;

                // Execute with combined args
                result = execute(combined_args);

                free(combined_args);
                parse_result_free(&alias_parsed);
                free(alias_line);
                if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
                if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
                free_expanded_args(expanded_args, expanded_count);
                restore_prefix_vars();
                return result;
            }
        }

        // No original args, just execute alias
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: Executing alias '%s' -> '%s'\n", exec_input[0], alias_value);
        fprintf(stderr, "DEBUG: Before recursive execute, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        result = execute(alias_parsed.tokens);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After recursive execute, last_command_exit_code=%d, result=%d\n", last_command_exit_code, result);
#endif
        parse_result_free(&alias_parsed);
        free(alias_line);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After freeing alias stuff, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: After free_expanded_args, last_command_exit_code=%d\n", last_command_exit_code);
#endif
        restore_prefix_vars();
        return result;
    }

    // Check for redirections
    RedirInfo *redir = redirect_parse(exec_input);

    // Set heredoc content if pending
    const char *heredoc = script_get_pending_heredoc();
    if (heredoc && redir) {
        redirect_set_heredoc_content(redir, heredoc, script_get_pending_heredoc_quoted());
    }

    char **exec_args = redir ? redir->args : exec_input;

    // Check if this is a builtin first (without executing it)
    int is_builtin_cmd = exec_args[0] ? is_builtin(exec_args[0]) : 0;

    // Strip quote markers only for builtins
    // External commands go through launch() which does its own redirect parsing and marker stripping
    if (is_builtin_cmd) {
        strip_quote_markers_args(exec_args);
    }

    // Check if this is a builtin that must NOT run in a child process
    // These include:
    // - Flow control: break, continue, return, exit (affect execution flow)
    // - State modifying: read, export, unset, set, cd, alias, unalias,
    //   readonly, eval, exec, source, ., trap (modify shell state/variables)
    bool is_special_builtin = false;
    if (exec_args[0]) {
        is_special_builtin = (strcmp(exec_args[0], ":") == 0 ||
                              strcmp(exec_args[0], "break") == 0 ||
                              strcmp(exec_args[0], "continue") == 0 ||
                              strcmp(exec_args[0], "return") == 0 ||
                              strcmp(exec_args[0], "exit") == 0 ||
                              strcmp(exec_args[0], "set") == 0 ||
                              strcmp(exec_args[0], "read") == 0 ||
                              strcmp(exec_args[0], "export") == 0 ||
                              strcmp(exec_args[0], "unset") == 0 ||
                              strcmp(exec_args[0], "readonly") == 0 ||
                              strcmp(exec_args[0], "cd") == 0 ||
                              strcmp(exec_args[0], "alias") == 0 ||
                              strcmp(exec_args[0], "unalias") == 0 ||
                              strcmp(exec_args[0], "eval") == 0 ||
                              strcmp(exec_args[0], "exec") == 0 ||
                              strcmp(exec_args[0], "source") == 0 ||
                              strcmp(exec_args[0], ".") == 0 ||
                              strcmp(exec_args[0], "trap") == 0);
    }

    // If it's a builtin with redirections (but NOT special), run in child process
    if (is_builtin_cmd && redir && redir->count > 0 && !is_special_builtin) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process - apply redirections and run builtin
            if (redirect_apply(redir) != 0) {
                _exit(EXIT_FAILURE);
            }
            try_builtin(exec_args);
            // The builtin sets last_command_exit_code, use that as exit code
            redirect_free(redir);
            // Flush child's stdio buffers before exit
            fflush(stdout);
            fflush(stderr);
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
        if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        restore_prefix_vars();
        return 1;
    }

    // For special builtins with redirections, handle in same process
    // Save and restore file descriptors (except for exec which persists redirections)
    int saved_fds[3] = {-1, -1, -1};  // stdin, stdout, stderr
    bool is_exec_builtin = exec_args[0] && strcmp(exec_args[0], "exec") == 0;

    // For exec builtin, pass original args (with all redirections) so it can process them
    // in the correct order. Don't use redirect_apply which would apply in wrong order.
    if (is_exec_builtin) {
        // shell_exec handles ALL redirections itself, in order
        result = try_builtin(exec_input);
        if (result != -1) {
            redirect_free(redir);
            if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            restore_prefix_vars();
            return result;
        }
    }

    if (is_special_builtin && redir && redir->count > 0 && !is_exec_builtin) {
        // Save current file descriptors
        saved_fds[0] = dup(STDIN_FILENO);
        saved_fds[1] = dup(STDOUT_FILENO);
        saved_fds[2] = dup(STDERR_FILENO);
        // Apply redirections - check for errors
        if (redirect_apply(redir) != 0) {
            // Restore file descriptors
            if (saved_fds[0] != -1) { dup2(saved_fds[0], STDIN_FILENO); close(saved_fds[0]); }
            if (saved_fds[1] != -1) { dup2(saved_fds[1], STDOUT_FILENO); close(saved_fds[1]); }
            if (saved_fds[2] != -1) { dup2(saved_fds[2], STDERR_FILENO); close(saved_fds[2]); }
            redirect_free(redir);
            if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
            if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
            free_expanded_args(expanded_args, expanded_count);
            restore_prefix_vars();
            last_command_exit_code = 1;
            // Special builtin redirect error: exit non-interactive shell
            return is_interactive ? 1 : 0;
        }
    }

    // Try built-in commands (no redirections, or will be handled below)
    if (!is_exec_builtin) {
        result = try_builtin(exec_args);
    }

    // Restore file descriptors if we saved them
    if (saved_fds[0] != -1 || saved_fds[1] != -1 || saved_fds[2] != -1) {
        if (saved_fds[0] != -1) { dup2(saved_fds[0], STDIN_FILENO); close(saved_fds[0]); }
        if (saved_fds[1] != -1) { dup2(saved_fds[1], STDOUT_FILENO); close(saved_fds[1]); }
        if (saved_fds[2] != -1) { dup2(saved_fds[2], STDERR_FILENO); close(saved_fds[2]); }
    }

    if (result != -1) {
        redirect_free(redir);
        if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        restore_prefix_vars();
        return result;
    }

    // Check for user-defined functions
    const ShellFunction *func = script_get_function(exec_args[0]);
    if (func) {
        // Count arguments (including function name as $0)
        int argc = 0;
        while (exec_args[argc]) argc++;

        // Apply redirections for function calls (save/restore FDs)
        int func_saved_fds[3] = {-1, -1, -1};
        if (redir && redir->count > 0) {
            func_saved_fds[0] = dup(STDIN_FILENO);
            func_saved_fds[1] = dup(STDOUT_FILENO);
            func_saved_fds[2] = dup(STDERR_FILENO);
            if (redirect_apply(redir) != 0) {
                // Restore on error
                if (func_saved_fds[0] != -1) { dup2(func_saved_fds[0], STDIN_FILENO); close(func_saved_fds[0]); }
                if (func_saved_fds[1] != -1) { dup2(func_saved_fds[1], STDOUT_FILENO); close(func_saved_fds[1]); }
                if (func_saved_fds[2] != -1) { dup2(func_saved_fds[2], STDERR_FILENO); close(func_saved_fds[2]); }
                redirect_free(redir);
                if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
                if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
                free_expanded_args(expanded_args, expanded_count);
                restore_prefix_vars();
                last_command_exit_code = 1;
                return 1;
            }
        }

        result = script_execute_function(func, argc, exec_args);

        // Restore file descriptors after function execution
        if (func_saved_fds[0] != -1 || func_saved_fds[1] != -1 || func_saved_fds[2] != -1) {
            if (func_saved_fds[0] != -1) { dup2(func_saved_fds[0], STDIN_FILENO); close(func_saved_fds[0]); }
            if (func_saved_fds[1] != -1) { dup2(func_saved_fds[1], STDOUT_FILENO); close(func_saved_fds[1]); }
            if (func_saved_fds[2] != -1) { dup2(func_saved_fds[2], STDERR_FILENO); close(func_saved_fds[2]); }
        }

        // Note: Do NOT set last_command_exit_code here - it's already set
        // by shell_return (or the last command in the function body).
        // The result is a control flow signal (1=continue, 0=exit, -2=return, etc.)
        redirect_free(redir);
        if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
        if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
        free_expanded_args(expanded_args, expanded_count);
        restore_prefix_vars();
        return result;
    }

    // Free redir before launching external command (launch() does its own redirect parsing)
    redirect_free(redir);

    // Build command string for job display
    char *cmd_string = build_cmd_string(exec_input);

    // Launch external program
    result = launch(exec_input, cmd_string);

    free(cmd_string);
    if (ifs_expanded) free_glob_args(ifs_args, ifs_arg_count);
    if (glob_expanded) free_glob_args(glob_args, glob_arg_count);
    free_expanded_args(expanded_args, expanded_count);
    restore_prefix_vars();

    return result;
}

// Get last exit code
int execute_get_last_exit_code(void) {
    return last_command_exit_code;
}
