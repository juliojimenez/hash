/*

hash - Command line interface (shell) for the Linux operating system.
https://github.com/juliojimenez/hash
Apache 2.0

Julio Jimenez, julio@julioj.com

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "hash.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "chain.h"
#include "lineedit.h"

// Signal handler for cleanup
static void signal_handler(int sig) {
    (void)sig;
    lineedit_cleanup();
    printf("\n");
    exit(EXIT_SUCCESS);
}

static void loop(void) {
    char *line;
    int status;
    int last_exit_code = 0;

    do {
        const char *prompt_str = prompt_generate(last_exit_code);

        line = read_line(prompt_str);

        CommandChain *chain = chain_parse(line);

        if (chain) {
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

int main(/*int argc, char **argv*/) {
    // Setup signal handler for clean terminal restoration
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize line editor
    lineedit_init();

    // Initialize colors
    colors_init();

    // Initialize config with defaults
    config_init();

    // Initialize prompt system
    prompt_init();

    // Load .hashrc if it exists
    config_load_default();

    if (shell_config.show_welcome) {
        color_print(COLOR_BOLD COLOR_CYAN, "%s", HASH_NAME);
        printf(" v%s\n", HASH_VERSION);
        printf("Type ");
        color_print(COLOR_YELLOW, "'exit'");
        printf(" to quit\n\n");
    }

    // Run Command Loop
    loop();

    // Cleanup
    lineedit_cleanup();

    return EXIT_SUCCESS;
}
