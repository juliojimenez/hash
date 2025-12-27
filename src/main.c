/*

hash - Command line interface (shell) for the Linux operating system.
https://github.com/juliojimenez/hash
Apache 2.0

Julio Jimenez, julio@julioj.com

*/

#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "chain.h"
#include "expand.h"

static void loop(void) {
    char *line;
    int status;
    int last_exit_code = 0;

    do {
        char *prompt_str = prompt_generate(last_exit_code);
        printf("%s", prompt_str);
        line = read_line();
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
    colors_init();
    config_init();
    prompt_init();
    config_load_default();
    if (shell_config.show_welcome) {
        color_print(COLOR_BOLD COLOR_CYAN, "%s", HASH_NAME);
        printf(" v%s\n", HASH_VERSION);
        printf("Type ");
        color_print(COLOR_YELLOW, "'exit'");
        printf(" to quit\n\n");
    }
    loop();
    return EXIT_SUCCESS;
}
