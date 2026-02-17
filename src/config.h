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
    bool use_colors;
    bool show_welcome;
    ShellOptions options;
} Config;

// Global config
extern Config shell_config;

/**
 * Initialize config with defaults
 */
void config_init(void);

/**
 * Load config from file
 *
 * @param filepath Path to the file
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_load(const char *filepath);

/**
 * Load default config file (~/.hashrc)
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_load_default(void);

/**
 * Add an alias
 *
 * @param name The alias name
 * @param value The value of alias
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_add_alias(const char *name, const char *value);

/**
 * Get alias value (returns NULL if not found)
 *
 * @param name Name of alias
 *
 * @return Returns alias value or NULL if not found
 */
const char *config_get_alias(const char *name);

/**
 * Remove an alias
 *
 * @param name Name of alias to remove
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_remove_alias(const char *name);

/**
 * List all aliases
 */
void config_list_aliases(void);

/**
 * Process a config line (returns 0 on success)
 *
 * @param line The line to process
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_process_line(char *line);

/**
 * Load config file silently (no error if file doesn't exist)
 *
 * @param filepath Path to file
 *
 * @return Returns -1 on error, 0 otherwise.
 */
int config_load_silent(const char *filepath);

/**
 * Load startup files based on shell type
 *
 * @param is_login_shell true if invoked as login shell (argv[0] starts with '-' or --login)
 */
void config_load_startup_files(bool is_login_shell);

/**
 * Load logout files for login shell exit
 *
 * Executes ~/.hash_logout if it exists (for login shells only)
 */
void config_load_logout_files(void);

/**
 * Initialize the shell options
 */
void shell_options_init(void);

/**
 * Get the nounset option value
 *
 * @return Value of nounset option
 */
bool shell_option_nounset(void);

/**
 * Set the nounset option value
 *
 * @param value The value to set to
 */
void shell_option_set_nounset(bool value);

/**
 * Get the errexit option value
 *
 * @return Value of errexit option
 */
bool shell_option_errexit(void);

/**
 * Set the errexit option value
 *
 * @param value The value to set to
 */
void shell_option_set_errexit(bool value);

/**
 * Get the monitor option value
 *
 * @return Value of monitor option
 */
bool shell_option_monitor(void);

/**
 * Set the monitor option value
 *
 * @param value The value to set to
 */
void shell_option_set_monitor(bool value);

/**
 * Get the nonlexicalctrl option value
 *
 * @return Value of nonlexicalctrl option
 */
bool shell_option_nonlexicalctrl(void);

/**
 * Set the nonlexicalctrl option value
 *
 * @param value The value to set to
 */
void shell_option_set_nonlexicalctrl(bool value);

/**
 * Get the nolog option value
 *
 * @return Value of nolog option
 */
bool shell_option_nolog(void);

/**
 * Set the nolog option value
 *
 * @param value The value to set to
 */
void shell_option_set_nolog(bool value);

#endif // CONFIG_H
