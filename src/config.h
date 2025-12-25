#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define MAX_CONFIG_LINE 1024
#define MAX_ALIASES 100
#define MAX_ALIAS_NAME 64
#define MAX_ALIAS_VALUE 512

// Alias structure
typedef struct {
    char name[MAX_ALIAS_NAME];
    char value[MAX_ALIAS_VALUE];
} Alias;

// Configuration structure
typedef struct {
    Alias aliases[MAX_ALIASES];
    int alias_count;
    bool colors_enabled;
    bool show_welcome;
} Config;

// Global config
extern Config shell_config;

// Initialize config with defaults
void config_init(void);

// Load config from file
int config_load(const char *filepath);

// Load default config file (~/.hashrc)
int config_load_default(void);

// Add an alias
int config_add_alias(const char *name, const char *value);

// Get alias value (returns NULL if not found)
const char *config_get_alias(const char *name);

// Remove an alias
int config_remove_alias(const char *name);

// List all aliases
void config_list_aliases(void);

// Process a config line (returns 0 on success)
int config_process_line(char *line);

#endif // CONFIG_H
