#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "completion.h"
#include "safe_string.h"
#include "config.h"
#include "builtins.h"

// Built-in commands for completion
static const char *builtin_commands[] = {
    "cd", "exit", "alias", "unalias", "source", "export", "history",
    "jobs", "fg", "bg", "logout", "test", "unset", "true", "false",
    "echo", "read", "return", "break", "continue", "eval", "update", NULL
};

// Initialize completion
void completion_init(void) {
    // Nothing to initialize yet
}

// Create new completion result
static CompletionResult *create_result(void) {
    CompletionResult *result = malloc(sizeof(CompletionResult));
    if (!result) return NULL;

    result->matches = calloc(MAX_COMPLETIONS, sizeof(char *));
    if (!result->matches) {
        free(result);
        return NULL;
    }

    result->count = 0;
    result->common_prefix = NULL;
    return result;
}

// Add a match to completion result
static int add_match(CompletionResult *result, const char *match) {
    if (!result || !match || result->count >= MAX_COMPLETIONS) {
        return -1;
    }

    result->matches[result->count] = strdup(match);
    if (!result->matches[result->count]) {
        return -1;
    }

    result->count++;
    return 0;
}

// Find common prefix of all matches
char *completion_common_prefix(char **matches, int count) {
    if (!matches || count == 0) return NULL;
    if (count == 1) return strdup(matches[0]);

    // Find length of common prefix
    size_t prefix_len = 0;
    int done = 0;

    while (!done) {
        char c = matches[0][prefix_len];
        if (c == '\0') break;

        for (int i = 1; i < count; i++) {
            if (matches[i][prefix_len] != c) {
                done = 1;
                break;
            }
        }

        if (!done) prefix_len++;
    }

    if (prefix_len == 0) return NULL;

    char *prefix = malloc(prefix_len + 1);
    if (!prefix) return NULL;

    memcpy(prefix, matches[0], prefix_len);
    prefix[prefix_len] = '\0';

    return prefix;
}

// Complete commands from PATH
static void complete_commands(CompletionResult *result, const char *prefix) {
    size_t prefix_len = strlen(prefix);

    // Add built-in commands
    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (strncmp(builtin_commands[i], prefix, prefix_len) == 0) {
            add_match(result, builtin_commands[i]);
        }
    }

    // Add aliases
    for (int i = 0; i < shell_config.alias_count; i++) {
        if (strncmp(shell_config.aliases[i].name, prefix, prefix_len) == 0) {
            add_match(result, shell_config.aliases[i].name);
        }
    }

    // Search PATH for executables
    const char *path_env = getenv("PATH");
    if (!path_env) return;

    char *path_copy = strdup(path_env);
    if (!path_copy) return;

    char *saveptr;
    char *dir = strtok_r(path_copy, ":", &saveptr);
    while (dir && result->count < MAX_COMPLETIONS) {
        DIR *dp = opendir(dir);
        if (!dp) {
            dir = strtok_r(NULL, ":", &saveptr);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dp)) != NULL && result->count < MAX_COMPLETIONS) {
            // Check if name matches prefix
            if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
                // Check if it's executable
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

                if (access(full_path, X_OK) == 0) {
                    // Check for duplicates
                    int is_dup = 0;
                    for (int i = 0; i < result->count; i++) {
                        if (strcmp(result->matches[i], entry->d_name) == 0) {
                            is_dup = 1;
                            break;
                        }
                    }

                    if (!is_dup) {
                        add_match(result, entry->d_name);
                    }
                }
            }
        }

        closedir(dp);
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
}

