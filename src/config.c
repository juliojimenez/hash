#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include "config.h"
#include "colors.h"
#include "color_config.h"
#include "prompt.h"
#include "hash.h"
#include "safe_string.h"
#include "script.h"
#include "varexpand.h"
#include "expand.h"
#include "cmdsub.h"
#include "utils.h"

Config shell_config;

// Track if we're running interactively (for history tracking, job control, etc.)
bool is_interactive = false;

// Flag to indicate we're in a command substitution child and should exec directly
bool exec_directly_in_child = false;

// Initialize config with defaults
void config_init(void) {
    shell_config.alias_count = 0;
    shell_config.use_colors = true;
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

// Load config file silently (no error if doesn't exist)
// Apply one hash-specific option, "name=value" with the quotes already removed
// by the shell: colors, welcome, PS1, color.<element>, syntax_highlight,
// autosuggest and danger_highlight. The set builtin hands every argument that
// carries '=' to this function, so the same lines work in ~/.hashrc, in a
// script and at the prompt. Returns 0 when the option was applied and -1 when
// it is not one of ours.
int config_set_option(const char *assignment) {
    if (!assignment) return -1;

    if (strcmp(assignment, "colors=on") == 0) {
        shell_config.use_colors = true;
        colors_enable();
        return 0;
    }
    if (strcmp(assignment, "colors=off") == 0) {
        shell_config.use_colors = false;
        colors_disable();
        return 0;
    }
    if (strcmp(assignment, "welcome=on") == 0) {
        shell_config.show_welcome = true;
        return 0;
    }
    if (strcmp(assignment, "welcome=off") == 0) {
        shell_config.show_welcome = false;
        return 0;
    }
    if (strncmp(assignment, "PS1=", 4) == 0) {
        prompt_set_ps1(assignment + 4);
        return 0;
    }

    // set color.<element>=<value>, e.g. color.prompt=bold,blue
    if (strncmp(assignment, "color.", 6) == 0) {
        const char *element = assignment + 6;
        const char *equals = strchr(element, '=');
        if (!equals) return -1;
        size_t len = (size_t)(equals - element);
        char name[64];
        if (len == 0 || len >= sizeof(name)) return -1;
        memcpy(name, element, len);
        name[len] = '\0';
        return color_config_set(name, equals + 1);
    }

    // Feature toggles: name=on|off
    static const struct { const char *name; bool *flag; } toggles[] = {
        {"syntax_highlight", &color_config.syntax_highlight_enabled},
        {"autosuggest", &color_config.autosuggestion_enabled},
        {"danger_highlight", &color_config.danger_highlight_enabled},
    };
    for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); i++) {
        size_t n = strlen(toggles[i].name);
        if (strncmp(assignment, toggles[i].name, n) == 0 && assignment[n] == '=') {
            const char *value = assignment + n + 1;
            if (strcmp(value, "on") == 0) { *toggles[i].flag = true; return 0; }
            if (strcmp(value, "off") == 0) { *toggles[i].flag = false; return 0; }
            return -1;
        }
    }

    return -1;  // Unknown option
}

// Run a startup file through the shell. ~/.hashrc is the user's own file and a
// shell script like any other (aliases, exports, set options, functions,
// conditionals, source, ...), so its errors are shown. The profile, login and
// logout files run with hash's own error messages suppressed, because they may
// carry syntax from other shells. A missing or unreadable file is not an error.
int config_load_silent(const char *filepath) {
    if (access(filepath, F_OK) != 0) {
        return 0;  // File doesn't exist - not an error
    }
    if (access(filepath, R_OK) != 0) {
        return 0;  // Not readable - silently skip
    }

    const char *basename = strrchr(filepath, '/');
    basename = basename ? basename + 1 : filepath;
    bool quiet = strcmp(basename, ".hashrc") != 0;

    return script_execute_file_ex(filepath, 0, NULL, quiet);
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
            (void)config_load_silent(path);
        }
    } else {
        // ====================================================================
        // INTERACTIVE NON-LOGIN SHELL
        // ====================================================================
        // Only load ~/.hashrc

        if (home) {
            snprintf(path, sizeof(path), "%s/.hashrc", home);
            (void)config_load_silent(path);
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
