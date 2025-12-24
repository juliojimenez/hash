#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include "prompt.h"
#include "colors.h"

PromptConfig prompt_config;

// Initialize prompt system
void prompt_init(void) {
    prompt_config.use_custom_ps1 = false;
    // Default PS1: <path> git:(branch) #>
    strncpy(prompt_config.ps1, "\\w\\g #> ", MAX_PROMPT_LENGTH - 1);
}

// Set custom PS1
void prompt_set_ps1(const char *ps1) {
    if (!ps1) return;

    strncpy(prompt_config.ps1, ps1, MAX_PROMPT_LENGTH - 1);
    prompt_config.ps1[MAX_PROMPT_LENGTH - 1] = '\0';
    prompt_config.use_custom_ps1 = true;
}

// Get current git branch
char *prompt_git_branch(void) {
    FILE *fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!fp) return NULL;

    static char branch[256];
    if (fgets(branch, sizeof(branch), fp) != NULL) {
        // Remove newline
        branch[strcspn(branch, "\n")] = '\0';
        pclose(fp);

        if (strlen(branch) > 0) {
            return branch;
        }
    }

    pclose(fp);
    return NULL;
}

// Check if git repo has uncommitted changes
bool prompt_git_dirty(void) {
    FILE *fp = popen("git status --porcelain 2>/dev/null", "r");
    if (!fp) return false;

    char line[256];
    bool dirty = false;

    if (fgets(line, sizeof(line), fp) != NULL) {
        dirty = true;  // If there's any output, repo is dirty
    }

    pclose(fp);
    return dirty;
}

// Get current working directory
char *prompt_get_cwd(void) {
    static char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // Replace home directory with ~
        const char *home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            static char short_cwd[PATH_MAX];
            snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
            return short_cwd;
        }
        return cwd;
    }

    return NULL;
}

// Get current directory name only
char *prompt_get_current_dir(void) {
    char *cwd = prompt_get_cwd();
    if (!cwd) return NULL;

    // Find last slash
    char *last_slash = strrchr(cwd, '/');
    if (last_slash && *(last_slash + 1) != '\0') {
        return last_slash + 1;
    }

    // If it's just "/" or "~"
    return cwd;
}

// Get username
char *prompt_get_user(void) {
    static char username[256];

    const char *user = getenv("USER");
    if (user) {
        strncpy(username, user, sizeof(username) - 1);
        return username;
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username) - 1);
        return username;
    }

    return NULL;
}

// Get hostname
char *prompt_get_hostname(void) {
    static char hostname[256];

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return hostname;
    }

    return NULL;
}

