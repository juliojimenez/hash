#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "color_config.h"
#include "colors.h"
#include "safe_string.h"

// Global color configuration instance
ColorConfig color_config;

// Color name to ANSI code mapping
typedef struct {
    const char *name;
    const char *code;
} ColorMapping;

static const ColorMapping color_names[] = {
    // Basic foreground colors
    {"black", COLOR_BLACK},
    {"red", COLOR_RED},
    {"green", COLOR_GREEN},
    {"yellow", COLOR_YELLOW},
    {"blue", COLOR_BLUE},
    {"magenta", COLOR_MAGENTA},
    {"cyan", COLOR_CYAN},
    {"white", COLOR_WHITE},

    // Bright foreground colors
    {"bright_black", COLOR_BRIGHT_BLACK},
    {"bright_red", COLOR_BRIGHT_RED},
    {"bright_green", COLOR_BRIGHT_GREEN},
    {"bright_yellow", COLOR_BRIGHT_YELLOW},
    {"bright_blue", COLOR_BRIGHT_BLUE},
    {"bright_magenta", COLOR_BRIGHT_MAGENTA},
    {"bright_cyan", COLOR_BRIGHT_CYAN},
    {"bright_white", COLOR_BRIGHT_WHITE},

    // Background colors
    {"bg_black", COLOR_BG_BLACK},
    {"bg_red", COLOR_BG_RED},
    {"bg_green", COLOR_BG_GREEN},
    {"bg_yellow", COLOR_BG_YELLOW},
    {"bg_blue", COLOR_BG_BLUE},
    {"bg_magenta", COLOR_BG_MAGENTA},
    {"bg_cyan", COLOR_BG_CYAN},
    {"bg_white", COLOR_BG_WHITE},

    // Styles
    {"bold", COLOR_BOLD},
    {"dim", COLOR_DIM},
    {"underline", COLOR_UNDERLINE},
    {"blink", COLOR_BLINK},
    {"reverse", COLOR_REVERSE},
    {"reset", COLOR_RESET},

    {NULL, NULL}
};

// Environment variable to config field mapping
typedef struct {
    const char *env_var;
    size_t offset;
} EnvMapping;

static const EnvMapping env_map[] = {
    {"HASH_COLOR_PROMPT", offsetof(ColorConfig, prompt)},
    {"HASH_COLOR_PROMPT_ERROR", offsetof(ColorConfig, prompt_error)},
    {"HASH_COLOR_PATH", offsetof(ColorConfig, prompt_path)},
    {"HASH_COLOR_GIT_CLEAN", offsetof(ColorConfig, prompt_git_clean)},
    {"HASH_COLOR_GIT_DIRTY", offsetof(ColorConfig, prompt_git_dirty)},
    {"HASH_COLOR_GIT_TEXT", offsetof(ColorConfig, prompt_git_text)},
    {"HASH_COLOR_GIT_BRANCH", offsetof(ColorConfig, prompt_git_branch)},
    {"HASH_COLOR_COMMAND", offsetof(ColorConfig, syn_command)},
    {"HASH_COLOR_BUILTIN", offsetof(ColorConfig, syn_builtin)},
    {"HASH_COLOR_INVALID", offsetof(ColorConfig, syn_invalid)},
    {"HASH_COLOR_STRING", offsetof(ColorConfig, syn_string)},
    {"HASH_COLOR_VARIABLE", offsetof(ColorConfig, syn_variable)},
    {"HASH_COLOR_OPERATOR", offsetof(ColorConfig, syn_operator)},
    {"HASH_COLOR_REDIRECT", offsetof(ColorConfig, syn_redirect)},
    {"HASH_COLOR_COMMENT", offsetof(ColorConfig, syn_comment)},
    {"HASH_COLOR_SUGGESTION", offsetof(ColorConfig, suggestion)},
    {"HASH_COLOR_DANGER", offsetof(ColorConfig, danger)},
    {"HASH_COLOR_DANGER_HIGH", offsetof(ColorConfig, danger_high)},
    {"HASH_COLOR_DIRECTORY", offsetof(ColorConfig, comp_directory)},
    {NULL, 0}
};

// Element name to config field mapping (for set color.* commands)
static const EnvMapping element_map[] = {
    {"prompt", offsetof(ColorConfig, prompt)},
    {"prompt_error", offsetof(ColorConfig, prompt_error)},
    {"path", offsetof(ColorConfig, prompt_path)},
    {"git_clean", offsetof(ColorConfig, prompt_git_clean)},
    {"git_dirty", offsetof(ColorConfig, prompt_git_dirty)},
    {"git_text", offsetof(ColorConfig, prompt_git_text)},
    {"git_branch", offsetof(ColorConfig, prompt_git_branch)},
    {"command", offsetof(ColorConfig, syn_command)},
    {"builtin", offsetof(ColorConfig, syn_builtin)},
    {"invalid", offsetof(ColorConfig, syn_invalid)},
    {"string", offsetof(ColorConfig, syn_string)},
    {"variable", offsetof(ColorConfig, syn_variable)},
    {"operator", offsetof(ColorConfig, syn_operator)},
    {"redirect", offsetof(ColorConfig, syn_redirect)},
    {"comment", offsetof(ColorConfig, syn_comment)},
    {"suggestion", offsetof(ColorConfig, suggestion)},
    {"danger", offsetof(ColorConfig, danger)},
    {"danger_high", offsetof(ColorConfig, danger_high)},
    {"directory", offsetof(ColorConfig, comp_directory)},
    {NULL, 0}
};

