#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include "prompt.h"
#include "colors.h"
#include "safe_string.h"

PromptConfig prompt_config;

// Initialize prompt system
void prompt_init(void) {
    prompt_config.use_custom_ps1 = false;
    // Default PS1: <path> git:(branch) #>
    safe_strcpy(prompt_config.ps1, "\\w\\g \\e#>\\e", MAX_PROMPT_LENGTH);
}

// Set custom PS1
void prompt_set_ps1(const char *ps1) {
    if (!ps1) return;

    safe_strcpy(prompt_config.ps1, ps1, MAX_PROMPT_LENGTH);
    prompt_config.use_custom_ps1 = true;
}

char *prompt_git_branch(void) {
    FILE *fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!fp) return NULL;

    static char branch[256];
    if (fgets(branch, sizeof(branch), fp) != NULL) {
        // Ensure null termination
        branch[sizeof(branch) - 1] = '\0';

        // Remove newline safely
        size_t len = safe_strlen(branch, sizeof(branch));
        if (len > 0 && branch[len - 1] == '\n') {
            branch[len - 1] = '\0';
        }

        pclose(fp);

        if (safe_strlen(branch, sizeof(branch)) > 0) {
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
        safe_strcpy(username, user, sizeof(username));
        return username;
    }

    struct passwd pw;
    struct passwd *result = NULL;
    char buf[1024];

    if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
        safe_strcpy(username, pw.pw_name, sizeof(username));
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

// Safe append helper - explicitly bounds-checked for static analyzers
static size_t safe_append(char *output, size_t out_pos, size_t max_pos, const char *str) {
    if (!str || out_pos >= max_pos) {
        return out_pos;
    }

    size_t available = max_pos - out_pos;
    size_t len = strlen(str);
    size_t to_copy = (len < available) ? len : available;

    if (to_copy > 0 && out_pos + to_copy <= max_pos) {
        memcpy(output + out_pos, str, to_copy);
        return out_pos + to_copy;
    }

    return out_pos;
}

// Process escape sequences in PS1
static void process_ps1_escapes(char *output, size_t out_size, const char *ps1, int last_exit_code) {
    if (out_size == 0) return;

    size_t out_pos = 0;
    size_t max_pos = out_size - 1;
    const char *p = ps1;

    while (*p && out_pos < max_pos) {
        if (*p == '\\' && *(p + 1)) {
            p++;

            switch (*p) {
                case 'u':
                    out_pos = safe_append(output, out_pos, max_pos, prompt_get_user());
                    break;

                case 'h':
                    out_pos = safe_append(output, out_pos, max_pos, prompt_get_hostname());
                    break;

                case 'w': {
                    const char *cwd = prompt_get_cwd();
                    if (cwd) {
                        out_pos = safe_append(output, out_pos, max_pos, color_code(COLOR_BOLD COLOR_BLUE));
                        out_pos = safe_append(output, out_pos, max_pos, cwd);
                        out_pos = safe_append(output, out_pos, max_pos, color_code(COLOR_RESET));
                    }
                    break;
                }

                case 'W': {
                    const char *dir = prompt_get_current_dir();
                    if (dir) {
                        out_pos = safe_append(output, out_pos, max_pos, color_code(COLOR_BOLD COLOR_BLUE));
                        out_pos = safe_append(output, out_pos, max_pos, dir);
                        out_pos = safe_append(output, out_pos, max_pos, color_code(COLOR_RESET));
                    }
                    break;
                }

                case 'g': {
                    char *branch = prompt_git_branch();
                    if (branch) {
                        bool dirty = prompt_git_dirty();
                        const char *git_color = dirty ? COLOR_YELLOW : COLOR_GREEN;

                        char temp[512];
                        int n = snprintf(temp, sizeof(temp), " %sgit:%s(%s%s%s)",
                            color_code(git_color), color_code(COLOR_RESET),
                            color_code(COLOR_CYAN), branch, color_code(COLOR_RESET));

                        if (n > 0 && (size_t)n < sizeof(temp)) {
                            out_pos = safe_append(output, out_pos, max_pos, temp);
                        }
                    }
                    break;
                }

                case '$': {
                    const char *symbol = (getuid() == 0) ? "#" : "$";
                    if (out_pos < max_pos) {
                        output[out_pos++] = *symbol;
                    }
                    break;
                }

                case 'e':
                    {
                        const char *bracket_color = (last_exit_code == 0) ?
                            COLOR_BOLD COLOR_BLUE : COLOR_BOLD COLOR_RED;
                        out_pos = safe_append(output, out_pos, max_pos, color_code(bracket_color));
                    }
                    break;

                case 'n':
                    if (out_pos < max_pos) {
                        output[out_pos++] = '\n';
                    }
                    break;

                case '\\':
                    if (out_pos < max_pos) {
                        output[out_pos++] = '\\';
                    }
                    break;

                default:
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
    size_t len = safe_strlen(prompt, sizeof(prompt));
    if (len < MAX_PROMPT_LENGTH - 10) {
        snprintf(prompt + len, MAX_PROMPT_LENGTH - len, "%s ", color_code(COLOR_RESET));
    }

    return prompt;
}
