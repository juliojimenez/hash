#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include "history.h"
#include "safe_string.h"
#include "colors.h"
#include "hash.h"

// History storage
static char **history = NULL;
static int history_size = 0;       // Max entries in memory (HISTSIZE)
static int history_filesize = 0;   // Max entries in file (HISTFILESIZE)
static int history_count_val = 0;  // Current count
static int history_start = 0;      // For circular buffer
static int history_position = -1;  // Current position for navigation

// Get history size from environment
static int get_histsize(void) {
    const char *histsize = getenv("HISTSIZE");
    if (histsize) {
        int size = atoi(histsize);
        if (size == -1) return -1;  // Unlimited
        if (size > 0) return size;
    }
    return HISTORY_DEFAULT_SIZE;
}

// Get history file size from environment
static int get_histfilesize(void) {
    const char *histfilesize = getenv("HISTFILESIZE");
    if (histfilesize) {
        int size = atoi(histfilesize);
        if (size == -1) return -1;  // Unlimited
        if (size > 0) return size;
    }
    return HISTORY_DEFAULT_FILESIZE;
}

// Get history file path
static void get_history_path(char *path, size_t size) {
    const char *histfile = getenv("HISTFILE");
    if (histfile) {
        safe_strcpy(path, histfile, size);
        return;
    }

    const char *home = getenv("HOME");
    if (!home) {
        struct passwd pw;
        struct passwd *result = NULL;
        char buf[1024];

        if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
            home = pw.pw_dir;
        }
    }

    if (home) {
        snprintf(path, size, "%s/.hash_history", home);
    } else {
        path[0] = '\0';
    }
}

// Check HISTCONTROL setting
static int should_ignore_space(void) {
    const char *histcontrol = getenv("HISTCONTROL");
    if (!histcontrol) return 1;  // Default: ignore space

    return (strstr(histcontrol, "ignorespace") != NULL ||
            strstr(histcontrol, "ignoreboth") != NULL);
}

static int should_ignore_dups(void) {
    const char *histcontrol = getenv("HISTCONTROL");
    if (!histcontrol) return 1;  // Default: ignore dups

    return (strstr(histcontrol, "ignoredups") != NULL ||
            strstr(histcontrol, "ignoreboth") != NULL);
}

static int should_erase_dups(void) {
    const char *histcontrol = getenv("HISTCONTROL");
    if (!histcontrol) return 0;

    return strstr(histcontrol, "erasedups") != NULL;
}

// Initialize history
void history_init(void) {
    // If already initialized, clear first
    if (history) {
        history_clear();
        free(history);
        history = NULL;
    }

    history_size = get_histsize();
    history_filesize = get_histfilesize();

    // Allocate initial history array
    if (history_size == -1) {
        // Unlimited - start with reasonable size
        history_size = 10000;
    }

    history = calloc(history_size, sizeof(char *));
    if (!history) {
        history_size = 0;
        return;
    }

    history_count_val = 0;
    history_start = 0;
    history_position = -1;

    // Load from file
    history_load();
}

// Get actual index in circular buffer
static int history_index(int logical_index) {
    if (logical_index < 0 || logical_index >= history_count_val) {
        return -1;
    }

    if (history_size <= 0) return -1;
    return (history_start + logical_index) % history_size;
}

// Erase all duplicate instances of a command
static void erase_duplicates(const char *line) {
    for (int i = history_count_val - 1; i >= 0; i--) {
        int idx = history_index(i);
        if (idx >= 0 && history[idx] && strcmp(history[idx], line) == 0) {
            // Found duplicate, remove it
            free(history[idx]);

            // Shift everything after it down
            for (int j = i; j < history_count_val - 1; j++) {
                int curr_idx = history_index(j);
                int next_idx = history_index(j + 1);
                if (curr_idx >= 0 && next_idx >= 0) {
                    history[curr_idx] = history[next_idx];
                }
            }

            history_count_val--;
        }
    }
}

