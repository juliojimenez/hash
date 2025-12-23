// Example showing format string checking
// Compile with: gcc -Wall -Wformat -c format_check.c -I../src

#include "colors.h"

void good_examples(void) {
    // These will compile without warnings
    color_print(COLOR_RED, "Simple string");
    color_print(COLOR_GREEN, "Number: %d", 42);
    color_print(COLOR_BLUE, "String: %s, Number: %d", "test", 123);
    color_error("Error code: %d", 500);
    color_success("Processed %zu items", (size_t)100);
}

void bad_examples(void) {
    // These will generate compiler warnings with -Wformat

    // Warning: format '%s' expects argument of type 'char *', but argument has type 'int'
    color_print(COLOR_RED, "Number: %s", 42);

    // Warning: format '%d' expects argument of type 'int', but argument has type 'char *'
    color_print(COLOR_GREEN, "String: %d", "hello");

    // Warning: too many arguments for format
    color_print(COLOR_BLUE, "One arg: %d", 1, 2);

    // Warning: too few arguments for format
    color_print(COLOR_YELLOW, "Two args: %d %d", 1);
}

int main(void) {
    colors_init();
    good_examples();
    bad_examples();
    return 0;
}
