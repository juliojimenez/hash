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

// Shell options (POSIX set options)
typedef struct {
    bool nounset;      // -u: Treat unset variables as an error
    bool errexit;      // -e: Exit on error (not fully implemented)
    bool xtrace;       // -x: Print commands before execution (not fully implemented)
    bool verbose;      // -v: Print input lines (not fully implemented)
    bool noclobber;    // -C: Don't overwrite files with > (not fully implemented)
    bool allexport;    // -a: Export all variables (not fully implemented)
    bool monitor;      // -m: Enable job control (monitor mode)
    bool nonlexicalctrl; // Enable dynamic scoping for break/continue across functions
    bool nolog;        // Disable command history logging
} ShellOptions;

// Configuration structure
typedef struct {
    Alias aliases[MAX_ALIASES];
    int alias_count;
    bool colors_enabled;
    bool show_welcome;
    ShellOptions options;
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

// Load config file silently (no error if file doesn't exist)
int config_load_silent(const char *filepath);

// Load startup files based on shell type
// is_login_shell: true if invoked as login shell (argv[0] starts with '-' or --login)
void config_load_startup_files(bool is_login_shell);

// Load logout files for login shell exit
// Executes ~/.hash_logout if it exists (for login shells only)
void config_load_logout_files(void);

// Shell option functions
void shell_options_init(void);
bool shell_option_nounset(void);
void shell_option_set_nounset(bool value);
bool shell_option_errexit(void);
void shell_option_set_errexit(bool value);
bool shell_option_monitor(void);
void shell_option_set_monitor(bool value);
bool shell_option_nonlexicalctrl(void);
void shell_option_set_nonlexicalctrl(bool value);
bool shell_option_nolog(void);
void shell_option_set_nolog(bool value);

#endif // CONFIG_H
