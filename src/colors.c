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

__attribute__((format(printf, 3, 0)))
static void color_fprint_va(FILE *stream, const char *color, const char *format, va_list args) {
    if (colors_enabled) {
        fprintf(stream, "%s", color);
    }

    vfprintf(stream, format, args);

    if (colors_enabled) {
        fprintf(stream, "%s", COLOR_RESET);
    }
}

__attribute__((format(printf, 3, 0)))
static void color_fprintln_va(FILE *stream, const char *color, const char *format, va_list args) {
    if (colors_enabled) {
        fprintf(stream, "%s", color);
    }

    vfprintf(stream, format, args);

    if (colors_enabled) {
        printf("%s", COLOR_RESET);
    }

    fprintf(stream, "\n");
}

// Print with color
void color_print(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprint_va(stdout, color, format, args);
    va_end(args);
}

// Print with color and newline
void color_println(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprintln_va(stdout, color, format, args);
    va_end(args);
}

// Print error message (red)
void color_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprintln_va(stderr, COLOR_RED, format, args);
    va_end(args);
}

// Print success message (green)
void color_success(const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprintln_va(stdout, COLOR_GREEN, format, args);
    va_end(args);
}

// Print warning message (yellow)
void color_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprintln_va(stdout, COLOR_YELLOW, format, args);
    va_end(args);
}

// Print info message (cyan)
void color_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    color_fprintln_va(stdout, COLOR_CYAN, format, args);
    va_end(args);
}
