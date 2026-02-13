#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>
#include <signal.h>
#include "hash.h"
#include "builtins.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "execute.h"
#include "history.h"
#include "jobs.h"
#include "test_builtin.h"
#include "script.h"
#include "chain.h"
#include "parser.h"
#include "safe_string.h"
#include "update.h"
#include "shellvar.h"
#include "trap.h"

extern int last_command_exit_code;

// Track if this is a login shell (set from main)
static bool is_login_shell = false;

void builtins_set_login_shell(bool is_login) {
    is_login_shell = is_login;
}

// Builtin command names and functions
Builtin builtins[BUILTIN_FUNC_MAX] = {
    [BUILTIN_FUNC_CD]               = (Builtin){"cd",           &shell_cd},
    [BUILTIN_FUNC_EXIT]             = (Builtin){"exit",         &shell_exit},
    [BUILTIN_FUNC_ALIAS]            = (Builtin){"alias",        &shell_alias},
    [BUILTIN_FUNC_UNALIAS]          = (Builtin){"unalias",      &shell_unalias},
    [BUILTIN_FUNC_SOURCE]           = (Builtin){"source",       &shell_source},
    [BUILTIN_FUNC_DOT]              = (Builtin){".",            &shell_source},         // Alias for source (POSIX)
    [BUILTIN_FUNC_EXPORT]           = (Builtin){"export",       &shell_export},
    [BUILTIN_FUNC_SET]              = (Builtin){"set",          &shell_set},
    [BUILTIN_FUNC_HISTORY]          = (Builtin){"history",      &shell_history},
    [BUILTIN_FUNC_JOBS]             = (Builtin){"jobs",         &shell_jobs},
    [BUILTIN_FUNC_FG]               = (Builtin){"fg",           &shell_fg},
    [BUILTIN_FUNC_BG]               = (Builtin){"bg",           &shell_bg},
    [BUILTIN_FUNC_LOGOUT]           = (Builtin){"logout",       &shell_logout},
    [BUILTIN_FUNC_TEST]             = (Builtin){"test",         &shell_test},
    [BUILTIN_FUNC_BRACKET]          = (Builtin){"[",            &shell_bracket},
    [BUILTIN_FUNC_DOUBLE_BRACKET]   = (Builtin){"[[",           &shell_double_bracket}, // Bash-style extended test
    [BUILTIN_FUNC_UNSET]            = (Builtin){"unset",        &shell_unset},
    [BUILTIN_FUNC_TRUE]             = (Builtin){"true",         &shell_true},
    [BUILTIN_FUNC_FALSE]            = (Builtin){"false",        &shell_false},
    [BUILTIN_FUNC_COLON]            = (Builtin){":",            &shell_colon},
    [BUILTIN_FUNC_ECHO]             = (Builtin){"echo",         &shell_echo},
    [BUILTIN_FUNC_READ]             = (Builtin){"read",         &shell_read},
    [BUILTIN_FUNC_RETURN]           = (Builtin){"return",       &shell_return},
    [BUILTIN_FUNC_BREAK]            = (Builtin){"break",        &shell_break},
    [BUILTIN_FUNC_CONTINUE]         = (Builtin){"continue",     &shell_continue_cmd},
    [BUILTIN_FUNC_EVAL]             = (Builtin){"eval",         &shell_eval},
    [BUILTIN_FUNC_UPDATE]           = (Builtin){"update",       &shell_update},
    [BUILTIN_FUNC_COMMAND]          = (Builtin){"command",      &shell_command},
    [BUILTIN_FUNC_EXEC]             = (Builtin){"exec",         &shell_exec},
    [BUILTIN_FUNC_TIMES]            = (Builtin){"times",        &shell_times},
    [BUILTIN_FUNC_TYPE]             = (Builtin){"type",         &shell_type},
    [BUILTIN_FUNC_READONLY]         = (Builtin){"readonly",     &shell_readonly},
    [BUILTIN_FUNC_TRAP]             = (Builtin){"trap",         &shell_trap},
    [BUILTIN_FUNC_WAIT]             = (Builtin){"wait",         &shell_wait},
    [BUILTIN_FUNC_KILL]             = (Builtin){"kill",         &shell_kill},
    [BUILTIN_FUNC_HASH]             = (Builtin){"hash",         &shell_hash},
};

// Parse job ID from argument (handles %n, %%, %+, %-, n)
static int parse_job_id(const char *arg) {
    if (!arg) return 0;  // No arg means current job

    while (isspace(*arg)) arg++;

    if (*arg == '%') {
        arg++;
        if (*arg == '%' || *arg == '+' || *arg == '\0') {
            return 0;
        } else if (*arg == '-') {
            return 0;
        } else if (isdigit(*arg)) {
            return atoi(arg);
        }
    } else if (isdigit(*arg)) {
        return atoi(arg);
    }

    return -1;
}

// ============================================================================
// Core Builtins
// ============================================================================

int shell_cd(char **args) {
    const char *path = args[1];

    // Handle cd - (go to previous directory)
    if (path != NULL && strcmp(path, "-") == 0) {
        path = getenv("OLDPWD");
        if (!path) {
            color_error("%s: cd: OLDPWD not set", HASH_NAME);
            last_command_exit_code = 1;
            return 1;
        }
        // Print the directory when using cd -
        printf("%s\n", path);
    }

    if (path == NULL) {
        path = getenv("HOME");

        if (!path) {
            struct passwd pw;
            struct passwd *result = NULL;
            char buf[1024];

            if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
                path = pw.pw_dir;
            } else {
                color_error("%s: could not determine home directory", HASH_NAME);
                last_command_exit_code = 1;
                return 1;
            }
        }
    }

    // Save current directory as OLDPWD before changing
    char oldpwd[PATH_MAX];
    if (getcwd(oldpwd, sizeof(oldpwd)) != NULL) {
        setenv("OLDPWD", oldpwd, 1);
    }

    if (chdir(path) != 0) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
    } else {
        // Update PWD after successful change
        char newpwd[PATH_MAX];
        if (getcwd(newpwd, sizeof(newpwd)) != NULL) {
            setenv("PWD", newpwd, 1);
        }
        last_command_exit_code = 0;
    }

    return 1;
}

int shell_exit(char **args) {
    int exit_code = 0;

    // Check for exit code argument
    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "%s: exit: %s: numeric argument required\n", HASH_NAME, args[1]);
            exit_code = 2;
        } else {
            exit_code = (int)(val & 0xFF);  // Exit codes are 0-255
        }
    }

    // Check for running jobs (only relevant in interactive mode)
    int job_count = jobs_count();
    if (job_count > 0 && isatty(STDIN_FILENO)) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'exit' again to force exit, or 'jobs' to see them.\n");

        static int exit_attempted = 0;
        if (exit_attempted) {
            exit_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = exit_code;
            script_state.exit_requested = true;
            return 0;
        }
        exit_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    // Only print goodbye message in interactive mode
    if (isatty(STDIN_FILENO)) {
        fprintf(stdout, "Bye :)\n");
    }
    last_command_exit_code = exit_code;
    script_state.exit_requested = true;  // Mark that exit was explicitly called
    return 0;
}

