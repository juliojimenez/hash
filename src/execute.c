#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "hash.h"
#include "execute.h"
#include "builtins.h"
#include "config.h"
#include "parser.h"

// Global to store last exit code
int last_command_exit_code = 0;

// Launch an external program
static int launch(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror(HASH_NAME);
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Fork error
        perror(HASH_NAME);
        last_command_exit_code = 1;
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        // Store exit code
        if (WIFEXITED(status)) {
            last_command_exit_code = WEXITSTATUS(status);
        } else {
            last_command_exit_code = 1;
        }
    }

    return 1;
}

// Execute command (built-in or external)
int execute(char **args) {
    if (args[0] == NULL) {
        // Empty command
        last_command_exit_code = 0;
        return 1;
    }

    // Check if command is an alias
    const char *alias_value = config_get_alias(args[0]);
    if (alias_value) {
        // Expand alias by parsing the alias value
        char *alias_line = strdup(alias_value);
        if (!alias_line) {
            last_command_exit_code = 1;
            return 1;
        }

        char **alias_args = parse_line(alias_line);
        if (!alias_args) {
            free(alias_line);
            last_command_exit_code = 1;
            return 1;
        }

        // If original command had arguments, we need to append them
        // Count original args (excluding command name)
        int orig_arg_count = 0;
        while (args[orig_arg_count + 1] != NULL) {
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
                int result = execute(combined_args);

                free(combined_args);
                free(alias_args);
                free(alias_line);
                return result;
            }
        }

        // No original args, just execute alias
        int result = execute(alias_args);
        free(alias_args);
        free(alias_line);
        return result;
    }

    // Try built-in commands first
    int result = try_builtin(args);
    if (result != -1) {
        last_command_exit_code = (result == 0) ? 0 : 0;  // Built-ins don't fail
        return result;
    }

    // Launch external program
    return launch(args);
}

// Get last exit code
int execute_get_last_exit_code(void) {
    return last_command_exit_code;
}
