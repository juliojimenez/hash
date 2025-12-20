#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "hash.h"
#include "execute.h"
#include "builtins.h"

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
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int execute(char **args) {
    if (args[0] == NULL) {
        return 1;
    }

    int result = try_builtin(args);
    if (result != -1) {
        return result;
    }

    return launch(args);
}
