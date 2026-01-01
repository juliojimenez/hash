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
#include "execute.h"
#include "history.h"
#include "jobs.h"

extern int last_command_exit_code;

// Track if this is a login shell (set from main)
static bool is_login_shell = false;

void builtins_set_login_shell(bool is_login) {
    is_login_shell = is_login;
}

static char *builtin_str[] = {
    "cd",
    "exit",
    "alias",
    "unalias",
    "source",
    "export",
    "history",
    "jobs",
    "fg",
    "bg",
    "logout"
};

static int (*builtin_func[])(char **) = {
    &shell_cd,
    &shell_exit,
    &shell_alias,
    &shell_unalias,
    &shell_source,
    &shell_export,
    &shell_history,
    &shell_jobs,
    &shell_fg,
    &shell_bg,
    &shell_logout
};

static int num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

// Parse job ID from argument (handles %n, %%, %+, %-, n)
static int parse_job_id(const char *arg) {
    if (!arg) return 0;  // No arg means current job

    // Skip leading whitespace
    while (isspace(*arg)) arg++;

    if (*arg == '%') {
        arg++;
        if (*arg == '%' || *arg == '+' || *arg == '\0') {
            return 0;  // Current job
        } else if (*arg == '-') {
            // Previous job - not fully implemented, treat as current
            return 0;
        } else if (isdigit(*arg)) {
            return atoi(arg);
        }
    } else if (isdigit(*arg)) {
        return atoi(arg);
    }

    return -1;  // Invalid
}

// Built-in: cd
int shell_cd(char **args) {
    const char *path = args[1];

    // If no argument, go to home directory
    if (path == NULL) {
        path = getenv("HOME");

        if (!path) {
            // Use getpwuid_r for thread safety
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

// Built-in: exit
int shell_exit(char **args) {
    // Check for running jobs
    int job_count = jobs_count();
    if (job_count > 0) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'exit' again to force exit, or 'jobs' to see them.\n");

        // Set a flag to allow exit on next attempt
        static int exit_attempted = 0;
        if (exit_attempted) {
            exit_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = 0;
            return 0;
        }
        exit_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    if (args[1] != NULL) {
        fprintf(stderr, "%s: exit accepts no arguments\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    fprintf(stdout, "Bye :)\n");
    last_command_exit_code = 0;
    return 0;
}

// Built-in: alias
int shell_alias(char **args) {
    // No arguments - list all aliases
    if (args[1] == NULL) {
        config_list_aliases();
        last_command_exit_code = 0;
        return 1;
    }

    // Check if it's an alias definition (name=value)
    char *equals = strchr(args[1], '=');
    if (equals) {
        *equals = '\0';
        char *name = args[1];
        char *value = equals + 1;

        // Remove quotes if present
        if ((value[0] == '"' || value[0] == '\'') &&
            value[0] == value[strlen(value) - 1]) {
            value[strlen(value) - 1] = '\0';
            value++;
        }

        if (config_add_alias(name, value) == 0) {
            color_success("Alias '%s' added", name);
            last_command_exit_code = 0;
        } else {
            color_error("Failed to add alias");
            last_command_exit_code = 1;
        }
    } else {
        // Show specific alias
        const char *value = config_get_alias(args[1]);
        if (value) {
            color_print(COLOR_CYAN, "%s", args[1]);
            printf("='%s'\n", value);
            last_command_exit_code = 0;
        } else {
            color_error("%s: alias not found: %s", HASH_NAME, args[1]);
            last_command_exit_code = 1;
        }
    }

    return 1;
}

// Built-in: unalias
int shell_unalias(char **args) {
    if (args[1] == NULL) {
        color_error("%s: expected argument to \"unalias\"", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    if (config_remove_alias(args[1]) == 0) {
        color_success("Alias '%s' removed", args[1]);
        last_command_exit_code = 0;
    } else {
        color_error("%s: alias not found: %s", HASH_NAME, args[1]);
        last_command_exit_code = 1;
    }

    return 1;
}

// Built-in: source
int shell_source(char **args) {
    if (args[1] == NULL) {
        color_error("%s: expected argument to \"source\"", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    if (config_load(args[1]) == 0) {
        color_success("Loaded config from '%s'", args[1]);
        last_command_exit_code = 0;
    } else {
        color_error("%s: failed to load config from '%s'", HASH_NAME, args[1]);
        last_command_exit_code = 1;
    }

    return 1;
}

// Built-in: export
int shell_export(char **args) {
    if (args[1] == NULL) {
        // No arguments - list all environment variables
        extern char **environ;
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
        last_command_exit_code = 0;
        return 1;
    }

    // Process each argument (VAR=value)
    for (int i = 1; args[i] != NULL; i++) {
        char *equals = strchr(args[i], '=');
        if (!equals) {
            // No = sign, just export existing variable (mark for export)
            // For now, we'll just report it's not supported
            color_warning("%s: export without assignment not yet supported: %s", HASH_NAME, args[i]);
            last_command_exit_code = 1;
            continue;
        }

        // Split into name=value
        *equals = '\0';
        const char *name = args[i];
        const char *value = equals + 1;

        // Set the environment variable
        if (setenv(name, value, 1) == 0) {
            last_command_exit_code = 0;
        } else {
            perror(HASH_NAME);
            last_command_exit_code = 1;
        }
    }

    return 1;
}

// Built-in: history
int shell_history(char **args) {
    // Handle flags
    if (args[1] != NULL) {
        if (strcmp(args[1], "-c") == 0) {
            // Clear history
            history_clear();
            color_success("History cleared");
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-w") == 0) {
            // Write/save history
            history_save();
            color_success("History saved");
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-r") == 0) {
            // Read/reload history
            history_clear();
            history_load();
            color_success("History reloaded");
            last_command_exit_code = 0;
            return 1;
        }
    }

    // List history with line numbers
    int count = history_count();
    for (int i = 0; i < count; i++) {
        const char *cmd = history_get(i);
        if (cmd) {
            color_print(COLOR_CYAN, "%5d", i);
            printf("  %s\n", cmd);
        }
    }

    last_command_exit_code = 0;
    return 1;
}

// Built-in: jobs
int shell_jobs(char **args) {
    (void)args;  // Unused for now

    jobs_list();
    last_command_exit_code = 0;
    return 1;
}

// Built-in: fg
int shell_fg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        color_error("%s: fg: invalid job specification: %s", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_foreground(job_id);
    last_command_exit_code = (result == -1) ? 1 : result;
    return 1;
}

// Built-in: bg
int shell_bg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        color_error("%s: bg: invalid job specification: %s", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_background(job_id);
    last_command_exit_code = (result == -1) ? 1 : 0;
    return 1;
}

// Built-in: logout (for login shells only)
int shell_logout(char **args) {
    (void)args;  // Unused

    if (!is_login_shell) {
        color_error("%s: logout: not a login shell", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    // Check for running jobs (same as exit)
    int job_count = jobs_count();
    if (job_count > 0) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'logout' again to force logout, or 'jobs' to see them.\n");

        // Set a flag to allow logout on next attempt
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

// Check if command is a built-in and execute it
int try_builtin(char **args) {
    if (args[0] == NULL) {
        return -1;
    }

    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return -1; // Not a built-in
}