// Process escape sequences in PS1
static void process_ps1_escapes(char *output, size_t out_size, const char *ps1, int last_exit_code) {
    size_t out_pos = 0;
    const char *p = ps1;

    // Reserve space for null terminator
    if (out_size == 0) return;
    size_t max_pos = out_size - 1;

    while (*p && out_pos < max_pos) {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip backslash

            switch (*p) {
                case 'u':  // Username
                    {
                        char *user = prompt_get_user();
                        if (user) {
                            size_t len = strlen(user);
                            size_t available = max_pos - out_pos;
                            size_t to_copy = (len < available) ? len : available;
                            memcpy(output + out_pos, user, to_copy);
                            out_pos += to_copy;
                        }
                    }
                    break;

                case 'h':  // Hostname
                    {
                        char *host = prompt_get_hostname();
                        if (host) {
                            size_t len = strlen(host);
                            size_t available = max_pos - out_pos;
                            size_t to_copy = (len < available) ? len : available;
                            memcpy(output + out_pos, host, to_copy);
                            out_pos += to_copy;
                        }
                    }
                    break;

                case 'w':  // Full current working directory
                    {
                        char *cwd = prompt_get_cwd();
                        if (cwd) {
                            const char *color = color_code(COLOR_BOLD COLOR_BLUE);
                            const char *reset = color_code(COLOR_RESET);
                            size_t available = max_pos - out_pos;

                            size_t color_len = strlen(color);
                            if (color_len < available) {
                                memcpy(output + out_pos, color, color_len);
                                out_pos += color_len;
                                available -= color_len;
                            }

                            size_t cwd_len = strlen(cwd);
                            size_t to_copy = (cwd_len < available) ? cwd_len : available;
                            if (to_copy > 0) {
                                memcpy(output + out_pos, cwd, to_copy);
                                out_pos += to_copy;
                                available -= to_copy;
                            }

                            size_t reset_len = strlen(reset);
                            to_copy = (reset_len < available) ? reset_len : available;
                            if (to_copy > 0) {
                                memcpy(output + out_pos, reset, to_copy);
                                out_pos += to_copy;
                            }
                        }
                    }
                    break;

                case 'W':  // Current directory name only
                    {
                        char *dir = prompt_get_current_dir();
                        if (dir) {
                            const char *color = color_code(COLOR_BOLD COLOR_BLUE);
                            const char *reset = color_code(COLOR_RESET);
                            size_t available = max_pos - out_pos;

                            size_t color_len = strlen(color);
                            if (color_len < available) {
                                memcpy(output + out_pos, color, color_len);
                                out_pos += color_len;
                                available -= color_len;
                            }

                            size_t dir_len = strlen(dir);
                            size_t to_copy = (dir_len < available) ? dir_len : available;
                            if (to_copy > 0) {
                                memcpy(output + out_pos, dir, to_copy);
                                out_pos += to_copy;
                                available -= to_copy;
                            }

                            size_t reset_len = strlen(reset);
                            to_copy = (reset_len < available) ? reset_len : available;
                            if (to_copy > 0) {
                                memcpy(output + out_pos, reset, to_copy);
                                out_pos += to_copy;
                            }
                        }
                    }
                    break;

                case 'g':  // Git branch (custom)
                    {
                        char *branch = prompt_git_branch();
                        if (branch) {
                            bool dirty = prompt_git_dirty();
                            const char *git_color = dirty ? COLOR_YELLOW : COLOR_GREEN;

                            char temp[512];
                            int written = snprintf(temp, sizeof(temp), " %sgit:%s(%s%s%s)",
                                color_code(git_color),
                                color_code(COLOR_RESET),
                                color_code(COLOR_CYAN),
                                branch,
                                color_code(COLOR_RESET));

                            if (written > 0 && (size_t)written < sizeof(temp)) {
                                size_t available = max_pos - out_pos;
                                size_t to_copy = ((size_t)written < available) ? (size_t)written : available;
                                memcpy(output + out_pos, temp, to_copy);
                                out_pos += to_copy;
                            }
                        }
                    }
                    break;

                case '$':  // $ for regular user, # for root
                    {
                        const char *symbol = (getuid() == 0) ? "#" : "$";
                        if (out_pos < max_pos) {
                            output[out_pos++] = *symbol;
                        }
                    }
                    break;

                case 'e':  // Exit code indicator
                    {
                        const char *bracket_color = (last_exit_code == 0) ?
                            COLOR_BOLD COLOR_BLUE : COLOR_BOLD COLOR_RED;
                        const char *color = color_code(bracket_color);
                        size_t len = strlen(color);
                        size_t available = max_pos - out_pos;
                        size_t to_copy = (len < available) ? len : available;
                        if (to_copy > 0) {
                            memcpy(output + out_pos, color, to_copy);
                            out_pos += to_copy;
                        }
                    }
                    break;

                case 'n':  // Newline
                    if (out_pos < max_pos) {
                        output[out_pos++] = '\\';
                    }
                    break;

                case '\\':  // Literal backslash
                    if (out_pos < max_pos) {
                        output[out_pos++] = '\\';
                    }
                    break;

                default:
                    // Unknown escape, keep the backslash and character
                    if (out_pos < max_pos) {
                        output[out_pos++] = '\\';
                    }
                    if (out_pos < max_pos) {
                        output[out_pos++] = *p;
                    }
                    break;
            }
            p++;
        } else {
            if (out_pos < max_pos) {
                output[out_pos++] = *p++;
            } else {
                break;
            }
        }
    }

    output[out_pos] = '\0';
}

// Generate prompt
char *prompt_generate(int last_exit_code) {
    static char prompt[MAX_PROMPT_LENGTH];

    // Get PS1 from environment or config
    const char *ps1_env = getenv("PS1");
    const char *ps1 = ps1_env ? ps1_env :
                      (prompt_config.use_custom_ps1 ? prompt_config.ps1 : "\\w\\g \\e#>\\e ");

    // Process escape sequences
    process_ps1_escapes(prompt, sizeof(prompt), ps1, last_exit_code);

    // Add final reset and space
    size_t len = strlen(prompt);
    if (len < MAX_PROMPT_LENGTH - 10) {
        snprintf(prompt + len, MAX_PROMPT_LENGTH - len, "%s ", color_code(COLOR_RESET));
    }

    return prompt;
}
