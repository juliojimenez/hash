#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include "config.h"
#include "colors.h"
#include "prompt.h"
#include "hash.h"
#include "safe_string.h"

Config shell_config;

// Initialize config with defaults
void config_init(void) {
    shell_config.alias_count = 0;
    shell_config.colors_enabled = true;
    shell_config.show_welcome = true;
    memset(shell_config.aliases, 0, sizeof(shell_config.aliases));
}

// Trim whitespace from string
static char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + safe_strlen(str, sizeof(str)) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Add an alias
int config_add_alias(const char *name, const char *value) {
    if (!name || !value) return -1;
    if (safe_strlen(name, sizeof(name)) >= MAX_ALIAS_NAME || safe_strlen(value, sizeof(value)) >= MAX_ALIAS_VALUE) {
        return -1;
    }

    // Check if alias already exists (update it)
    for (int i = 0; i < shell_config.alias_count; i++) {
        if (strcmp(shell_config.aliases[i].name, name) == 0) {
            safe_strcpy(shell_config.aliases[i].value, value, MAX_ALIAS_VALUE);
            return 0;
        }
    }

    // Add new alias
    if (shell_config.alias_count >= MAX_ALIASES) {
        color_error("%s: too many aliases (max %d)", HASH_NAME, MAX_ALIASES);
        return -1;
    }

    safe_strcpy(shell_config.aliases[shell_config.alias_count].name, name, MAX_ALIAS_NAME);
    safe_strcpy(shell_config.aliases[shell_config.alias_count].value, value, MAX_ALIAS_VALUE);

    shell_config.alias_count++;
    return 0;
}

// Get alias value
const char *config_get_alias(const char *name) {
    if (!name) return NULL;

    for (int i = 0; i < shell_config.alias_count; i++) {
        if (strcmp(shell_config.aliases[i].name, name) == 0) {
            return shell_config.aliases[i].value;
        }
    }

    return NULL;
}

// Remove an alias
int config_remove_alias(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < shell_config.alias_count; i++) {
        if (strcmp(shell_config.aliases[i].name, name) == 0) {
            // Shift remaining aliases
            for (int j = i; j < shell_config.alias_count - 1; j++) {
                shell_config.aliases[j] = shell_config.aliases[j + 1];
            }
            shell_config.alias_count--;
            return 0;
        }
    }

    return -1;
}

// List all aliases
void config_list_aliases(void) {
    if (shell_config.alias_count == 0) {
        printf("No aliases defined\n");
        return;
    }

    for (int i = 0; i < shell_config.alias_count; i++) {
        color_print(COLOR_CYAN, "%s", shell_config.aliases[i].name);
        printf("='%s'\n", shell_config.aliases[i].value);
    }
}

// Process a single config line
int config_process_line(char *line) {
    if (!line) return -1;

    // Trim whitespace
    line = trim_whitespace(line);

    // Skip empty lines and comments
    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }

    // Handle "alias name='value'" or "alias name=value"
    if (strncmp(line, "alias ", 6) == 0) {
        char *alias_def = line + 6;
        alias_def = trim_whitespace(alias_def);

        char *equals = strchr(alias_def, '=');
        if (!equals) {
            color_warning("%s: invalid alias format: %s", HASH_NAME, line);
            return -1;
        }

        *equals = '\0';
        char *name = trim_whitespace(alias_def);
        char *value = trim_whitespace(equals + 1);

        // Remove quotes if present
        if ((value[0] == '"' || value[0] == '\'') &&
            value[0] == value[safe_strlen(value, sizeof(value)) - 1]) {
            value[safe_strlen(value, sizeof(value)) - 1] = '\0';
            value++;
        }

        return config_add_alias(name, value);
    }

    // Handle "export VAR=value"
    if (strncmp(line, "export ", 7) == 0) {
        char *export_def = line + 7;
        export_def = trim_whitespace(export_def);

        char *equals = strchr(export_def, '=');
        if (!equals) {
            color_warning("%s: invalid export format: %s", HASH_NAME, line);
            return -1;
        }

        *equals = '\0';
        char *name = trim_whitespace(export_def);
        char *value = trim_whitespace(equals + 1);

        // Remove quotes if present
        if ((value[0] == '"' || value[0] == '\'') &&
            value[0] == value[safe_strlen(value, sizeof(value)) - 1]) {
            value[safe_strlen(value, sizeof(value)) - 1] = '\0';
            value++;
        }

        setenv(name, value, 1);
        return 0;
    }

    // Handle "set option=value"
    if (strncmp(line, "set ", 4) == 0) {
        char *set_def = line + 4;
        set_def = trim_whitespace(set_def);

        if (strcmp(set_def, "colors=on") == 0) {
            shell_config.colors_enabled = true;
            colors_enable();
            return 0;
        } else if (strcmp(set_def, "colors=off") == 0) {
            shell_config.colors_enabled = false;
            colors_disable();
            return 0;
        } else if (strcmp(set_def, "welcome=on") == 0) {
            shell_config.show_welcome = true;
            return 0;
        } else if (strcmp(set_def, "welcome=off") == 0) {
            shell_config.show_welcome = false;
            return 0;
        }

        // Handle PS1 setting
        if (strncmp(set_def, "PS1=", 4) == 0) {
            char *ps1_value = set_def + 4;

            // Remove quotes if present
            if ((ps1_value[0] == '"' || ps1_value[0] == '\'') &&
                ps1_value[0] == ps1_value[safe_strlen(ps1_value, sizeof(ps1_value)) - 1]) {
                ps1_value[strlen(ps1_value) - 1] = '\0';
                ps1_value++;
            }

            prompt_set_ps1(ps1_value);
            return 0;
        }

        color_warning("%s: unknown option: %s", HASH_NAME, set_def);
        return -1;
    }

    // Unknown directive
    color_warning("%s: unknown directive in config: %s", HASH_NAME, line);
    return -1;
}

// Load config from file
int config_load(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return -1;
    }

    char line[MAX_CONFIG_LINE];
    int line_num = 0;
    int errors = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        // Ensure null termination (defense in depth)
        line[MAX_CONFIG_LINE - 1] = '\0';

        // Remove newline characters safely
        size_t len = safe_strlen(line, sizeof(line));
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        if (config_process_line(line) != 0) {
            errors++;
        }
    }

    fclose(fp);
    return errors > 0 ? -1 : 0;
}

// Load default config file (~/.hashrc)
int config_load_default(void) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (!home) {
        return -1;
    }

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.hashrc", home);

    // Check if file exists
    if (access(config_path, F_OK) != 0) {
        return 0; // Not an error if file doesn't exist
    }

    return config_load(config_path);
}
