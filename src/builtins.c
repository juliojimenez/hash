#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hash.h"
#include "builtins.h"
#include "colors.h"

static char *builtin_str[] = {
    "cd",
    "exit"
};

// Built-in command functions
static int (*builtin_func[])(char **) = {
    &shell_cd,
    &shell_exit
};

static int num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

// Built-in: cd
int shell_cd(char **args) {
    if (args[1] == NULL) {
        color_error("%s: expected argument to \"cd\"", HASH_NAME);
    } else {
        if (chdir(args[1]) != 0) {
            perror(HASH_NAME);
        }
    }
    return 1;
}

int shell_exit(char **args) {
    if (args[1] != NULL) {
        fprintf(stderr, "%s: exit accepts no arguments\n", HASH_NAME);
    } else {
        fprintf(stdout, "Bye :)\n");
        return 0;
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
