#define _POSIX_C_SOURCE 200809L

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
#include "script.h"
#include "varexpand.h"
#include "expand.h"

Config shell_config;

// Track if we're running interactively (for history tracking, job control, etc.)
bool is_interactive = false;

// Initialize config with defaults
void config_init(void) {
    shell_config.alias_count = 0;
    shell_config.colors_enabled = true;
    shell_config.show_welcome = true;
    memset(shell_config.aliases, 0, sizeof(shell_config.aliases));
    shell_options_init();
}

// Initialize shell options to defaults
void shell_options_init(void) {
    shell_config.options.nounset = false;
    shell_config.options.errexit = false;
    shell_config.options.xtrace = false;
    shell_config.options.verbose = false;
    shell_config.options.noclobber = false;
    shell_config.options.allexport = false;
    shell_config.options.monitor = false;
}

// Get the nounset option value
bool shell_option_nounset(void) {
    return shell_config.options.nounset;
}

// Set the nounset option value
void shell_option_set_nounset(bool value) {
    shell_config.options.nounset = value;
}

// Get the errexit option value
bool shell_option_errexit(void) {
    return shell_config.options.errexit;
}

// Set the errexit option value
void shell_option_set_errexit(bool value) {
    shell_config.options.errexit = value;
}

// Get the monitor option value
bool shell_option_monitor(void) {
    return shell_config.options.monitor;
}

// Set the monitor option value
void shell_option_set_monitor(bool value) {
    shell_config.options.monitor = value;
}

// Get the nonlexicalctrl option value
bool shell_option_nonlexicalctrl(void) {
    return shell_config.options.nonlexicalctrl;
}

// Set the nonlexicalctrl option value
void shell_option_set_nonlexicalctrl(bool value) {
    shell_config.options.nonlexicalctrl = value;
}

// Get the nolog option value
bool shell_option_nolog(void) {
    return shell_config.options.nolog;
}

// Set the nolog option value
void shell_option_set_nolog(bool value) {
    shell_config.options.nolog = value;
}

// Trim whitespace from string
static char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Add an alias
int config_add_alias(const char *name, const char *value) {
    if (!name || !value) return -1;
    if (strlen(name) >= MAX_ALIAS_NAME || strlen(value) >= MAX_ALIAS_VALUE) {
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
        return;  // Silently return if no aliases
    }

    for (int i = 0; i < shell_config.alias_count; i++) {
        printf("alias %s='%s'\n", shell_config.aliases[i].name,
               shell_config.aliases[i].value);
    }
}

// Process a single config line (for simple .hashrc format)
// This handles hash-specific directives: alias, export, set
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
            return -1;  // Silently fail for bad format
        }

        *equals = '\0';
        const char *name = trim_whitespace(alias_def);
        char *value = trim_whitespace(equals + 1);

        // Remove quotes if present
        size_t val_len = strlen(value);
        if (val_len >= 2 &&
            (value[0] == '"' || value[0] == '\'') &&
            value[0] == value[val_len - 1]) {
            value[val_len - 1] = '\0';
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
            // export without assignment - just mark variable for export
            // For now, just ignore
            return 0;
        }

        *equals = '\0';
        const char *name = trim_whitespace(export_def);
        char *value = trim_whitespace(equals + 1);

        // Remove quotes if present
        size_t val_len = strlen(value);
        if (val_len >= 2 &&
            (value[0] == '"' || value[0] == '\'') &&
            value[0] == value[val_len - 1]) {
            value[val_len - 1] = '\0';
            value++;
        }

        // First expand tilde if present (e.g., ~/bin)
        char *tilde_expanded = NULL;
        if (value[0] == '~') {
            tilde_expanded = expand_tilde_path(value);
            if (tilde_expanded) {
                value = tilde_expanded;
            }
        }

        // Expand variables in the value (e.g., $HOME, $PATH)
        char *var_expanded = varexpand_expand(value, 0);
        if (var_expanded) {
            setenv(name, var_expanded, 1);
            free(var_expanded);
        } else {
            setenv(name, value, 1);
        }

        free(tilde_expanded);  // Safe to free NULL
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
            size_t val_len = strlen(ps1_value);
            if (val_len >= 2 &&
                (ps1_value[0] == '"' || ps1_value[0] == '\'') &&
                ps1_value[0] == ps1_value[val_len - 1]) {
                ps1_value[val_len - 1] = '\0';
                ps1_value++;
            }

            prompt_set_ps1(ps1_value);
            return 0;
        }

        return -1;  // Unknown option
    }

    // Unknown directive - return error but don't print warning
    // (Warnings are printed by caller if needed)
    return -1;
}