// Complete files and directories
static void complete_files(CompletionResult *result, const char *prefix) {
    // Split prefix into directory and filename
    char dir_path[1024];
    const char *filename_prefix;

    const char *last_slash = strrchr(prefix, '/');
    if (last_slash) {
        // Has directory component
        size_t dir_len = last_slash - prefix;
        if (dir_len == 0) {
            // Root directory
            safe_strcpy(dir_path, "/", sizeof(dir_path));
        } else {
            memcpy(dir_path, prefix, dir_len);
            dir_path[dir_len] = '\0';
        }
        filename_prefix = last_slash + 1;
    } else {
        // Current directory
        safe_strcpy(dir_path, ".", sizeof(dir_path));
        filename_prefix = prefix;
    }

    size_t prefix_len = strlen(filename_prefix);

    // Open directory
    DIR *dp = opendir(dir_path);
    if (!dp) return;

    const struct dirent *entry;
    while ((entry = readdir(dp)) != NULL && result->count < MAX_COMPLETIONS) {
        // Skip . and .. unless explicitly requested
        if (prefix_len == 0 && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }

        // Check if name matches prefix
        if (strncmp(entry->d_name, filename_prefix, prefix_len) == 0) {
            // Calculate required buffer size
            size_t dir_len = safe_strlen(dir_path, sizeof(dir_path));
            size_t name_len = strlen(entry->d_name);
            size_t required_size = dir_len + 1 + name_len + 1;  // dir + / + name + \0

            // Skip if path would be too long
            if (required_size > MAX_COMPLETION_LENGTH) {
                continue;
            }

            // Build full match
            char full_match[MAX_COMPLETION_LENGTH];

            if (strcmp(dir_path, ".") == 0) {
                safe_strcpy(full_match, entry->d_name, sizeof(full_match));
            } else {
                // Manually build path to avoid truncation warning
                size_t written = 0;
                safe_strcpy(full_match, dir_path, sizeof(full_match));
                written = strlen(full_match);

                if (written + 1 < sizeof(full_match)) {
                    full_match[written++] = '/';
                    safe_strcpy(full_match + written, entry->d_name, sizeof(full_match) - written);
                }
            }

            // Build check path for stat
            char check_path[1024];

            if (strcmp(dir_path, ".") == 0) {
                safe_strcpy(check_path, entry->d_name, sizeof(check_path));
            } else {
                // Manually build to avoid warning
                safe_strcpy(check_path, dir_path, sizeof(check_path));
                size_t written = safe_strlen(check_path, sizeof(check_path));

                if (written + 1 + name_len < sizeof(check_path)) {
                    check_path[written++] = '/';
                    safe_strcpy(check_path + written, entry->d_name, sizeof(check_path) - written);
                }
            }

            struct stat st;
            if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                safe_strcat(full_match, "/", sizeof(full_match));
            }

            add_match(result, full_match);
        }
    }

    closedir(dp);
}

// Generate completions
CompletionResult *completion_generate(const char *line, size_t pos) {
    if (!line) return NULL;

    CompletionResult *result = create_result();
    if (!result) return NULL;

    // Extract word at cursor position
    size_t word_start = pos;
    while (word_start > 0 && !isspace(line[word_start - 1])) {
        word_start--;
    }

    char word[1024];
    size_t word_len = pos - word_start;
    if (word_len >= sizeof(word)) word_len = sizeof(word) - 1;
    memcpy(word, line + word_start, word_len);
    word[word_len] = '\0';

    // Determine if this is the first word (command) or argument
    int is_first_word = 1;
    for (size_t i = 0; i < word_start; i++) {
        if (!isspace(line[i])) {
            is_first_word = 0;
            break;
        }
    }

    if (is_first_word) {
        // Complete commands
        complete_commands(result, word);
    } else {
        // Complete files/directories
        complete_files(result, word);
    }

    // Calculate common prefix
    if (result->count > 0) {
        result->common_prefix = completion_common_prefix(result->matches, result->count);
    }

    return result;
}

// Free completion result
void completion_free_result(CompletionResult *result) {
    if (!result) return;

    if (result->matches) {
        for (int i = 0; i < result->count; i++) {
            free(result->matches[i]);
        }
        free(result->matches);
    }

    free(result->common_prefix);
    free(result);
}