int shell_alias(char **args) {
    if (args[1] == NULL) {
        config_list_aliases();
        last_command_exit_code = 0;
        return 1;
    }

    char *equals = strchr(args[1], '=');
    if (equals) {
        *equals = '\0';
        const char *name = args[1];
        char *value = equals + 1;

        if ((value[0] == '"' || value[0] == '\'') &&
            value[0] == value[strlen(value) - 1]) {
            value[strlen(value) - 1] = '\0';
            value++;
        }

        if (config_add_alias(name, value) == 0) {
            last_command_exit_code = 0;
        } else {
            color_error("Failed to add alias");
            last_command_exit_code = 1;
        }
    } else {
        const char *value = config_get_alias(args[1]);
        if (value) {
            printf("alias %s='%s'\n", args[1], value);
            last_command_exit_code = 0;
        } else {
            fprintf(stderr, "%s: alias: %s: not found\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
        }
    }

    return 1;
}

int shell_unalias(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "%s: unalias: usage: unalias name\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    if (config_remove_alias(args[1]) == 0) {
        last_command_exit_code = 0;
    } else {
        fprintf(stderr, "%s: unalias: %s: not found\n", HASH_NAME, args[1]);
        last_command_exit_code = 1;
    }

    return 1;
}

int shell_source(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "%s: %s: filename argument required\n", HASH_NAME, args[0]);
        last_command_exit_code = 2;
        return 1;
    }

    const char *filepath = args[1];
    char resolved_path[PATH_MAX];

    // POSIX: If filename doesn't contain '/', search PATH for it
    // Note: POSIX says to use PATH only, NOT to check CWD first
    if (strchr(filepath, '/') == NULL) {
        // Use shellvar_get to see shell variables (which may not be in env)
        const char *path_env = shellvar_get("PATH");
        bool found = false;

        if (path_env && *path_env) {
            char *path_copy = strdup(path_env);
            if (path_copy) {
                char *saveptr;
                char *dir = strtok_r(path_copy, ":", &saveptr);

                while (dir) {
                    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", dir, filepath);
                    // Check if file exists and is readable
                    if (access(resolved_path, R_OK) == 0) {
                        filepath = resolved_path;
                        found = true;
                        break;
                    }
                    dir = strtok_r(NULL, ":", &saveptr);
                }
                free(path_copy);
            }
        }

        if (!found) {
            // File not found in PATH
            fprintf(stderr, "%s: %s: not found\n", args[0], args[1]);
            last_command_exit_code = 1;
            // Special builtin: non-interactive shell should exit on error
            return is_interactive ? 1 : 0;
        }
    } else {
        // Filepath contains '/' - check if it exists
        if (access(filepath, R_OK) != 0) {
            fprintf(stderr, "%s: %s: not found\n", args[0], args[1]);
            last_command_exit_code = 1;
            // Special builtin: non-interactive shell should exit on error
            return is_interactive ? 1 : 0;
        }
    }

    // Execute the script file
    // If we're already in silent mode (e.g., sourcing system files),
    // continue in silent mode for nested sources
    // Note: break/continue have lexical scope - they don't propagate from sourced files
    int result = script_execute_file_ex(filepath, 0, NULL, script_state.silent_errors);
    last_command_exit_code = result;

    return 1;
}

int shell_export(char **args) {
    if (args[1] == NULL) {
        shellvar_list_exported();
        last_command_exit_code = 0;
        return 1;
    }

    // Handle export -p option (same as export with no args)
    if (strcmp(args[1], "-p") == 0) {
        shellvar_list_exported();
        last_command_exit_code = 0;
        return 1;
    }

    for (int i = 1; args[i] != NULL; i++) {
        char *equals = strchr(args[i], '=');
        if (!equals) {
            // Just marking for export - mark variable for export
            if (shellvar_is_readonly(args[i])) {
                // It's OK to export a readonly variable
            }
            shellvar_set_export(args[i]);
            last_command_exit_code = 0;
            continue;
        }

        *equals = '\0';
        const char *name = args[i];
        const char *value = equals + 1;

        // Check if readonly
        if (shellvar_is_readonly(name)) {
            fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, name);
            last_command_exit_code = 1;
            *equals = '=';  // Restore for next iteration
            return 0;  // Exit shell in non-interactive mode per POSIX
        }

        // Set and export the variable
        if (shellvar_set(name, value) == 0) {
            shellvar_set_export(name);
            setenv(name, value, 1);
            last_command_exit_code = 0;
        } else {
            last_command_exit_code = 1;
            *equals = '=';
            return 0;  // Exit shell on error
        }
        *equals = '=';  // Restore
    }

    return 1;
}

int shell_history(char **args) {
    if (args[1] != NULL) {
        if (strcmp(args[1], "-c") == 0) {
            history_clear();
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-w") == 0) {
            history_save();
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-r") == 0) {
            history_clear();
            history_load();
            last_command_exit_code = 0;
            return 1;
        }
    }

    int count = history_count();
    for (int i = 0; i < count; i++) {
        const char *cmd = history_get(i);
        if (cmd) {
            printf("%5d  %s\n", i, cmd);
        }
    }

    last_command_exit_code = 0;
    return 1;
}

static void handle_positional_arguments(char **args) {
    int argc = 0;
    // Count arguments
    for (argc = 0; args[argc] != NULL; ++argc);

    // Set positional parameters ($1, $2, ...)
    script_set_positional_params(argc, argc > 0 ? args : NULL);
    last_command_exit_code = 0;
}

// Handle POSIX shell options: -u, +u, -m, +m, -o option, +o option, etc.
// Returns true if arg was handled and last_command_exit_code was set.
static bool handle_posix_shell_options(char **args, int *i) {
    const char *arg = args[*i];
    if (strcmp(arg, "-u") == 0) {
        shell_option_set_nounset(true);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "+u") == 0) {
        shell_option_set_nounset(false);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "-e") == 0) {
        shell_option_set_errexit(true);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "+e") == 0) {
        shell_option_set_errexit(false);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "-m") == 0) {
        shell_option_set_monitor(true);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "+m") == 0) {
        shell_option_set_monitor(false);
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "-o") == 0 && args[(*i) + 1] != NULL) {
        // Handle -o option_name
        const char *opt = args[++(*i)];
        if (strcmp(opt, "nounset") == 0) {
            shell_option_set_nounset(true);
        } else if (strcmp(opt, "errexit") == 0) {
            shell_option_set_errexit(true);
        } else if (strcmp(opt, "monitor") == 0) {
            shell_option_set_monitor(true);
        } else if (strcmp(opt, "nonlexicalctrl") == 0) {
            shell_option_set_nonlexicalctrl(true);
        } else if (strcmp(opt, "nolog") == 0) {
            shell_option_set_nolog(true);
        } else {
            // POSIX: unknown option is an error
            color_error("%s: set: %s: invalid option name", HASH_NAME, opt);
            last_command_exit_code = 1;
            return true;
        }
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "+o") == 0 && args[(*i) + 1] != NULL) {
        // Handle +o option_name (disable option)
        const char *opt = args[++(*i)];
        if (strcmp(opt, "nounset") == 0) {
            shell_option_set_nounset(false);
        } else if (strcmp(opt, "errexit") == 0) {
            shell_option_set_errexit(false);
        } else if (strcmp(opt, "monitor") == 0) {
            shell_option_set_monitor(false);
        } else if (strcmp(opt, "nonlexicalctrl") == 0) {
            shell_option_set_nonlexicalctrl(false);
        } else if (strcmp(opt, "nolog") == 0) {
            shell_option_set_nolog(false);
        } else {
            // POSIX: unknown option is an error
            color_error("%s: set: %s: invalid option name", HASH_NAME, opt);
            last_command_exit_code = 1;
            return true;
        }
        last_command_exit_code = 0;
        return true;
    }

    return false;
}

