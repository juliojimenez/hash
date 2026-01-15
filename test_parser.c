#include <stdio.h>
#include "src/parser.h"

int main() {
    const char *test = "printf %s \\&";
    printf("Input: %s\n", test);
    char **tokens = parse_line(test);
    for (int i = 0; tokens[i] != NULL; i++) {
        printf("Token %d: [", i);
        for (const char *p = tokens[i]; *p; p++) {
            if (*p >= 32 && *p <= 126) {
                printf("%c", *p);
            } else {
                printf("\\x%02x", (unsigned char)*p);
            }
        }
        printf("]\n");
    }
    return 0;
}
