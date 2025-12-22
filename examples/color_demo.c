// Color demonstration for hash shell
// Compile: gcc -o color_demo color_demo.c ../src/colors.c -I../src

#include <stdio.h>
#include "colors.h"

void demo_basic_colors(void) {
    printf("\n=== Basic Colors ===\n");
    color_println(COLOR_BLACK, "Black text");
    color_println(COLOR_RED, "Red text");
    color_println(COLOR_GREEN, "Green text");
    color_println(COLOR_YELLOW, "Yellow text");
    color_println(COLOR_BLUE, "Blue text");
    color_println(COLOR_MAGENTA, "Magenta text");
    color_println(COLOR_CYAN, "Cyan text");
    color_println(COLOR_WHITE, "White text");
}

void demo_bright_colors(void) {
    printf("\n=== Bright Colors ===\n");
    color_println(COLOR_BRIGHT_BLACK, "Bright Black (Gray)");
    color_println(COLOR_BRIGHT_RED, "Bright Red");
    color_println(COLOR_BRIGHT_GREEN, "Bright Green");
    color_println(COLOR_BRIGHT_YELLOW, "Bright Yellow");
    color_println(COLOR_BRIGHT_BLUE, "Bright Blue");
    color_println(COLOR_BRIGHT_MAGENTA, "Bright Magenta");
    color_println(COLOR_BRIGHT_CYAN, "Bright Cyan");
    color_println(COLOR_BRIGHT_WHITE, "Bright White");
}

void demo_styles(void) {
    printf("\n=== Text Styles ===\n");
    color_println(COLOR_BOLD, "Bold text");
    color_println(COLOR_DIM, "Dim text");
    color_println(COLOR_UNDERLINE, "Underlined text");
    color_println(COLOR_REVERSE, "Reversed text");
}

void demo_combinations(void) {
    printf("\n=== Style Combinations ===\n");
    color_println(COLOR_BOLD COLOR_RED, "Bold Red");
    color_println(COLOR_BOLD COLOR_GREEN, "Bold Green");
    color_println(COLOR_UNDERLINE COLOR_BLUE, "Underlined Blue");
    color_println(COLOR_BOLD COLOR_UNDERLINE COLOR_CYAN, "Bold Underlined Cyan");
    color_println(COLOR_RED COLOR_BG_YELLOW, "Red on Yellow background");
}

void demo_convenience_functions(void) {
    printf("\n=== Convenience Functions ===\n");
    color_error("This is an error message");
    color_success("This is a success message");
    color_warning("This is a warning message");
    color_info("This is an info message");
}

void demo_mixed_output(void) {
    printf("\n=== Mixed Output ===\n");
    
    printf("Status: ");
    color_print(COLOR_GREEN, "ONLINE");
    printf(" | ");
    
    printf("CPU: ");
    color_print(COLOR_YELLOW, "45%%");
    printf(" | ");
    
    printf("Memory: ");
    color_print(COLOR_RED, "89%%");
    printf("\n");
}

void demo_prompt(void) {
    printf("\n=== Shell Prompts ===\n");
    color_print(COLOR_BOLD COLOR_GREEN, "user");
    printf("@");
    color_print(COLOR_BOLD COLOR_BLUE, "hostname");
    printf(":");
    color_print(COLOR_BOLD COLOR_CYAN, "~/projects");
    color_print(COLOR_BOLD COLOR_WHITE, "$ ");
    printf("echo hello\n");
    
    color_print(COLOR_BOLD COLOR_BLUE, "#> ");
    printf("ls -la\n");
}

void demo_tables(void) {
    printf("\n=== Colored Table ===\n");
    color_print(COLOR_BOLD, "%-15s %-10s %-10s\n", "Service", "Status", "Uptime");
    printf("%-15s ", "web-server");
    color_print(COLOR_GREEN, "%-10s", "running");
    printf(" %-10s\n", "5d 3h");
    
    printf("%-15s ", "database");
    color_print(COLOR_GREEN, "%-10s", "running");
    printf(" %-10s\n", "12d 7h");
    
    printf("%-15s ", "cache");
    color_print(COLOR_RED, "%-10s", "stopped");
    printf(" %-10s\n", "0d 0h");
}

int main(void) {
    colors_init();
    
    color_print(COLOR_BOLD COLOR_CYAN, "Hash Shell Color Demo\n");
    color_print(COLOR_DIM, "Demonstrating color capabilities\n");
    
    if (!colors_enabled) {
        printf("\nColors are disabled (NO_COLOR set or not a TTY)\n");
        return 0;
    }
    
    demo_basic_colors();
    demo_bright_colors();
    demo_styles();
    demo_combinations();
    demo_convenience_functions();
    demo_mixed_output();
    demo_prompt();
    demo_tables();
    
    printf("\n");
    color_success("Color demo complete!");
    
    return 0;
}

