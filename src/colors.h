#ifndef COLORS_H
#define COLORS_H

#include <stdio.h>

// Format attribute for printf-style functions
#ifdef __GNUC__
#define COLOR_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define COLOR_PRINTF_FORMAT(fmt, args)
#endif

// ANSI Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_UNDERLINE "\033[4m"
#define COLOR_BLINK   "\033[5m"
#define COLOR_REVERSE "\033[7m"

// Foreground colors
#define COLOR_BLACK   "\033[30m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

// Bright foreground colors
#define COLOR_BRIGHT_BLACK   "\033[90m"
#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"

// Background colors
#define COLOR_BG_BLACK   "\033[40m"
#define COLOR_BG_RED     "\033[41m"
#define COLOR_BG_GREEN   "\033[42m"
#define COLOR_BG_YELLOW  "\033[43m"
#define COLOR_BG_BLUE    "\033[44m"
#define COLOR_BG_MAGENTA "\033[45m"
#define COLOR_BG_CYAN    "\033[46m"
#define COLOR_BG_WHITE   "\033[47m"

// Color control
extern int colors_enabled;

// Initialize color support (checks if terminal supports colors)
void colors_init(void);

// Enable/disable colors
void colors_enable(void);
void colors_disable(void);

// Get color code (returns empty string if colors disabled)
const char *color_code(const char *code);

// Convenience functions for colored output
void color_print(const char *color, const char *format, ...) COLOR_PRINTF_FORMAT(2, 3);
void color_println(const char *color, const char *format, ...) COLOR_PRINTF_FORMAT(2, 3);
void color_error(const char *format, ...) COLOR_PRINTF_FORMAT(1, 2);
void color_success(const char *format, ...) COLOR_PRINTF_FORMAT(1, 2);
void color_warning(const char *format, ...) COLOR_PRINTF_FORMAT(1, 2);
void color_info(const char *format, ...) COLOR_PRINTF_FORMAT(1, 2);

#endif // COLORS_H
