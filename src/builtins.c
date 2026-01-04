#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <ctype.h>
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

extern int last_command_exit_code;

// Track if this is a login shell (set from main)
static bool is_login_shell = false;

void builtins_set_login_shell(bool is_login) {
    is_login_shell = is_login;
}

// Builtin command names and functions
static char *builtin_str[] = {
    "cd",
    "exit",
    "alias",
    "unalias",
    "source",
    ".",          // Alias for source (POSIX)
    "export",
    "set",
    "history",
    "jobs",
    "fg",
    "bg",
    "logout",
    "test",
    "[",
    "[[",         // Bash-style extended test
    "unset",
    "true",
    "false",
    ":",
    "echo",
    "read",
    "return",
    "break",
    "continue",
    "eval",
    "update"
};

static int (*builtin_func[])(char **) = {
    &shell_cd,
    &shell_exit,
    &shell_alias,
    &shell_unalias,
    &shell_source,
    &shell_source,    // . is alias for source
    &shell_export,
    &shell_set,
    &shell_history,
    &shell_jobs,
    &shell_fg,
    &shell_bg,
    &shell_logout,
    &shell_test,
    &shell_bracket,
    &shell_double_bracket,
    &shell_unset,
    &shell_true,
    &shell_false,
    &shell_colon,
    &shell_echo,
    &shell_read,
    &shell_return,
    &shell_break,
    &shell_continue_cmd,
    &shell_eval,
    &shell_update
};

static int num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

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

    if (chdir(path) != 0) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
    } else {
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

    // Check for running jobs
    int job_count = jobs_count();
    if (job_count > 0) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'exit' again to force exit, or 'jobs' to see them.\n");

        static int exit_attempted = 0;
        if (exit_attempted) {
            exit_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = exit_code;
            return 0;
        }
        exit_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    fprintf(stdout, "Bye :)\n");
    last_command_exit_code = exit_code;
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

    // Execute the script file
    // If we're already in silent mode (e.g., sourcing system files),
    // continue in silent mode for nested sources
    int result = script_execute_file_ex(args[1], 0, NULL, script_state.silent_errors);
    last_command_exit_code = result;

    return 1;
}

int shell_export(char **args) {
    if (args[1] == NULL) {
        extern char **environ;
        for (char **env = environ; *env != NULL; env++) {
            printf("export %s\n", *env);
        }
        last_command_exit_code = 0;
        return 1;
    }

    for (int i = 1; args[i] != NULL; i++) {
        char *equals = strchr(args[i], '=');
        if (!equals) {
            // Just marking for export - check if variable exists
            const char *val = getenv(args[i]);
            if (!val) {
                // Variable doesn't exist, but that's OK for export
                last_command_exit_code = 0;
            }
            continue;
        }

        *equals = '\0';
        const char *name = args[i];
        const char *value = equals + 1;

        if (setenv(name, value, 1) == 0) {
            last_command_exit_code = 0;
        } else {
            perror(HASH_NAME);
            last_command_exit_code = 1;
        }
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

int shell_set(char **args) {
    // Handle set with no arguments - list all shell variables
    // (For now, we don't maintain shell-local variables, so this is a no-op)
    if (args[1] == NULL) {
        last_command_exit_code = 0;
        return 1;
    }

    // Handle set option=value syntax for hash-specific options
    for (int i = 1; args[i] != NULL; i++) {
        const char *arg = args[i];

        // Handle colors option
        if (strcmp(arg, "colors=on") == 0) {
            shell_config.colors_enabled = true;
            colors_enable();
            last_command_exit_code = 0;
            continue;
        } else if (strcmp(arg, "colors=off") == 0) {
            shell_config.colors_enabled = false;
            colors_disable();
            last_command_exit_code = 0;
            continue;
        }

        // Handle welcome option
        if (strcmp(arg, "welcome=on") == 0) {
            shell_config.show_welcome = true;
            last_command_exit_code = 0;
            continue;
        } else if (strcmp(arg, "welcome=off") == 0) {
            shell_config.show_welcome = false;
            last_command_exit_code = 0;
            continue;
        }

        // Handle PS1 setting
        if (strncmp(arg, "PS1=", 4) == 0) {
            char *ps1_value = (char *)(arg + 4);

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
    (void)args;
    jobs_list();
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
    if (job_count > 0) {
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

    fprintf(stdout, "Bye :)\n");
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

    for (int i = start; args[i] != NULL; i++) {
        if (unset_var) {
            unsetenv(args[i]);
        }
        if (unset_func) {
            // TODO: Implement function unset when functions are fully implemented
        }
    }

    last_command_exit_code = 0;
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
            putchar(' ');
        }

        if (interpret_escapes) {
            // Interpret escape sequences
            const char *s = args[i];
            while (*s) {
                if (*s == '\\' && *(s + 1)) {
                    s++;
                    switch (*s) {
                        case 'n': putchar('\n'); break;
                        case 't': putchar('\t'); break;
                        case 'r': putchar('\r'); break;
                        case '\\': putchar('\\'); break;
                        case 'a': putchar('\a'); break;
                        case 'b': putchar('\b'); break;
                        case 'f': putchar('\f'); break;
                        case 'v': putchar('\v'); break;
                        case 'c': goto done;  // Stop output
                        default: putchar('\\'); putchar(*s); break;
                    }
                } else {
                    putchar(*s);
                }
                s++;
            }
        } else {
            printf("%s", args[i]);
        }
    }

done:
    if (newline) {
        putchar('\n');
    }

    last_command_exit_code = 0;
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
    int return_code = 0;

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

    // Check if we're in a loop
    if (!script_in_control_structure()) {
        fprintf(stderr, "%s: break: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    (void)levels;  // TODO: Handle multiple levels

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

    if (!script_in_control_structure()) {
        fprintf(stderr, "%s: continue: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    (void)levels;  // TODO: Handle multiple levels

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

    // Execute the concatenated command
    CommandChain *chain = chain_parse(cmd);
    if (chain) {
        chain_execute(chain);
        chain_free(chain);
    }

    free(cmd);

    // Exit code already set by chain_execute
    return 1;
}

// ============================================================================
// Builtin Dispatch
// ============================================================================

int try_builtin(char **args) {
    if (args[0] == NULL) {
        return -1;
    }

    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return -1;
}