// Add command to history
void history_add(const char *line) {
    if (!line || *line == '\0' || !history) return;

    // Skip if line is only whitespace
    const char *p = line;
    while (*p && isspace(*p)) p++;
    if (*p == '\0') return;

    // Check HISTCONTROL for ignorespace
    if (should_ignore_space() && isspace(line[0])) {
        return;
    }

    // Check HISTCONTROL for ignoredups
    if (should_ignore_dups() && history_count_val > 0) {
        int last_idx = history_index(history_count_val - 1);
        if (last_idx >= 0 && history[last_idx] && strcmp(history[last_idx], line) == 0) {
            return;
        }
    }

    // Check HISTCONTROL for erasedups
    if (should_erase_dups()) {
        erase_duplicates(line);
    }

    // Check if we need to grow the array (unlimited mode)
    int histsize_limit = get_histsize();
    if (histsize_limit == -1 && history_count_val >= history_size) {
        // Unlimited - grow the array
        int new_size = history_size * 2;
        char **new_history = realloc(history, new_size * sizeof(char *));
        if (new_history) {
            // Initialize new entries
            for (int i = history_size; i < new_size; i++) {
                new_history[i] = NULL;
            }
            history = new_history;
            history_size = new_size;
        } else {
            // Allocation failed, remove oldest
            if (history_count_val > 0) {
                free(history[history_start]);
                history[history_start] = NULL;
                history_start = (history_start + 1) % history_size;
                history_count_val--;
            }
        }
    } else if (histsize_limit != -1 && history_count_val >= histsize_limit) {
        // Limited size - remove oldest entry
        free(history[history_start]);
        history[history_start] = NULL;
        history_start = (history_start + 1) % history_size;
        history_count_val--;
    }

    // Add new entry
    int idx = (history_start + history_count_val) % history_size;
    history[idx] = strdup(line);
    if (history[idx]) {
        history_count_val++;
    }

    // Reset position to end
    history_position = -1;

    // Save immediately to file
    history_save();
}

// Get history entry
const char *history_get(int index) {
    int actual_idx = history_index(index);
    if (actual_idx < 0) return NULL;
    return history[actual_idx];
}

// Get count
int history_count(void) {
    return history_count_val;
}

// Get current position
int history_get_position(void) {
    return history_position;
}

// Set position
void history_set_position(int pos) {
    history_position = pos;
}

// Move to previous (older) command
const char *history_prev(void) {
    if (history_count_val == 0) return NULL;

    if (history_position == -1) {
        history_position = history_count_val - 1;
    } else if (history_position > 0) {
        history_position--;
    } else {
        return history_get(history_position);
    }

    return history_get(history_position);
}

// Move to next (newer) command
const char *history_next(void) {
    if (history_position == -1) {
        return NULL;
    }

    if (history_position < history_count_val - 1) {
        history_position++;
        return history_get(history_position);
    } else {
        history_position = -1;
        return NULL;
    }
}

// Reset to end
void history_reset_position(void) {
    history_position = -1;
}

// Search for command with prefix
const char *history_search_prefix(const char *prefix) {
    if (!prefix || *prefix == '\0') return NULL;

    size_t prefix_len = strlen(prefix);

    for (int i = history_count_val - 1; i >= 0; i--) {
        const char *cmd = history_get(i);
        if (cmd && strncmp(cmd, prefix, prefix_len) == 0) {
            return cmd;
        }
    }

    return NULL;
}

// Search for command containing substring
const char *history_search_substring(const char *substring, int start_index,
                                      int direction, int *result_index) {
    if (result_index) *result_index = -1;
    if (!substring || *substring == '\0') return NULL;
    if (history_count_val == 0) return NULL;

    // Determine search bounds
    int start, end, step;
    if (direction == 1) {
        // Reverse search (older)
        start = (start_index < 0) ? history_count_val - 1 : start_index;
        if (start >= history_count_val) start = history_count_val - 1;
        end = -1;
        step = -1;
    } else {
        // Forward search (newer)
        start = (start_index < 0) ? 0 : start_index;
        if (start >= history_count_val) return NULL;
        end = history_count_val;
        step = 1;
    }

    for (int i = start; i != end; i += step) {
        const char *cmd = history_get(i);
        if (cmd && strstr(cmd, substring) != NULL) {
            if (result_index) *result_index = i;
            return cmd;
        }
    }

    return NULL;
}

