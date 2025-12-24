#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hash.h"
#include "builtins.h"
#include "colors.h"
#include "config.h"
#include "execute.h"

extern int last_command_exit_code;

static char *builtin_str[] = {
    "cd",
    "exit",
    "alias",
    "unalias",
    "source"
};

static int (*builtin_func[])(char **) = {
    &shell_cd,
    &shell_exit,
    &shell_alias,
    &shell_unalias,
    &shell_source
};

static int num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

// Built-in: cd
int shell_cd(char **args) {
    if (args[1] == NULL) {
        color_error("%s: expected argument to \"cd\"", HASH_NAME);
        last_command_exit_code = 1;
    } else {
        if (chdir(args[1]) != 0) {
            perror(HASH_NAME);
            last_command_exit_code = 1;
        } else {
            last_command_exit_code = 0;
        }
    }
    return 1;
}

// Built-in: exit
int shell_exit(char **args) {
    if (args[1] != NULL) {
        fprintf(stderr, "%s: exit accepts no arguments\n", HASH_NAME);
        last_command_exit_code = 1;
    } else {
        fprintf(stdout, "Bye :)\n");
        last_command_exit_code = 0;
    }
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
