#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include "prompt.h"
#include "hash.h"
#include "history.h"
#include "colors.h"
#include "color_config.h"
#include "safe_string.h"
#include "cmdsub.h"
#include "varexpand.h"
#include "jobs.h"
#include "utils.h"

PromptConfig prompt_config;

// Initialize prompt system
void prompt_init(void) {
    prompt_config.use_custom_ps1 = false;
    prompt_config.ps1[0] = '\0';
}

// Set up default fancy prompt (call only when stdin is a tty)
// ============================================================================
// Presets: named prompts, picked with `set prompt=NAME`
// ============================================================================
// Each is a PS1 in hash's escape language; colours come from the colour
// configuration through \w, \g and \e, except the powerline segments, whose
// backgrounds are part of the look. The characters used (chevron, box
// drawing, triangle) are in every common monospace font: no special font.

static const PromptPreset presets[] = {
    {"classic", "\\w\\g \\e#>\\e",
     "path, git, and #> coloured by the last status (the default)"},
    {"plain", "\\u@\\h:\\w\\$",
     "user@host:path $, like a classic bash prompt"},
    {"minimal", "\\W \\e\xe2\x9d\xaf\\e",
     "the directory and a chevron coloured by the last status"},
    {"two-line", "\xe2\x95\xad\xe2\x94\x80 \\u@\\h \\w\\g \\e\\?\\e\n\xe2\x95\xb0\xe2\x94\x80\\e\xe2\x9d\xaf\\e",
     "user, host, path and git above, the chevron below"},
    {"powerline", "\033[0;30;44m \\u@\\h \033[0;34;100m\xe2\x96\xb6\033[0;97;100m \\w\033[0;97;100m \033[0;90m\xe2\x96\xb6\033[0m\\g \\e\xe2\x9d\xaf\\e",
     "coloured segments and triangles; no special font needed"},
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static const char *current_preset = NULL;

const PromptPreset *prompt_presets(size_t *count) {
    if (count) {
        *count = PRESET_COUNT;
    }
    return presets;
}

const char *prompt_current_preset(void) {
    return current_preset;
}

bool prompt_set_preset(const char *name) {
    if (!name) {
        return false;
    }
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        if (strcmp(presets[i].name, name) == 0) {
            prompt_set_ps1(presets[i].ps1);
            current_preset = presets[i].name;
            unsetenv("PS1");  // a preset asked for by name beats an inherited PS1
            return true;
        }
    }
    return false;
}

void prompt_set_fancy_default(void) {
    // Only set fancy default if PS1 is not already in environment
    // and no custom PS1 has been configured
    if (getenv("PS1") == NULL && !prompt_config.use_custom_ps1) {
        prompt_config.use_custom_ps1 = true;
        safe_strcpy(prompt_config.ps1, presets[0].ps1, MAX_PROMPT_LENGTH);  // the classic preset
        current_preset = presets[0].name;
    }
}

// Continuation prompt shown while a compound command is still open.
// POSIX PS2; defaults to "> " when PS2 is not set.
const char *prompt_continuation(void) {
    const char *ps2 = getenv("PS2");
    if (ps2) {
        return ps2;
    }
    return "> ";
}