// Look up a color name and return its ANSI code
static const char *lookup_color(const char *name) {
    for (int i = 0; color_names[i].name != NULL; i++) {
        if (strcmp(color_names[i].name, name) == 0) {
            return color_names[i].code;
        }
    }
    return NULL;
}

// Initialize color configuration with defaults
void color_config_init(void) {
    // Prompt colors - bold by default for entire prompt
    safe_strcpy(color_config.prompt, COLOR_BOLD, MAX_COLOR_CODE);
    safe_strcpy(color_config.prompt_error, COLOR_BOLD COLOR_RED, MAX_COLOR_CODE);
    safe_strcpy(color_config.prompt_path, COLOR_BOLD COLOR_BLUE, MAX_COLOR_CODE);
    safe_strcpy(color_config.prompt_git_clean, COLOR_GREEN, MAX_COLOR_CODE);
    safe_strcpy(color_config.prompt_git_dirty, COLOR_YELLOW, MAX_COLOR_CODE);
    safe_strcpy(color_config.prompt_git_text, "", MAX_COLOR_CODE);  // Inherits from prompt
    safe_strcpy(color_config.prompt_git_branch, COLOR_CYAN, MAX_COLOR_CODE);

    // Syntax highlighting colors
    safe_strcpy(color_config.syn_command, COLOR_GREEN, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_builtin, COLOR_CYAN, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_invalid, COLOR_RED, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_string, COLOR_YELLOW, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_variable, COLOR_MAGENTA, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_operator, COLOR_BRIGHT_BLACK, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_redirect, COLOR_BLUE, MAX_COLOR_CODE);
    safe_strcpy(color_config.syn_comment, COLOR_BRIGHT_BLACK, MAX_COLOR_CODE);

    // Autosuggestion - muted gray
    safe_strcpy(color_config.suggestion, COLOR_BRIGHT_BLACK, MAX_COLOR_CODE);

    // Danger highlighting
    safe_strcpy(color_config.danger, COLOR_BOLD COLOR_RED, MAX_COLOR_CODE);      // Medium: bold red
    safe_strcpy(color_config.danger_high, COLOR_BOLD COLOR_WHITE COLOR_BG_RED, MAX_COLOR_CODE);  // High: white on red

    // Completion colors
    safe_strcpy(color_config.comp_directory, COLOR_BOLD COLOR_BLUE, MAX_COLOR_CODE);

    // Feature toggles - all enabled by default
    color_config.syntax_highlight_enabled = true;
    color_config.autosuggestion_enabled = true;
    color_config.danger_highlight_enabled = true;
}

// Parse a color string (e.g., "bold,red" or "bright_blue")
int color_config_parse(const char *color_str, char *buf, size_t buf_size) {
    if (!color_str || !buf || buf_size == 0) {
        return -1;
    }

    buf[0] = '\0';

    // Make a copy to tokenize
    char copy[256];
    safe_strcpy(copy, color_str, sizeof(copy));

    // Tokenize by comma
    char *saveptr = NULL;
    char *token = strtok_r(copy, ",", &saveptr);

    while (token != NULL) {
        // Trim leading whitespace
        while (*token == ' ' || *token == '\t') token++;

        // Trim trailing whitespace
        size_t len = strlen(token);
        while (len > 0 && (token[len-1] == ' ' || token[len-1] == '\t')) {
            token[--len] = '\0';
        }

        // Look up the color
        const char *code = lookup_color(token);
        if (code) {
            safe_strcat(buf, code, buf_size);
        } else {
            // Unknown color name
            return -1;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

// Set a specific color element by name
int color_config_set(const char *element, const char *value) {
    if (!element || !value) {
        return -1;
    }

    // Find the element in the map
    for (int i = 0; element_map[i].env_var != NULL; i++) {
        if (strcmp(element_map[i].env_var, element) == 0) {
            char *field = (char *)&color_config + element_map[i].offset;
            return color_config_parse(value, field, MAX_COLOR_CODE);
        }
    }

    return -1;  // Unknown element
}

// Load color configuration from environment variables
void color_config_load_env(void) {
    for (int i = 0; env_map[i].env_var != NULL; i++) {
        const char *value = getenv(env_map[i].env_var);
        if (value) {
            char *field = (char *)&color_config + env_map[i].offset;
            color_config_parse(value, field, MAX_COLOR_CODE);
        }
    }

    // Check feature toggle env vars
    const char *syntax = getenv("HASH_SYNTAX_HIGHLIGHT");
    if (syntax) {
        color_config.syntax_highlight_enabled = (strcmp(syntax, "1") == 0 ||
                                                  strcmp(syntax, "on") == 0 ||
                                                  strcmp(syntax, "true") == 0);
    }

    const char *suggest = getenv("HASH_AUTOSUGGEST");
    if (suggest) {
        color_config.autosuggestion_enabled = (strcmp(suggest, "1") == 0 ||
                                                strcmp(suggest, "on") == 0 ||
                                                strcmp(suggest, "true") == 0);
    }

    const char *danger = getenv("HASH_DANGER_HIGHLIGHT");
    if (danger) {
        color_config.danger_highlight_enabled = (strcmp(danger, "1") == 0 ||
                                                  strcmp(danger, "on") == 0 ||
                                                  strcmp(danger, "true") == 0);
    }
}

// Get color for element (respects NO_COLOR and colors_enabled)
const char *color_config_get(const char *element_color) {
    if (!colors_enabled || !element_color) {
        return "";
    }

    return element_color;
}
