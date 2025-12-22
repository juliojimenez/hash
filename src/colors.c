#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "colors.h"

int colors_enabled = 1;

// Initialize color support
void colors_init(void) {
    // Check if stdout is a terminal
    if (!isatty(STDOUT_FILENO)) {
        colors_enabled = 0;
        return;
    }

    // Check TERM environment variable
    const char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
        colors_enabled = 0;
        return;
    }

    // Check NO_COLOR environment variable (https://no-color.org/)
    if (getenv("NO_COLOR") != NULL) {
        colors_enabled = 0;
        return;
    }

    colors_enabled = 1;
}

// Enable colors
void colors_enable(void) {
    colors_enabled = 1;
}

// Disable colors
void colors_disable(void) {
    colors_enabled = 0;
}

// Get color code (returns empty string if colors disabled)
const char *color_code(const char *code) {
    return colors_enabled ? code : "";
}

// Print with color
void color_print(const char *color, const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        printf("%s", color);
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }
}

// Print with color and newline
void color_println(const char *color, const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        printf("%s", color);
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }

    printf("\n");
}

// Print error message (red)
void color_error(const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        fprintf(stderr, "%s", COLOR_RED);
    }

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (colors_enabled) {
        fprintf(stderr, "%s", COLOR_RESET);
    }

    fprintf(stderr, "\n");
}

// Print success message (green)
void color_success(const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        printf("%s", COLOR_GREEN);
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }

    printf("\n");
}

// Print warning message (yellow)
void color_warning(const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        printf("%s", COLOR_YELLOW);
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }

    printf("\n");
}

// Print info message (cyan)
void color_info(const char *format, ...) {
    va_list args;

    if (colors_enabled) {
        printf("%s", COLOR_CYAN);
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }

    printf("\n");
}