// Returns true if arg was handled and last_command_exit_code was set.
static bool handle_hash_shell_options(const char *arg) {
    // Handle colors option
    if (strcmp(arg, "colors=on") == 0) {
        shell_config.colors_enabled = true;
        colors_enable();
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "colors=off") == 0) {
        shell_config.colors_enabled = false;
        colors_disable();
        last_command_exit_code = 0;
        return true;
    }

    // Handle welcome option
    if (strcmp(arg, "welcome=on") == 0) {
        shell_config.show_welcome = true;
        last_command_exit_code = 0;
        return true;
    } else if (strcmp(arg, "welcome=off") == 0) {
        shell_config.show_welcome = false;
        last_command_exit_code = 0;
        return true;
    }

    // Handle PS1 setting
    if (strncmp(arg, "PS1=", 4) == 0) {
        const char *ps1_value = arg + 4;

        // Remove quotes if present
        size_t val_len = strlen(ps1_value);
        if (val_len >= 2 &&
            (ps1_value[0] == '"' || ps1_value[0] == '\'') &&
            ps1_value[0] == ps1_value[val_len - 1]) {
            // Make a mutable copy to remove quotes
            char *temp = strdup(ps1_value);
            if (temp) {
                temp[val_len - 1] = '\0';
                prompt_set_ps1(temp + 1);
                free(temp);
            }
        } else {
            prompt_set_ps1(ps1_value);
        }
        last_command_exit_code = 0;
        return true;
    }

    return false;
}

int shell_set(char **args) {
    // Handle set with no arguments - list all shell variables
    if (args[1] == NULL) {
        shellvar_list_all();
        last_command_exit_code = 0;
        return 1;
    }

    // Handle set option=value syntax for hash-specific options
    for (int i = 1; args[i] != NULL; i++) {
        const char *arg = args[i];

        // Handle -- to set positional parameters
        // set -- arg1 arg2 ... sets $1, $2, etc.
        // set -- (with no args) clears positional parameters
        if (strcmp(arg, "--") == 0) {
            // All remaining arguments are used as positional arguments
            handle_positional_arguments(&args[i + 1]);
            return 1;
        }

        // POSIX: If argument doesn't start with - or +, and isn't an option=value,
        // treat it and all following arguments as positional parameters
        if (arg[0] != '-' && arg[0] != '+' && !strchr(arg, '=')) {
            // arguments from this point are used as positional arguments
            handle_positional_arguments(&args[i]);
            return 1;
        }

        // Handle POSIX shell options: -u, +u, -m, +m, -o option, +o option, etc.
        if (handle_posix_shell_options(args, &i)) {
            // On error it sets last_command_exit code to 1
            if (last_command_exit_code == 1) {
                return 1;
            }
            continue;
        }

        if (handle_hash_shell_options(arg)) {
            continue;
        }

        // Unknown option - silently ignore for compatibility
        // (bash 'set' has many options we don't support)
    }

    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// Job Control Builtins
// ============================================================================

int shell_jobs(char **args) {
    JobsFormat format = JOBS_FORMAT_DEFAULT;

    // Parse options
    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "-l") == 0) {
            format = JOBS_FORMAT_LONG;
        } else if (strcmp(args[i], "-p") == 0) {
            format = JOBS_FORMAT_PID_ONLY;
        }
    }

    jobs_list(format);
    last_command_exit_code = 0;
    return 1;
}

int shell_fg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        fprintf(stderr, "%s: fg: %s: no such job\n", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_foreground(job_id);
    last_command_exit_code = (result == -1) ? 1 : result;
    return 1;
}

int shell_bg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        fprintf(stderr, "%s: bg: %s: no such job\n", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_background(job_id);
    last_command_exit_code = (result == -1) ? 1 : 0;
    return 1;
}