// Expand history references
char *history_expand(const char *line) {
    if (!line || strchr(line, '!') == NULL) {
        return NULL;
    }

    char *result = malloc(HISTORY_MAX_LINE);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = line;

    while (*p && out_pos < HISTORY_MAX_LINE - 1) {
        if (*p == '\\' && *(p + 1) == '!') {
            result[out_pos++] = '!';
            p += 2;
        } else if (*p == '!' && *(p + 1) == '!') {
            p += 2;

            const char *last = history_get(history_count_val - 1);
            if (last) {
                size_t len = strlen(last);
                size_t space = HISTORY_MAX_LINE - 1 - out_pos;
                size_t to_copy = (len < space) ? len : space;
                memcpy(result + out_pos, last, to_copy);
                out_pos += to_copy;
            }
        } else if (*p == '!' && *(p + 1) == '-' && isdigit(*(p + 2))) {
            p += 2;

            int n = 0;
            while (isdigit(*p)) {
                n = n * 10 + (*p - '0');
                p++;
            }

            int target_idx = history_count_val - n;
            if (target_idx >= 0 && target_idx < history_count_val) {
                const char *cmd = history_get(target_idx);
                if (cmd) {
                    size_t len = strlen(cmd);
                    size_t space = HISTORY_MAX_LINE - 1 - out_pos;
                    size_t to_copy = (len < space) ? len : space;
                    memcpy(result + out_pos, cmd, to_copy);
                    out_pos += to_copy;
                }
            }
        } else if (*p == '!' && isdigit(*(p + 1))) {
            p++;

            int n = 0;
            while (isdigit(*p)) {
                n = n * 10 + (*p - '0');
                p++;
            }

            if (n >= 0 && n < history_count_val) {
                const char *cmd = history_get(n);
                if (cmd) {
                    size_t len = strlen(cmd);
                    size_t space = HISTORY_MAX_LINE - 1 - out_pos;
                    size_t to_copy = (len < space) ? len : space;
                    memcpy(result + out_pos, cmd, to_copy);
                    out_pos += to_copy;
                }
            }
        } else if (*p == '!' && isalpha(*(p + 1))) {
            p++;

            char prefix[256];
            size_t prefix_len = 0;
            while (*p && (isalnum(*p) || *p == '_' || *p == '-') && prefix_len < sizeof(prefix) - 1) {
                prefix[prefix_len++] = *p++;
            }
            prefix[prefix_len] = '\0';

            const char *cmd = history_search_prefix(prefix);
            if (cmd) {
                size_t len = strlen(cmd);
                size_t space = HISTORY_MAX_LINE - 1 - out_pos;
                size_t to_copy = (len < space) ? len : space;
                memcpy(result + out_pos, cmd, to_copy);
                out_pos += to_copy;
            }
        } else {
            result[out_pos++] = *p++;
        }
    }

    result[out_pos] = '\0';

    if (strcmp(result, line) == 0) {
        free(result);
        return NULL;
    }

    return result;
}

// Save history to file
void history_save(void) {
    char history_path[1024];
    get_history_path(history_path, sizeof(history_path));

    if (history_path[0] == '\0') return;

    FILE *fp = fopen(history_path, "w");
    if (!fp) return;

    // Determine how many entries to write
    int filesize = get_histfilesize();
    int entries_to_write = history_count_val;

    if (filesize != -1 && entries_to_write > filesize) {
        entries_to_write = filesize;
    }

    // Write most recent entries
    int start_idx = history_count_val - entries_to_write;
    for (int i = start_idx; i < history_count_val; i++) {
        const char *cmd = history_get(i);
        if (cmd) {
            fprintf(fp, "%s\n", cmd);
        }
    }
    fflush(fp);
    fclose(fp);
}

// Load history from file
void history_load(void) {
    char history_path[1024];
    get_history_path(history_path, sizeof(history_path));

    if (history_path[0] == '\0') return;

    FILE *fp = fopen(history_path, "r");
    if (!fp) return;

    // Save old HISTCONTROL
    const char *old_histcontrol = getenv("HISTCONTROL");
    char saved_histcontrol[256] = {0};
    if (old_histcontrol) {
        safe_strcpy(saved_histcontrol, old_histcontrol, sizeof(saved_histcontrol));
    }

    // Temporarily disable HISTCONTROL during load
    unsetenv("HISTCONTROL");

    char line[HISTORY_MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        line[HISTORY_MAX_LINE - 1] = '\0';

        size_t len = safe_strlen(line, sizeof(line));
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (line[0] != '\0') {
            // Check if we need to grow or wrap
            if (history_count_val >= history_size) {
                int histsize_limit = get_histsize();
                if (histsize_limit == -1) {
                    // Grow
                    int new_size = history_size * 2;
                    char **new_history = realloc(history, new_size * sizeof(char *));
                    if (new_history) {
                        for (int i = history_size; i < new_size; i++) {
                            new_history[i] = NULL;
                        }
                        history = new_history;
                        history_size = new_size;
                    } else {
                        // Can't grow, skip this entry
                        continue;
                    }
                } else {
                    // Wrap around
                    free(history[history_start]);
                    history[history_start] = NULL;
                    history_start = (history_start + 1) % history_size;
                    history_count_val--;
                }
            }

            int idx = (history_start + history_count_val) % history_size;
            history[idx] = strdup(line);
            if (history[idx]) {
                history_count_val++;
            }
        }
    }

    fclose(fp);

    // Restore HISTCONTROL
    if (saved_histcontrol[0] != '\0') {
        setenv("HISTCONTROL", saved_histcontrol, 1);
    }

    history_reset_position();
}

// Clear history
void history_clear(void) {
    if (!history) return;

    for (int i = 0; i < history_size; i++) {
        if (history[i]) {
            free(history[i]);
            history[i] = NULL;
        }
    }
    history_count_val = 0;
    history_start = 0;
    history_position = -1;
}