// Load config from file (simple hash-specific format)
int config_load(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return -1;
    }

    char line[MAX_CONFIG_LINE];
    int errors = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Ensure null termination
        line[MAX_CONFIG_LINE - 1] = '\0';

        // Remove newline characters
        size_t len = strlen(line);
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

// Get home directory helper
static const char *get_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return home;

    // Fallback to passwd entry
    static char home_buf[1024];
    struct passwd pw;
    struct passwd *result = NULL;
    char buf[1024];

    if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
        safe_strcpy(home_buf, pw.pw_dir, sizeof(home_buf));
        return home_buf;
    }

    return NULL;
}

// Load default config file (~/.hashrc)
int config_load_default(void) {
    const char *home = get_home_dir();
    if (!home) return -1;

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.hashrc", home);

    if (access(config_path, F_OK) != 0) {
        return 0;  // Not an error if doesn't exist
    }

    return config_load(config_path);
}

// Load config file silently (no error if doesn't exist)
// For .hashrc files, use config_load which handles hash-specific directives
// For profile/login files, use script_execute_file_ex
int config_load_silent(const char *filepath) {
    if (access(filepath, F_OK) != 0) {
        return 0;  // File doesn't exist - not an error
    }
    if (access(filepath, R_OK) != 0) {
        return 0;  // Not readable - silently skip
    }

    // Check if this is a .hashrc file - use config_load for hash-specific directives
    const char *basename = strrchr(filepath, '/');
    basename = basename ? basename + 1 : filepath;

    if (strcmp(basename, ".hashrc") == 0) {
        // Use config_load which handles hash-specific directives (alias, set, export)
        return config_load(filepath);
    }

    // For other files (profile, login, etc.), use script execution
    // with silent errors for system files that may contain unsupported syntax
    return script_execute_file_ex(filepath, 0, NULL, true);
}

// Load startup files based on shell type
void config_load_startup_files(bool is_login_shell) {
    const char *home = get_home_dir();
    char path[1024];

    if (is_login_shell) {
        // ====================================================================
        // LOGIN SHELL STARTUP SEQUENCE
        // ====================================================================
        // Note: We intentionally skip /etc/profile because it typically
        // contains bash/sh-specific syntax that hash doesn't support.
        // Hash loads only hash-specific startup files.
        //
        // Load order:
        // 1. First of: ~/.hash_profile, ~/.hash_login
        // 2. ~/.hashrc (for interactive settings)

        if (home) {
            bool profile_loaded = false;

            // Try ~/.hash_profile first (hash-specific)
            snprintf(path, sizeof(path), "%s/.hash_profile", home);
            if (access(path, F_OK) == 0) {
                config_load_silent(path);
                profile_loaded = true;
            }

            // Try ~/.hash_login if no profile yet
            if (!profile_loaded) {
                snprintf(path, sizeof(path), "%s/.hash_login", home);
                if (access(path, F_OK) == 0) {
                    config_load_silent(path);
                }
            }

            // Note: We don't fall back to ~/.profile because it may contain
            // bash-specific syntax. Users who want shared profile settings
            // should source ~/.profile from ~/.hash_profile if desired.

            // Interactive config
            snprintf(path, sizeof(path), "%s/.hashrc", home);
            config_load_silent(path);
        }
    } else {
        // ====================================================================
        // INTERACTIVE NON-LOGIN SHELL
        // ====================================================================
        // Only load ~/.hashrc

        if (home) {
            snprintf(path, sizeof(path), "%s/.hashrc", home);
            config_load_silent(path);
        }
    }
}

// Load logout files for login shell exit
void config_load_logout_files(void) {
    const char *home = get_home_dir();
    char path[1024];

    if (!home) return;

    // Execute ~/.hash_logout if it exists
    snprintf(path, sizeof(path), "%s/.hash_logout", home);
    config_load_silent(path);
}