int shell_logout(char **args) {
    (void)args;

    if (!is_login_shell) {
        fprintf(stderr, "%s: logout: not login shell: use `exit'\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    int job_count = jobs_count();
    if (job_count > 0 && isatty(STDIN_FILENO)) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'logout' again to force logout, or 'jobs' to see them.\n");

        static int logout_attempted = 0;
        if (logout_attempted) {
            logout_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = 0;
            return 0;
        }
        logout_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    // Only print goodbye message in interactive mode
    if (isatty(STDIN_FILENO)) {
        fprintf(stdout, "Bye :)\n");
    }
    last_command_exit_code = 0;
    return 0;
}

// ============================================================================
// Test and Conditional Builtins
// ============================================================================

int shell_test(char **args) {
    last_command_exit_code = builtin_test(args);
    return 1;
}

int shell_bracket(char **args) {
    last_command_exit_code = builtin_bracket(args);
    return 1;
}

int shell_double_bracket(char **args) {
    last_command_exit_code = builtin_double_bracket(args);
    return 1;
}

// ============================================================================
// Variable Management Builtins
// ============================================================================

int shell_unset(char **args) {
    if (args[1] == NULL) {
        last_command_exit_code = 0;
        return 1;
    }

    bool unset_var = true;
    bool unset_func = false;
    int start = 1;

    // Check for -v (variable) or -f (function) flags
    if (args[1][0] == '-') {
        if (strcmp(args[1], "-v") == 0) {
            unset_var = true;
            unset_func = false;
            start = 2;
        } else if (strcmp(args[1], "-f") == 0) {
            unset_var = false;
            unset_func = true;
            start = 2;
        }
    }

    int error = 0;
    for (int i = start; args[i] != NULL; i++) {
        if (unset_var) {
            // Check for readonly variable
            if (shellvar_unset(args[i]) != 0) {
                // shellvar_unset prints error for readonly
                error = 1;
                // In non-interactive mode, exit shell per POSIX
                // In interactive mode, continue processing
                if (!is_interactive) {
                    last_command_exit_code = 1;
                    return 0;  // Exit shell
                }
            }
        }
        if (unset_func) {
            // TODO: Implement function unset when functions are fully implemented
        }
    }

    last_command_exit_code = error ? 1 : 0;
    return 1;
}

// ============================================================================
// Simple Utility Builtins
// ============================================================================

int shell_true(char **args) {
    (void)args;
    last_command_exit_code = 0;
    return 1;
}

int shell_false(char **args) {
    (void)args;
    last_command_exit_code = 1;
    return 1;
}

int shell_colon(char **args) {
    (void)args;
    last_command_exit_code = 0;
    return 1;
}

int shell_echo(char **args) {
    bool newline = true;
    bool interpret_escapes = false;
    int start = 1;
    int write_error = 0;

    // Handle options
    while (args[start] != NULL && args[start][0] == '-') {
        if (strcmp(args[start], "-n") == 0) {
            newline = false;
            start++;
        } else if (strcmp(args[start], "-e") == 0) {
            interpret_escapes = true;
            start++;
        } else if (strcmp(args[start], "-E") == 0) {
            interpret_escapes = false;
            start++;
        } else {
            break;  // Unknown option, treat as argument
        }
    }

    // Print arguments
    for (int i = start; args[i] != NULL; i++) {
        if (i > start) {
            if (putchar(' ') == EOF) write_error = 1;
        }

        if (interpret_escapes) {
            // Interpret escape sequences
            const char *s = args[i];
            while (*s) {
                if (*s == '\\' && *(s + 1)) {
                    s++;
                    switch (*s) {
                        case 'n': if (putchar('\n') == EOF) write_error = 1; break;
                        case 't': if (putchar('\t') == EOF) write_error = 1; break;
                        case 'r': if (putchar('\r') == EOF) write_error = 1; break;
                        case '\\': if (putchar('\\') == EOF) write_error = 1; break;
                        case 'a': if (putchar('\a') == EOF) write_error = 1; break;
                        case 'b': if (putchar('\b') == EOF) write_error = 1; break;
                        case 'f': if (putchar('\f') == EOF) write_error = 1; break;
                        case 'v': if (putchar('\v') == EOF) write_error = 1; break;
                        case 'c': goto done;  // Stop output
                        default:
                            if (putchar('\\') == EOF) write_error = 1;
                            if (putchar(*s) == EOF) write_error = 1;
                            break;
                    }
                } else {
                    if (putchar(*s) == EOF) write_error = 1;
                }
                s++;
            }
        } else {
            if (printf("%s", args[i]) < 0) write_error = 1;
        }
    }

done:
    if (newline) {
        if (putchar('\n') == EOF) write_error = 1;
    }

    // Check for write errors on flush
    if (fflush(stdout) == EOF) write_error = 1;

    last_command_exit_code = write_error ? 1 : 0;
    return 1;
}

int shell_read(char **args) {
    // Basic read implementation
    // Usage: read [-r] [var ...]

    bool raw = false;
    int start = 1;

    if (args[start] && strcmp(args[start], "-r") == 0) {
        raw = true;
        start++;
    }

    // Read a line from stdin
    char line[4096];
    if (fgets(line, sizeof(line), stdin) == NULL) {
        last_command_exit_code = 1;
        return 1;
    }

    // Remove trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }

    // If no variables specified, use REPLY
    if (args[start] == NULL) {
        setenv("REPLY", line, 1);
        last_command_exit_code = 0;
        return 1;
    }

    // Split line into words and assign to variables
    char *saveptr;
    // TODO: Use the IFS
    const char *word = strtok_r(line, " \t", &saveptr);
    int i = start;

    while (args[i] != NULL) {
        if (args[i + 1] == NULL) {
            // Last variable gets the rest of the line
            if (word) {
                char rest[4096];
                safe_strcpy(rest, word, sizeof(rest));

                const char *remaining = strtok_r(NULL, "", &saveptr);
                if (remaining) {
                    safe_strcat(rest, " ", sizeof(rest));
                    safe_strcat(rest, remaining, sizeof(rest));
                }
                setenv(args[i], rest, 1);
            } else {
                setenv(args[i], "", 1);
            }
        } else {
            setenv(args[i], word ? word : "", 1);
            if (word) {
                // TODO: Use the IFS
                word = strtok_r(NULL, " \t", &saveptr);
            }
        }
        i++;
    }

    (void)raw;  // TODO: Handle -r flag for backslash escapes

    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// Control Flow Builtins
// ============================================================================

int shell_return(char **args) {
    // POSIX: If no argument is provided, use the exit status of the last command
    int return_code = last_command_exit_code;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "%s: return: %s: numeric argument required\n", HASH_NAME, args[1]);
            return_code = 2;
        } else {
            return_code = (int)(val & 0xFF);
        }
    }

    // If not in a function or sourced script, this is an error
    if (!script_state.in_script) {
        fprintf(stderr, "%s: return: can only `return' from a function or sourced script\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    last_command_exit_code = return_code;
    // Set return pending flag for condition evaluation
    script_set_return_pending(true);
    // Signal to caller to return
    return -2;  // Special return code for "return from function"
}

int shell_break(char **args) {
    int levels = 1;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || val < 1) {
            fprintf(stderr, "%s: break: %s: numeric argument required\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
            return 1;
        }
        levels = (int)val;
    }

    // POSIX: Check if we're in a loop (dynamic scoping across function boundaries)
    int available_loops = script_count_loops_at_current_depth();
    if (available_loops == 0) {
        fprintf(stderr, "%s: break: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    // If requesting more levels than available, just break all available
    if (levels > available_loops) {
        levels = available_loops;
    }

    // Set pending break levels for dynamic scoping propagation
    script_set_break_pending(levels);

    last_command_exit_code = 0;
    return -3;  // Special return code for "break from loop"
}

int shell_continue_cmd(char **args) {
    int levels = 1;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || val < 1) {
            fprintf(stderr, "%s: continue: %s: numeric argument required\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
            return 1;
        }
        levels = (int)val;
    }

    // POSIX: Check if we're in a loop (dynamic scoping across function boundaries)
    int available_loops = script_count_loops_at_current_depth();
    if (available_loops == 0) {
        fprintf(stderr, "%s: continue: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    // If requesting more levels than available, just continue the outermost available
    if (levels > available_loops) {
        levels = available_loops;
    }

    // Set pending continue levels for dynamic scoping propagation
    script_set_continue_pending(levels);

    last_command_exit_code = 0;
    return -4;  // Special return code for "continue loop"
}

int shell_eval(char **args) {
    if (args[1] == NULL) {
        last_command_exit_code = 0;
        return 1;
    }

    // Concatenate all arguments with spaces
    size_t total_len = 0;
    for (int i = 1; args[i] != NULL; i++) {
        total_len += strlen(args[i]) + 1;  // +1 for space or null
    }

    char *cmd = malloc(total_len);
    if (!cmd) {
        last_command_exit_code = 1;
        return 1;
    }

    cmd[0] = '\0';
    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) {
            strcat(cmd, " ");
        }
        strcat(cmd, args[i]);
    }

    // Save context depth before eval to detect incomplete compound commands
    int saved_context_depth = script_state.context_depth;

    // Execute using script_process_line to properly handle shell keywords
    int result = script_process_line(cmd);

    free(cmd);

    // Check for incomplete compound command (syntax error)
    // If context depth increased, eval introduced an incomplete structure like "if" without "then"
    if (script_state.context_depth > saved_context_depth) {
        fprintf(stderr, "%s: eval: syntax error: unexpected end of file\n", HASH_NAME);
        last_command_exit_code = 2;
        // POSIX: In non-interactive shell, syntax error causes exit
        // Clear the incomplete contexts introduced by eval
        while (script_state.context_depth > saved_context_depth) {
            script_pop_context();
        }
        script_state.exit_requested = true;
        return 0;  // Signal exit
    }

    // POSIX: Check for pending break/continue to propagate from eval
    if (script_get_break_pending() > 0) {
        return -3;  // Propagate break signal
    }
    if (script_get_continue_pending() > 0) {
        return -4;  // Propagate continue signal
    }

    // Handle exit request from eval
    if (result == 0) {
        return 0;  // Exit was called
    }

    // Exit code already set by script_process_line
    return 1;
}

// ============================================================================
// Command Information Builtins
// ============================================================================

// POSIX reserved words (keywords)
static const char *posix_keywords[] = {
    "!", "{", "}", "case", "do", "done", "elif", "else", "esac",
    "fi", "for", "if", "in", "then", "until", "while", NULL
};

// Check if word is a POSIX reserved word
static bool is_posix_keyword(const char *word) {
    for (int i = 0; posix_keywords[i] != NULL; i++) {
        if (strcmp(word, posix_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Check if name is a builtin (helper for command -v/-V)
static bool is_builtin_name(const char *name) {
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return true;
        }
    }
    return false;
}

// Find command in PATH and return full path (caller must free)
char *find_in_path(const char *cmd) {
    // If cmd contains /, it's already a path
    if (strchr(cmd, '/') != NULL) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "/usr/bin:/bin";
    }

    char *path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char *saveptr;
    char *dir = strtok_r(path_copy, ":", &saveptr);

    while (dir) {
        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd);

        if (access(fullpath, X_OK) == 0) {
            free(path_copy);
            return strdup(fullpath);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

static bool find_command(const char *cmd, bool verbose) {
    bool found = false;

    // Check for alias first
    const char *alias_val = config_get_alias(cmd);
    if (alias_val) {
        if (verbose) {
            printf("%s is aliased to '%s'\n", cmd, alias_val);
        } else {
            printf("alias %s='%s'\n", cmd, alias_val);
        }
        found = true;
    }

    // Check for keyword
    if (is_posix_keyword(cmd)) {
        if (verbose) {
            printf("%s is a shell keyword\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for builtin
    if (is_builtin_name(cmd)) {
        if (verbose) {
            printf("%s is a shell builtin\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for function
    if (script_get_function(cmd)) {
        if (verbose) {
            printf("%s is a function\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for external command
    if (!found) {
        char *path = find_in_path(cmd);
        if (path) {
            if (verbose) {
                printf("%s is %s\n", cmd, path);
            } else {
                printf("%s\n", path);
            }
            free(path);
            found = true;
        }
    }

    if (!found && verbose) {
        fprintf(stderr, "%s: %s: not found\n", HASH_NAME, cmd);
    }

    last_command_exit_code = found ? 0 : 1;

    return found;
}

int shell_command(char **args) {
    bool opt_v = false;  // -v: print command path
    bool opt_V = false;  // -V: verbose description
    bool opt_p = false;  // -p: use default PATH
    int arg_start = 1;

    // Parse options
    while (args[arg_start] && args[arg_start][0] == '-') {
        const char *opt = args[arg_start];
        if (strcmp(opt, "-v") == 0) {
            opt_v = true;
            arg_start++;
        } else if (strcmp(opt, "-V") == 0) {
            opt_V = true;
            arg_start++;
        } else if (strcmp(opt, "-p") == 0) {
            opt_p = true;
            arg_start++;
        } else if (strcmp(opt, "--") == 0) {
            arg_start++;
            break;
        } else {
            // Unknown option
            break;
        }
    }

    (void)opt_p;  // TODO: implement default PATH

    // No command specified
    if (!args[arg_start]) {
        last_command_exit_code = 0;
        return 1;
    }

    const char *cmd = args[arg_start];

    // Handle -v option (print command location/type)
    if (opt_v) {
        bool ret = find_command(cmd, false);
        (void)ret;
        return 1;
    }

    // Handle -V option (verbose description)
    if (opt_V) {
        bool ret = find_command(cmd, true);
        (void)ret;
        return 1;
    }

    // Execute command, bypassing functions
    // First check builtins
    // POSIX: 'command' removes "special" status from special builtins
    // So errors should not cause shell to exit (always return 1)
    int result = try_builtin(args + arg_start);
    if (result != -1) {
        // Always return 1 (continue) - special builtins lose their "exit on error" behavior
        // The exit code is already set in last_command_exit_code by the builtin
        return 1;
    }

    // Execute as external command (execute() will handle it)
    // We need to skip function lookup, so we directly launch
    // For now, just call execute which handles external commands
    execute(args + arg_start);
    return 1;
}

static bool check_has_command(char **args) {
    // Parse for redirections vs command
    for (int i = 0; args[i]; i++) {
        const char *arg = args[i];
        // Check if this looks like a redirection
        // Patterns: N<file, N>file, N>>file, N<&M, N>&M, <file, >file, etc.
        bool is_redir = false;

        // Check for digit prefix followed by redirection operator
        const char *p = arg;
        while (*p && isdigit(*p)) p++;

        if (*p == '<' || *p == '>') {
            is_redir = true;
        } else if (p == arg) {
            // No digit prefix, check for bare redirection
            if (*p == '<' || *p == '>') {
                is_redir = true;
            }
        }

        if (!is_redir) {
            // has command
            return true;
        }
    }

    return false;
}

static int handle_redirections(char **args, bool has_command) {
    for (int i = 0; args[i]; i++) {
        const char *arg = args[i];

        // Parse fd number (default to 0 for input, 1 for output)
        int fd = -1;
        const char *p = arg;

        while (*p && isdigit(*p)) p++;

        if (p > arg) {
            fd = atoi(arg);
        }

        // Handle different redirection types
        if (*p == '<') {
            if (fd < 0) fd = 0;  // Default input fd
            p++;

            if (*p == '&') {
                // N<&M or N<&-
                p++;
                if (*p == '-') {
                    // Close fd
                    close(fd);
                } else {
                    // Duplicate fd
                    int src_fd = atoi(p);
                    if (dup2(src_fd, fd) < 0) {
                        perror(HASH_NAME);
                        last_command_exit_code = 1;
                        return 1;
                    }
                }
            } else {
                // N<file - open file for input
                const char *filename = p;
                if (!*filename && args[i + 1]) {
                    filename = args[++i];
                }
                int new_fd = open(filename, O_RDONLY);
                if (new_fd < 0) {
                    fprintf(stderr, "%s: %s: %s\n", HASH_NAME, filename, strerror(errno));
                    last_command_exit_code = 1;
                    return 1;
                }
                if (new_fd != fd) {
                    dup2(new_fd, fd);
                    close(new_fd);
                }
            }
        } else if (*p == '>') {
            if (fd < 0) fd = 1;  // Default output fd
            p++;

            bool append = false;
            if (*p == '>') {
                append = true;
                p++;
            }

            if (*p == '&') {
                // N>&M or N>&-
                p++;
                if (*p == '-') {
                    close(fd);
                } else {
                    int src_fd = atoi(p);
                    if (dup2(src_fd, fd) < 0) {
                        perror(HASH_NAME);
                        last_command_exit_code = 1;
                        return 1;
                    }
                }
            } else {
                // N>file or N>>file
                const char *filename = p;
                if (!*filename && args[i + 1]) {
                    filename = args[++i];
                }
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int new_fd = open(filename, flags, 0644);
                if (new_fd < 0) {
                    fprintf(stderr, "%s: %s: %s\n", HASH_NAME, filename, strerror(errno));
                    last_command_exit_code = 1;
                    return 1;
                }
                if (new_fd != fd) {
                    dup2(new_fd, fd);
                    close(new_fd);
                }
            }
        } else if (has_command) {
            // This is the command to exec
            break;
        }
    }

    return 0;
}

int shell_exec(char **args) {
    // exec with no arguments: just return success (noop)
    if (!args[1]) {
        last_command_exit_code = 0;
        return 1;
    }

    // Check for redirections only (exec N<file, exec N>file, etc.)
    // These persist for the shell process
    bool has_command = check_has_command(&args[1]);

    // Handle redirections
    int ret = handle_redirections(&args[1], has_command);
    if (ret != 0) {
        return ret;
    }

    // If there's a command, replace the shell
    if (has_command) {
        // Find the command start
        for (int i = 1; args[i]; i++) {
            const char *arg = args[i];
            const char *p = arg;
            while (*p && isdigit(*p)) p++;
            if (*p != '<' && *p != '>') {
                // This is the command
                // Flush all output buffers before replacing the process
                fflush(stdout);
                fflush(stderr);
                execvp(args[i], args + i);
                // If we get here, exec failed
                fprintf(stderr, "%s: %s: %s\n", HASH_NAME, args[i], strerror(errno));
                last_command_exit_code = 127;
                return 0;  // Exit shell on exec failure
            }
        }
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_times(char **args) {
    (void)args;

    struct tms times_buf;
    clock_t real_time = times(&times_buf);

    if (real_time == (clock_t)-1) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) ticks_per_sec = 100;  // Fallback

    // Shell times (user and system)
    long shell_user_sec = times_buf.tms_utime / ticks_per_sec;
    long shell_user_ms = (times_buf.tms_utime % ticks_per_sec) * 1000 / ticks_per_sec;
    long shell_sys_sec = times_buf.tms_stime / ticks_per_sec;
    long shell_sys_ms = (times_buf.tms_stime % ticks_per_sec) * 1000 / ticks_per_sec;

    // Children times (user and system)
    long child_user_sec = times_buf.tms_cutime / ticks_per_sec;
    long child_user_ms = (times_buf.tms_cutime % ticks_per_sec) * 1000 / ticks_per_sec;
    long child_sys_sec = times_buf.tms_cstime / ticks_per_sec;
    long child_sys_ms = (times_buf.tms_cstime % ticks_per_sec) * 1000 / ticks_per_sec;

    // Format output into buffer and use write(2) directly to detect EPIPE immediately
    // (printf/fflush may not detect broken pipe due to buffering)
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%ldm%ld.%03lds %ldm%ld.%03lds\n",
           shell_user_sec / 60, shell_user_sec % 60, shell_user_ms,
           shell_sys_sec / 60, shell_sys_sec % 60, shell_sys_ms);

    if (len < 0 || write(STDOUT_FILENO, buf, (size_t)len) < 0) {
        fprintf(stderr, "%s: times: I/O error\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }

    len = snprintf(buf, sizeof(buf), "%ldm%ld.%03lds %ldm%ld.%03lds\n",
           child_user_sec / 60, child_user_sec % 60, child_user_ms,
           child_sys_sec / 60, child_sys_sec % 60, child_sys_ms);

    if (len < 0 || write(STDOUT_FILENO, buf, (size_t)len) < 0) {
        fprintf(stderr, "%s: times: I/O error\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_type(char **args) {
    // type is equivalent to command -V
    if (!args[1]) {
        last_command_exit_code = 0;
        return 1;
    }

    bool all_found = true;

    for (int i = 1; args[i]; i++) {
        const char *cmd = args[i];
        if (!find_command(cmd, true)) {
            all_found = false;
        }
    }

    last_command_exit_code = all_found ? 0 : 1;
    return 1;
}

int shell_readonly(char **args) {
    // readonly with no args: list all readonly variables
    if (args[1] == NULL) {
        shellvar_list_readonly();
        last_command_exit_code = 0;
        return 1;
    }

    // Handle options
    int start = 1;
    while (args[start] && args[start][0] == '-') {
        if (strcmp(args[start], "-p") == 0) {
            if (args[start + 1] == NULL) {
                shellvar_list_readonly();
                last_command_exit_code = 0;
                return 1;
            }
            start++;
        } else if (strcmp(args[start], "--") == 0) {
            // End of options
            start++;
            break;
        } else {
            // Unknown option - just skip it for now
            start++;
        }
    }

    // Process each argument
    for (int i = start; args[i] != NULL; i++) {
        const char *arg = args[i];
        char *equals = strchr(arg, '=');

        if (equals) {
            // readonly name=value: set and mark as readonly
            *equals = '\0';
            const char *name = arg;
            const char *value = equals + 1;

            // Check if already readonly with different value
            if (shellvar_is_readonly(name)) {
                const char *old_val = shellvar_get(name);
                if (old_val && strcmp(old_val, value) != 0) {
                    fprintf(stderr, "readonly: %s: is read only\n", name);
                    last_command_exit_code = 1;
                    *equals = '=';
                    return 0;  // Exit shell per POSIX (unless via 'command')
                }
            }

            // Set the value first
            if (shellvar_set(name, value) != 0) {
                last_command_exit_code = 1;
                *equals = '=';
                return 0;
            }

            // Then mark as readonly
            shellvar_set_readonly(name);

            // Also set in environment
            setenv(name, value, 1);

            *equals = '=';
        } else {
            // readonly name: just mark as readonly
            shellvar_set_readonly(arg);
        }
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_trap(char **args) {
    // trap with no args: list all traps
    if (args[1] == NULL) {
        trap_list();
        last_command_exit_code = 0;
        return 1;
    }

    // trap -p: print traps (same as no args for now)
    if (strcmp(args[1], "-p") == 0) {
        if (args[2] == NULL) {
            trap_list();
        } else {
            // Print specific traps
            for (int i = 2; args[i]; i++) {
                int signum = trap_parse_signal(args[i]);
                if (signum >= 0) {
                    const char *action = trap_get(signum);
                    if (action) {
                        const char *name = trap_signal_name(signum);
                        if (name) {
                            printf("trap -- '%s' %s\n", action, name);
                        } else {
                            printf("trap -- '%s' %d\n", action, signum);
                        }
                    }
                }
            }
        }
        last_command_exit_code = 0;
        return 1;
    }

    // trap -l: list signal names
    if (strcmp(args[1], "-l") == 0) {
        // Print common signal numbers and names
        printf(" 1) SIGHUP\t 2) SIGINT\t 3) SIGQUIT\t 4) SIGILL\n");
        printf(" 5) SIGTRAP\t 6) SIGABRT\t 7) SIGBUS\t 8) SIGFPE\n");
        printf(" 9) SIGKILL\t10) SIGUSR1\t11) SIGSEGV\t12) SIGUSR2\n");
        printf("13) SIGPIPE\t14) SIGALRM\t15) SIGTERM\t16) SIGSTKFLT\n");
        printf("17) SIGCHLD\t18) SIGCONT\t19) SIGSTOP\t20) SIGTSTP\n");
        printf("21) SIGTTIN\t22) SIGTTOU\n");
        last_command_exit_code = 0;
        return 1;
    }

    // Check if first arg is a signal name (reset to default)
    // trap signal [signal...]: reset signals to default
    // trap '' signal [signal...]: ignore signals
    // trap action signal [signal...]: set trap

    const char *action = args[1];
    int start = 2;

    // If action is "-", it means reset to default
    if (action[0] == '-' && action[1] == '\0') {
        action = NULL;
    }

    // Set traps for all specified signals
    for (int i = start; args[i]; i++) {
        if (trap_set(action, args[i]) != 0) {
            last_command_exit_code = 1;
            return 1;
        }
    }

    last_command_exit_code = 0;
    return 1;
}

static void wait_for_all_jobs(sigset_t old_mask) {
    int status;
    pid_t pid;

    // Wait for all child processes
    // Keep trying until no more children (ECHILD) or interrupted
    while (1) {
        pid = waitpid(-1, &status, 0);
        if (pid > 0) {
            // Successfully waited for a child
            jobs_update_status(pid, status);
            const Job *job = jobs_get_by_pid(pid);
            if (job && (job->state == JOB_DONE || job->state == JOB_TERMINATED)) {
                jobs_remove(job->job_id);
            }
        } else if (pid == -1) {
            if (errno == ECHILD) {
                // No more children to wait for
                break;
            } else if (errno == EINTR) {
                // Interrupted by signal, try again
                continue;
            } else {
                // Other error, stop waiting
                break;
            }
        } else {
            // pid == 0 shouldn't happen without WNOHANG, but handle it
            break;
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    last_command_exit_code = 0;
}

static void wait_for_job_pid(pid_t pid, int job_id_to_remove) {
    int status;
    pid_t result = waitpid(pid, &status, 0);
    if (result > 0) {
        if (WIFEXITED(status)) {
            last_command_exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_command_exit_code = 128 + WTERMSIG(status);
        } else {
            last_command_exit_code = 1;
        }
        jobs_update_status(pid, status);
        // Remove the completed job from the table
        if (job_id_to_remove > 0) {
            jobs_remove(job_id_to_remove);
        }
    } else {
        // Process already exited (reaped by SIGCHLD handler) or doesn't exist
        // Check if we have a stored exit status from the job table
        const Job *job = jobs_get_by_pid(pid);
        if (job && (job->state == JOB_DONE || job->state == JOB_TERMINATED)) {
            // Use the stored exit status from when SIGCHLD reaped it
            last_command_exit_code = job->exit_status;
            jobs_remove(job->job_id);
        } else if (job_id_to_remove > 0) {
            // Job exists but no stored status - shouldn't happen
            last_command_exit_code = 127;
            jobs_remove(job_id_to_remove);
        } else {
            // Not a known job - just report 127
            last_command_exit_code = 127;
        }
    }
}

int shell_wait(char **args) {
    // Block SIGCHLD during wait to prevent race condition where
    // the SIGCHLD handler reaps children before waitpid can see them
    sigset_t block_mask, old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    // wait with no args: wait for all background jobs
    if (args[1] == NULL) {
        wait_for_all_jobs(old_mask);
        return 1;
    }

    // wait with arguments: wait for specific PIDs or job IDs
    for (int i = 1; args[i]; i++) {
        const char *arg = args[i];
        pid_t pid = 0;
        int job_id_to_remove = 0;

        // Check for job ID (%n)
        if (arg[0] == '%') {
            const Job *job = NULL;
            if (arg[1] == '%' || arg[1] == '+' || arg[1] == '\0') {
                // %%, %+, or just % - current job
                job = jobs_get_current();
            } else {
                int job_id = atoi(arg + 1);
                job = jobs_get(job_id);
            }
            if (job) {
                pid = job->pid;
                job_id_to_remove = job->job_id;
            } else {
                fprintf(stderr, "%s: wait: %s: no such job\n", HASH_NAME, arg);
                last_command_exit_code = 127;
                continue;
            }
        } else {
            // PID
            pid = atoi(arg);
            // Find job ID if this PID is in the table
            const Job *job = jobs_get_by_pid(pid);
            if (job) {
                job_id_to_remove = job->job_id;
            }
        }

        if (pid > 0) {
            wait_for_job_pid(pid, job_id_to_remove);
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return 1;
}

// Signal name to number mapping
static int signal_name_to_number(const char *name) {
    // Skip optional "SIG" prefix
    if (strncasecmp(name, "SIG", 3) == 0) {
        name += 3;
    }

    // Common signals
    if (strcasecmp(name, "HUP") == 0) return SIGHUP;
    if (strcasecmp(name, "INT") == 0) return SIGINT;
    if (strcasecmp(name, "QUIT") == 0) return SIGQUIT;
    if (strcasecmp(name, "ILL") == 0) return SIGILL;
#ifdef SIGTRAP
    if (strcasecmp(name, "TRAP") == 0) return SIGTRAP;
#endif
    if (strcasecmp(name, "ABRT") == 0) return SIGABRT;
    if (strcasecmp(name, "FPE") == 0) return SIGFPE;
    if (strcasecmp(name, "KILL") == 0) return SIGKILL;
    if (strcasecmp(name, "BUS") == 0) return SIGBUS;
    if (strcasecmp(name, "SEGV") == 0) return SIGSEGV;
    if (strcasecmp(name, "SYS") == 0) return SIGSYS;
    if (strcasecmp(name, "PIPE") == 0) return SIGPIPE;
    if (strcasecmp(name, "ALRM") == 0) return SIGALRM;
    if (strcasecmp(name, "TERM") == 0) return SIGTERM;
    if (strcasecmp(name, "URG") == 0) return SIGURG;
    if (strcasecmp(name, "STOP") == 0) return SIGSTOP;
    if (strcasecmp(name, "TSTP") == 0) return SIGTSTP;
    if (strcasecmp(name, "CONT") == 0) return SIGCONT;
    if (strcasecmp(name, "CHLD") == 0) return SIGCHLD;
    if (strcasecmp(name, "TTIN") == 0) return SIGTTIN;
    if (strcasecmp(name, "TTOU") == 0) return SIGTTOU;
#ifdef SIGIO
    if (strcasecmp(name, "IO") == 0) return SIGIO;
#endif
#ifdef SIGXCPU
    if (strcasecmp(name, "XCPU") == 0) return SIGXCPU;
#endif
#ifdef SIGXFSZ
    if (strcasecmp(name, "XFSZ") == 0) return SIGXFSZ;
#endif
#ifdef SIGVTALRM
    if (strcasecmp(name, "VTALRM") == 0) return SIGVTALRM;
#endif
#ifdef SIGPROF
    if (strcasecmp(name, "PROF") == 0) return SIGPROF;
#endif
#ifdef SIGWINCH
    if (strcasecmp(name, "WINCH") == 0) return SIGWINCH;
#endif
    if (strcasecmp(name, "USR1") == 0) return SIGUSR1;
    if (strcasecmp(name, "USR2") == 0) return SIGUSR2;

    return -1;  // Unknown signal
}

static void print_signals(void) {
    printf("HUP INT QUIT ILL ");
#ifdef SIGTRAP
    printf("TRAP ");
#endif
    printf("ABRT FPE KILL BUS SEGV SYS PIPE ALRM TERM\n");

    printf("URG STOP TSTP CONT CHLD TTIN TTOU ");
#ifdef SIGIO
    printf("IO ");
#endif
#ifdef SIGXCPU
    printf("XCPU ");
#endif
#ifdef SIGXFSZ
    printf("XFSZ ");
#endif
#ifdef SIGVTALRM
    printf("VTALRM ");
#endif
#ifdef SIGPROF
    printf("PROF ");
#endif
#ifdef SIGWINCH
    printf("WINCH ");
#endif
    printf("USR1 USR2\n");
}

static int process_each_target(char **args, int start_idx, int sig) {
    int result = 0;
    for (int i = start_idx; args[i]; i++) {
        pid_t pid = 0;
        const char *target = args[i];

        // Check for job specification
        if (target[0] == '%') {
            // Job specs only work when job control is enabled (set -m)
            if (!shell_option_monitor()) {
                fprintf(stderr, "%s: kill: %s: no job control\n", HASH_NAME, target);
                result = 1;
                continue;
            }

            const Job *job = NULL;
            if (target[1] == '%' || target[1] == '+' || target[1] == '\0') {
                // %%, %+, or just % - current job
                job = jobs_get_current();
            } else if (target[1] == '-') {
                // %- - previous job (we don't track this, use current)
                job = jobs_get_current();
            } else if (isdigit(target[1])) {
                // %n - job number
                int job_id = atoi(target + 1);
                job = jobs_get(job_id);
            }

            if (job) {
                pid = job->pid;
            } else {
                fprintf(stderr, "%s: kill: %s: no such job\n", HASH_NAME, target);
                result = 1;
                continue;
            }
        } else {
            // PID
            char *endptr;
            long num = strtol(target, &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "%s: kill: %s: arguments must be process or job IDs\n", HASH_NAME, target);
                result = 1;
                continue;
            }
            pid = (pid_t)num;
        }

        // Send signal
        if (kill(pid, sig) == -1) {
            fprintf(stderr, "%s: kill: (%d) - %s\n", HASH_NAME, pid, strerror(errno));
            result = 1;
        }
    }

    return result;
}

int shell_kill(char **args) {
    int sig = SIGTERM;  // Default signal
    int start_idx = 1;

    // Check for -l option (list signals)
    if (args[1] && strcmp(args[1], "-l") == 0) {
        print_signals();
        last_command_exit_code = 0;
        return 1;
    }

    // Parse signal specification
    if (args[1] && args[1][0] == '-') {
        const char *sigspec = args[1] + 1;

        if (strcmp(sigspec, "s") == 0) {
            // -s SIG format
            if (!args[2]) {
                fprintf(stderr, "%s: kill: -s requires an argument\n", HASH_NAME);
                last_command_exit_code = 1;
                return 1;
            }
            sigspec = args[2];
            start_idx = 3;
        } else {
            start_idx = 2;
        }

        // Check if it's a number
        char *endptr;
        long num = strtol(sigspec, &endptr, 10);
        if (*endptr == '\0') {
            sig = (int)num;
        } else {
            // It's a signal name
            sig = signal_name_to_number(sigspec);
            if (sig == -1) {
                fprintf(stderr, "%s: kill: %s: invalid signal specification\n", HASH_NAME, sigspec);
                last_command_exit_code = 1;
                return 1;
            }
        }
    }

    // No targets specified
    if (!args[start_idx]) {
        fprintf(stderr, "usage: kill [-s sigspec | -sigspec] pid | jobspec ...\n");
        last_command_exit_code = 1;
        return 1;
    }

    // Process each target
    last_command_exit_code = process_each_target(args, start_idx, sig);
    return 1;
}

// ============================================================================
// Command Hash Table (for hash builtin)
// ============================================================================

#define CMD_HASH_SIZE 64

typedef struct CmdHashEntry {
    char *name;
    char *path;
    int hits;
    struct CmdHashEntry *next;
} CmdHashEntry;

static CmdHashEntry *cmd_hash_table[CMD_HASH_SIZE];

// Simple hash function for command names
static unsigned int cmd_hash_func(const char *s) {
    unsigned int hash = 0;
    while (*s) {
        hash = hash * 31 + (unsigned char)*s++;
    }
    return hash % CMD_HASH_SIZE;
}

// Add a command to the hash table
void cmd_hash_add(const char *name, const char *path) {
    unsigned int h = cmd_hash_func(name);

    // Check if already exists
    CmdHashEntry *e = cmd_hash_table[h];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            // Update path if different
            if (strcmp(e->path, path) != 0) {
                free(e->path);
                e->path = strdup(path);
            }
            e->hits++;
            return;
        }
        e = e->next;
    }

    // Add new entry
    CmdHashEntry *new_entry = malloc(sizeof(CmdHashEntry));
    if (new_entry) {
        new_entry->name = strdup(name);
        new_entry->path = strdup(path);
        new_entry->hits = 1;
        new_entry->next = cmd_hash_table[h];
        cmd_hash_table[h] = new_entry;
    }
}

// Clear all entries from hash table
static void cmd_hash_clear(void) {
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        CmdHashEntry *e = cmd_hash_table[i];
        while (e) {
            CmdHashEntry *next = e->next;
            free(e->name);
            free(e->path);
            free(e);
            e = next;
        }
        cmd_hash_table[i] = NULL;
    }
}

// List all hashed commands
static void cmd_hash_list(void) {
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        const CmdHashEntry *e = cmd_hash_table[i];
        if (e) {
            printf("hits\tcommand\n");
        }
        if (cmd_hash_table[i]) break;
    }

    // Print header and entries
    bool has_entries = false;
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        if (cmd_hash_table[i]) {
            has_entries = true;
            break;
        }
    }

    if (has_entries) {
        for (int i = 0; i < CMD_HASH_SIZE; i++) {
            CmdHashEntry *e = cmd_hash_table[i];
            while (e) {
                printf("%4d\t%s\n", e->hits, e->path);
                e = e->next;
            }
        }
    }
}

int shell_hash(char **args) {
    // hash with no args: list all hashed commands
    if (args[1] == NULL) {
        cmd_hash_list();
        last_command_exit_code = 0;
        return 1;
    }

    // hash -r: clear hash table
    if (strcmp(args[1], "-r") == 0) {
        cmd_hash_clear();
        last_command_exit_code = 0;
        return 1;
    }

    // hash name [name...]: look up and hash commands
    for (int i = 1; args[i]; i++) {
        char *path = find_in_path(args[i]);
        if (path) {
            cmd_hash_add(args[i], path);
            free(path);
        } else {
            fprintf(stderr, "%s: hash: %s: not found\n", HASH_NAME, args[i]);
            last_command_exit_code = 1;
            return 1;
        }
    }

    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// Builtin Dispatch
// ============================================================================

int try_builtin(char **args) {
    if (args[0] == NULL) {
        return -1;
    }

    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            return (*builtins[i].func)(args);
        }
    }

    return -1;
}

bool is_builtin(const char *cmd) {
    if (cmd == NULL) {
        return false;
    }

    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return true;
        }
    }

    return false;
}
