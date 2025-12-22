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

static void loop(void) {
    char *line;
    char **args;
    int status;

    do {
        color_print(COLOR_BOLD COLOR_BLUE, "#> ");
        line = read_line();
        args = parse_line(line);
        status = execute(args);
        free(line);
        free(args);
    } while(status);
}

int main(/*int argc, char **argv*/) {
    colors_init();
    color_print(COLOR_BOLD COLOR_CYAN, "%s", HASH_NAME);
    printf(" v%s\n", HASH_VERSION);
    printf("Type ");
    color_print(COLOR_YELLOW, "'exit'");
    printf(" to quit\n\n");

    loop();

    return EXIT_SUCCESS;
}
