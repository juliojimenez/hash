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
    bool noglob;       // -f: No pathname expansion
    bool noexec;       // -n: Read and parse commands but do not run them (not interactive)
    bool notify;       // -b: accepted; background job status is reported at the prompt
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
 * Apply one hash-specific option given as "name=value" (quotes already removed
 * by the shell): colors, welcome, PS1, color.<element>, syntax_highlight,
 * autosuggest, danger_highlight. Used by the set builtin.
 *
 * @return 0 when applied, -1 when the option is not one of ours
 */
int config_set_option(const char *assignment);

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

bool shell_option_xtrace(void);
bool shell_option_verbose(void);
bool shell_option_noclobber(void);
bool shell_option_allexport(void);
bool shell_option_noglob(void);
bool shell_option_noexec(void);

/**
 * The POSIX set options by letter (-a -b -C -e -f -h -m -n -u -v -x) and by
 * -o name, plus hash's own nolog and nonlexicalctrl. Return false for an
 * option the shell does not have.
 */
bool shell_option_apply_letter(char letter, bool on);
bool shell_option_apply_name(const char *name, bool on);

/** `set -o`: one "name<TAB>on|off" line per option; `set +o`: "set -o name" / "set +o name" lines. */
void shell_options_print(bool as_commands);

/** $-: the letters of the options in effect, plus i for an interactive shell. */
void shell_options_flags(char *buf, size_t size);

/**
 * Set the nolog option value
 *
 * @param value The value to set to
 */
void shell_option_set_nolog(bool value);

#endif // CONFIG_H