// Set custom PS1
void prompt_set_ps1(const char *ps1) {
    if (!ps1) return;

    safe_strcpy(prompt_config.ps1, ps1, MAX_PROMPT_LENGTH);
    prompt_config.use_custom_ps1 = true;
    current_preset = NULL;  // prompt_set_preset() names it again when it was a preset
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

static size_t handle_directories(char c, char *output, size_t out_pos, size_t max_pos, const char *base_color) {
    const char *dir;
    if (c == 'w') {
        dir = prompt_get_cwd();
    } else if (c == 'W') {
        dir = prompt_get_current_dir();
    } else {
        return out_pos;
    }

    out_pos = safe_append(output, out_pos, max_pos, color_config_get(color_config.prompt_path));
    out_pos = safe_append(output, out_pos, max_pos, dir);
    // Reset and re-apply base color to maintain bold
    out_pos = safe_append(output, out_pos, max_pos, color_code(COLOR_RESET));
    out_pos = safe_append(output, out_pos, max_pos, base_color);

    return out_pos;
}

static size_t handle_git_branches(char *output, size_t out_pos, size_t max_pos, const char *base_color) {
    char *branch = prompt_git_branch();
    if (branch) {
        bool dirty = prompt_git_dirty();
        const char *git_status_color = dirty ?
            color_config_get(color_config.prompt_git_dirty) :
            color_config_get(color_config.prompt_git_clean);

        // Build git string with proper color handling
        // After each reset, re-apply base color for bold
        char temp[512];
        int n = snprintf(temp, sizeof(temp), " %s%sgit:%s%s(%s%s%s%s%s)",
            base_color, git_status_color,  // bold + status color for "git:"
            color_code(COLOR_RESET), base_color,  // reset then re-apply bold for "("
            color_config_get(color_config.prompt_git_branch), branch,  // branch color + name
            color_code(COLOR_RESET), base_color,  // reset then re-apply bold for ")"
            "");

        if (n > 0 && (size_t)n < sizeof(temp)) {
            out_pos = safe_append(output, out_pos, max_pos, temp);
        }
    }

    return out_pos;
}

// Process escape sequences in PS1
static void process_ps1_escapes(char *output, size_t out_size, const char *ps1, int last_exit_code) {
    if (out_size == 0) return;

    size_t out_pos = 0;
    size_t max_pos = out_size - 1;
    const char *p = ps1;

    // Get base prompt color (bold by default) - used to restore after resets
    const char *base_color = color_config_get(color_config.prompt);

    while (*p && out_pos < max_pos) {
        if (*p != '\\' || !(*(p + 1))) {
            if (out_pos < max_pos) {
                output[out_pos++] = *p++;
            } else {
                break;
            }
            continue;
        }

        // We got '\something'
        // Skip '\'
        p++;

        switch (*p) {
            case 'u':
                out_pos = safe_append(output, out_pos, max_pos, prompt_get_user());
                break;

            case 'h': {
                    // The host name up to the first dot, as in bash; \H is the whole name
                    const char *host = prompt_get_hostname();
                    const char *dot = host ? strchr(host, '.') : NULL;
                    if (dot) {
                        char shortname[256];
                        size_t n = (size_t)(dot - host);
                        if (n >= sizeof(shortname)) n = sizeof(shortname) - 1;
                        memcpy(shortname, host, n);
                        shortname[n] = '\0';
                        out_pos = safe_append(output, out_pos, max_pos, shortname);
                    } else {
                        out_pos = safe_append(output, out_pos, max_pos, host);
                    }
                }
                break;
            case 'H':
                out_pos = safe_append(output, out_pos, max_pos, prompt_get_hostname());
                break;

            case 'w':
            case 'W':
                out_pos = handle_directories(*p, output, out_pos, max_pos, base_color);
                break;

            case 'g':
                out_pos = handle_git_branches(output, out_pos, max_pos, base_color);
                break;

            case '$':
                if (out_pos < max_pos) {
                    const char symbol = (getuid() == 0) ? '#' : '$';
                    output[out_pos++] = symbol;
                }
                break;

            case 'e': {
                    // \e outputs the exit-code-based color (success or error)
                    const char *bracket_color = (last_exit_code == 0) ?
                        color_config_get(color_config.prompt) :
                        color_config_get(color_config.prompt_error);
                    out_pos = safe_append(output, out_pos, max_pos, bracket_color);
                }
                break;

            case 'n':
                if (out_pos < max_pos) {
                    output[out_pos++] = '\n';
                }
                break;

            case 't': case 'T': case '@': case 'd': {
                    // Time and date, as in bash: \t 24h, \T 12h, \@ 12h am/pm, \d weekday month day
                    time_t now = time(NULL);
                    struct tm tm_now;
                    localtime_r(&now, &tm_now);
                    const char *format = (*p == 't') ? "%H:%M:%S" : (*p == 'T') ? "%I:%M:%S"
                                       : (*p == '@') ? "%I:%M %p" : "%a %b %d";
                    char stamp[64];
                    if (strftime(stamp, sizeof(stamp), format, &tm_now) > 0) {
                        out_pos = safe_append(output, out_pos, max_pos, stamp);
                    }
                }
                break;

            case 'j': {
                    char count[16];
                    snprintf(count, sizeof(count), "%d", jobs_count());
                    out_pos = safe_append(output, out_pos, max_pos, count);
                }
                break;

            case '!': {
                    char number[16];
                    snprintf(number, sizeof(number), "%d", history_count() + 1);
                    out_pos = safe_append(output, out_pos, max_pos, number);
                }
                break;

            case 's':
                out_pos = safe_append(output, out_pos, max_pos, HASH_NAME);
                break;

            case 'v':
                out_pos = safe_append(output, out_pos, max_pos, HASH_VERSION);
                break;

            case '?':
                // The last exit status, only when it was not 0
                if (last_exit_code != 0) {
                    char status[16];
                    snprintf(status, sizeof(status), "%d", last_exit_code);
                    out_pos = safe_append(output, out_pos, max_pos, status);
                }
                break;

            case 'b': {
                    // The git branch alone, nothing outside a repository
                    char *branch = prompt_git_branch();
                    if (branch) {
                        out_pos = safe_append(output, out_pos, max_pos, branch);
                    }
                }
                break;

            case '[': case ']':
                break;  // bash's non-printing markers: the editor measures escapes itself

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
    }

    output[out_pos] = '\0';
}

// Check if PS1 contains command substitution or variable expansion
static bool ps1_needs_expansion(const char *ps1) {
    if (!ps1) return false;
    // Check for $(...) or ${...} or $VAR patterns
    return (strstr(ps1, "$(") != NULL ||
            strstr(ps1, "${") != NULL ||
            strchr(ps1, '`') != NULL);
}

static void handle_dynamic_expansion(char *prompt, const char *ps1, int last_exit_code) {
    // Set environment variables for prompt programs like Starship
    char status_str[16];
    snprintf(status_str, sizeof(status_str), "%d", last_exit_code);
    setenv("STATUS", status_str, 1);

    char jobs_str[16];
    snprintf(jobs_str, sizeof(jobs_str), "%d", jobs_count());
    setenv("NUM_JOBS", jobs_str, 1);

    // Make a mutable copy for expansion
    char ps1_copy[MAX_PROMPT_LENGTH];
    safe_strcpy(ps1_copy, ps1, sizeof(ps1_copy));

    // Expand variables first (e.g., $STATUS -> "0")
    char *var_expanded = varexpand_expand(ps1_copy, 0);
    const char *to_expand = var_expanded ? var_expanded : ps1_copy;

    // Expand command substitutions (e.g., $(starship prompt) -> actual prompt)
    char *cmd_expanded = cmdsub_expand(to_expand);

    if (cmd_expanded) {
        // Strip leading newlines - hash already ensures prompts start on a new line
        const char *src = cmd_expanded;
        while (*src == '\n') src++;
        safe_strcpy(prompt, src, sizeof(prompt));

        // Strip trailing whitespace from the last line (but keep the newline structure)
        size_t plen = strlen(prompt);
        while (plen > 0 && char_in_string(prompt[plen-1], " \t")) {
            prompt[--plen] = '\0';
        }

        free(cmd_expanded);
    } else if (var_expanded) {
        safe_strcpy(prompt, var_expanded, sizeof(prompt));
    } else {
        safe_strcpy(prompt, ps1, sizeof(prompt));
    }

    free(var_expanded);  // Safe to free NULL

    // Don't add extra reset/space for external prompts - they handle their own formatting
}

// Escapes processed and, for the shell's own prompt, wrapped in the base colour
// with a trailing reset and space; a PS1 from the environment is left as is
static void render_prompt(char *out, size_t out_size, const char *ps1, int last_exit_code, bool wrap) {
    // Reserve space for base_color (~32 bytes) + COLOR_RESET (~10 bytes) + space + null
    char temp_prompt[MAX_PROMPT_LENGTH - 64];
    process_ps1_escapes(temp_prompt, sizeof(temp_prompt), ps1, last_exit_code);
    if (!wrap) {
        safe_strcpy(out, temp_prompt, out_size);
        return;
    }
    const char *base_color = color_config_get(color_config.prompt);
    snprintf(out, out_size, "%s%s%s ", base_color, temp_prompt, color_code(COLOR_RESET));
}

// Generate prompt
char *prompt_generate(int last_exit_code) {
    static char prompt[MAX_PROMPT_LENGTH];

    // Get PS1 from environment or config
    const char *ps1;
    const char *ps1_env = getenv("PS1");
    bool ps1_from_env = false;

    if (ps1_env) {
        // PS1 explicitly set in environment - always use it
        ps1 = ps1_env;
        ps1_from_env = true;
    } else if (prompt_config.use_custom_ps1 && isatty(STDIN_FILENO)) {
        // Use custom PS1 only when stdin is a tty
        ps1 = prompt_config.ps1;
    } else {
        // POSIX default: "$ " (or "# " for root)
        // Used when stdin is not a tty or no custom PS1 is configured
        ps1 = (getuid() == 0) ? "# " : "$ ";
        ps1_from_env = true;  // Treat as simple prompt (no escape processing)
    }

    // Check if PS1 needs dynamic expansion (for Starship, etc.)
    if (ps1_needs_expansion(ps1)) {
        handle_dynamic_expansion(prompt, ps1, last_exit_code);
        return prompt;
    }

    render_prompt(prompt, sizeof(prompt), ps1, last_exit_code, !ps1_from_env);
    return prompt;
}

const char *prompt_render(const char *ps1, int last_exit_code) {
    static char rendered[MAX_PROMPT_LENGTH];
    render_prompt(rendered, sizeof(rendered), ps1 ? ps1 : "", last_exit_code, true);
    return rendered;
}

void prompt_print_presets(FILE *out) {
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        const char *mark = (current_preset && strcmp(current_preset, presets[i].name) == 0) ? "*" : " ";
        fprintf(out, "%s %-10s %s\n", mark, presets[i].name, presets[i].blurb);
        // The sample, its own lines indented under the name
        fputs("             ", out);
        for (const char *c = prompt_render(presets[i].ps1, 0); *c; c++) {
            fputc(*c, out);
            if (*c == '\n') {
                fputs("             ", out);
            }
        }
        fputc('\n', out);
    }
    fprintf(out, "\n`set prompt=NAME` picks one; `set PS1='...'` builds your own.\n");
}
