/*

hash - Command line interface (shell) for the Linux operating system.
https://github.com/juliojimenez/hash
Apache 2.0

Julio Jimenez, julio@julioj.com

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "hash.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "chain.h"
#include "lineedit.h"
#include "history.h"
#include "completion.h"
#include "builtins.h"
#include "jobs.h"

// Shell process group ID
static pid_t shell_pgid;

// Track if this is a login shell (needed for logout handling)
static bool is_login_shell_global = false;

// Signal handler for cleanup
static void signal_handler(int sig) {
    (void)sig;
    lineedit_cleanup();
    printf("\n");
    exit(EXIT_SUCCESS);
}

// Initialize job control for the shell
static void init_job_control(void) {
    // Check if we're running interactively
    if (!isatty(STDIN_FILENO)) {
        return;
    }

    // Loop until we're in the foreground
    while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp())) {
        kill(-shell_pgid, SIGTTIN);
    }

    // Ignore interactive and job-control signals in the shell
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // Put ourselves in our own process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        // EPERM means we're already a session leader (e.g., login shell via SSH)
        // This is fine - we're already in control
        if (errno != EPERM) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }
        // We're a session leader, so our pgid is already our pid
        shell_pgid = getpgrp();
    }

    // Grab control of the terminal
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

static void loop(void) {
    char *line;
    int status;
    int last_exit_code = 0;

    do {
        // Check for completed background jobs before displaying prompt
        jobs_check_completed();

        const char *prompt_str = prompt_generate(last_exit_code);

        line = read_line(prompt_str);

        history_reset_position();

        // Expand history references
        char *expanded = history_expand(line);
        if (expanded) {
            printf("%s\n", expanded);
            free(line);
            line = expanded;
        }

        CommandChain *chain = chain_parse(line);

        if (chain) {
            history_add(line);

            // Execute the chain
            status = chain_execute(chain);

            // Get exit code for next prompt
            last_exit_code = execute_get_last_exit_code();

            // Free the chain
            chain_free(chain);
        } else {
            // Empty line or parse error
            status = 1;
            last_exit_code = 0;
        }
        free(line);
    } while(status);
}

int main(int argc, char *argv[]) {
    // Determine if we're a login shell
    // A login shell is indicated by:
    // 1. argv[0] starting with '-' (e.g., "-hash" set by login/sshd)
    // 2. --login or -l flag passed as argument

    if (argc > 0 && argv[0][0] == '-') {
        is_login_shell_global = true;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--login") == 0 || strcmp(argv[i], "-l") == 0) {
            is_login_shell_global = true;
        }
    }

    // Initialize job control (must be done early)
    init_job_control();

    // Setup signal handler for clean terminal restoration on SIGTERM
    signal(SIGTERM, signal_handler);

    // Initialize line editor
    lineedit_init();

    // Initialize colors
    colors_init();

    // Initialize config with defaults
    config_init();

    // Initialize prompt system
    prompt_init();

    // Initialize history (loads from ~/.hash_history)
    history_init();

    // Initialize tab completion
    completion_init();

    // Initialize job control subsystem
    jobs_init();

    // Set login shell status for builtins (needed for logout command)
    builtins_set_login_shell(is_login_shell_global);

    // Load startup files based on shell type
    config_load_startup_files(is_login_shell_global);

    if (shell_config.show_welcome) {
        color_print(COLOR_BOLD COLOR_CYAN, "%s", HASH_NAME);
        printf(" v%s", HASH_VERSION);
        if (is_login_shell_global) {
            printf(" (login)");
        }
        printf("\n");
        printf("Type ");
        color_print(COLOR_YELLOW, "'exit'");
        printf(" to quit\n\n");
    }

    // Run Command Loop
    loop();

    // Run logout scripts for login shells
    if (is_login_shell_global) {
        config_load_logout_files();
    }

    // Cleanup
    lineedit_cleanup();

    return EXIT_SUCCESS;
}
