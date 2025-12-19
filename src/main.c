/*

hash - Command line interface (shell) for the Linux operating system.
https://github.com/juliojimenez/hash
Apache 2.0

Julio Jimenez, julio@julioj.com

*/

#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

int main(/*int argc, char **argv*/) {
    printf("hash v%s\n", HASH_VERSION);
    printf("Type `exit` to quit\n\n");

    return EXIT_SUCCESS;
}
