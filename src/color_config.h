#ifndef COLOR_CONFIG_H
#define COLOR_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_COLOR_CODE 32

// Color configuration for different semantic elements
typedef struct {
    // Prompt colors (Phase 1)
    char prompt[MAX_COLOR_CODE];           // Default: bold
    char prompt_error[MAX_COLOR_CODE];     // When last command failed
    char prompt_path[MAX_COLOR_CODE];      // Path in prompt
    char prompt_git_clean[MAX_COLOR_CODE]; // Git branch (clean)
    char prompt_git_dirty[MAX_COLOR_CODE]; // Git branch (dirty)
    char prompt_git_text[MAX_COLOR_CODE];  // "git:" text
    char prompt_git_branch[MAX_COLOR_CODE]; // Branch name

    // Syntax highlighting colors (Phase 2)
    char syn_command[MAX_COLOR_CODE];      // Valid commands
    char syn_builtin[MAX_COLOR_CODE];      // Builtin commands
    char syn_invalid[MAX_COLOR_CODE];      // Invalid commands
    char syn_string[MAX_COLOR_CODE];       // Quoted strings
    char syn_variable[MAX_COLOR_CODE];     // $VAR, ${VAR}
    char syn_operator[MAX_COLOR_CODE];     // |, &&, ||, ;
    char syn_redirect[MAX_COLOR_CODE];     // >, <, >>
    char syn_comment[MAX_COLOR_CODE];      // # comments

    // Autosuggestion color (Phase 3)
    char suggestion[MAX_COLOR_CODE];       // Muted suggestion text

    // Dangerous command colors (Phase 4)
    char danger[MAX_COLOR_CODE];           // Medium danger (bold red)
    char danger_high[MAX_COLOR_CODE];      // High danger (white on red bg)

    // Completion colors (Phase 5)
    char comp_directory[MAX_COLOR_CODE];   // Directories in completion

    // Feature toggles
    bool syntax_highlight_enabled;
    bool autosuggestion_enabled;
    bool danger_highlight_enabled;
} ColorConfig;

// Global color configuration
extern ColorConfig color_config;

/**
 * Initialize color configuration with defaults
 */
void color_config_init(void);

/**
 * Load color configuration from environment variables
 */
void color_config_load_env(void);

/**
 * Parse a color string (e.g., "bold,red" or "bright_blue")
 *
 * Stores ANSI escape sequence in buf
 *
 * @param color_str The color string to parse
 * @param buf The buffer to write the parsed result
 * @param buf_size Size of the buffer
 *
 * @return Returns 0 on success, -1 on error
 */
int color_config_parse(const char *color_str, char *buf, size_t buf_size);

/**
 * Set a specific color element by name
 *
 * @param element The element to set color for
 * @param value The value to set to
 *
 * @return Returns 0 on success, -1 on error
 */
int color_config_set(const char *element, const char *value);

/**
 * Get color for element (respects NO_COLOR and colors_enabled)
 *
 * @param element_color The element to get the color for
 *
 * @return Returns the color code or empty string if colors disabled
 */
const char *color_config_get(const char *element_color);

#endif // COLOR_CONFIG_H
