#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include "completion.h"
#include "safe_string.h"
#include "config.h"
#include "builtins.h"
#include "expand.h"

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

static void add_builtin_commands(CompletionResult *result, const char *prefix, size_t prefix_len) {
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strncmp(builtins[i].name, prefix, prefix_len) == 0) {
            add_match(result, builtins[i].name);
        }
    }
}

static void add_aliases(CompletionResult *result, const char *prefix, size_t prefix_len) {
    for (int i = 0; i < shell_config.alias_count; i++) {
        if (strncmp(shell_config.aliases[i].name, prefix, prefix_len) == 0) {
            add_match(result, shell_config.aliases[i].name);
        }
    }
}

static void add_executables_from_path(CompletionResult *result, const char *prefix, size_t prefix_len) {
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
                char full_path[PATH_MAX];
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

// Complete commands from PATH
static void complete_commands(CompletionResult *result, const char *prefix) {
    size_t prefix_len = strlen(prefix);

    // Add built-in commands
    add_builtin_commands(result, prefix, prefix_len);

    // Add aliases
    add_aliases(result, prefix, prefix_len);

    // Search PATH for executables
    add_executables_from_path(result, prefix, prefix_len);
}

static void build_full_match(const struct dirent *entry, char *full_match,
        const char *dir_path, const char *tilde_part, bool has_tilde) {
    if (has_tilde && tilde_part[0] != '\0') {
        // Reconstruct path with original tilde prefix
        // We need to replace the expanded home path with tilde_part
        char *home_expanded = expand_tilde_path(tilde_part);
        if (home_expanded) {
            size_t home_len = strlen(home_expanded);
            size_t dir_path_len = strlen(dir_path);

            // Start with tilde part
            safe_strcpy(full_match, tilde_part, MAX_COMPLETION_LENGTH);

            // Add any path components after the home directory
            // e.g., if dir_path is /Users/julio/Documents and home is /Users/julio
            // then we add /Documents
            if (dir_path_len > home_len) {
                safe_strcat(full_match, dir_path + home_len, MAX_COMPLETION_LENGTH);
            }

            // Add slash and filename
            size_t written = strlen(full_match);
            if (written > 0 && full_match[written - 1] != '/'
                    && written + 1 < MAX_COMPLETION_LENGTH) {
                full_match[written++] = '/';
                full_match[written] = '\0';
            }
            safe_strcat(full_match, entry->d_name, MAX_COMPLETION_LENGTH);

            free(home_expanded);
        } else {
            // Fallback to expanded path
            safe_strcpy(full_match, dir_path, MAX_COMPLETION_LENGTH);
            size_t written = strlen(full_match);
            if (written > 0 && full_match[written - 1] != '/'
                    && written + 1 < MAX_COMPLETION_LENGTH) {
                full_match[written++] = '/';
                full_match[written] = '\0';
            }
            safe_strcat(full_match, entry->d_name, MAX_COMPLETION_LENGTH);
        }
    } else {
        // Manually build path to avoid truncation warning
        size_t written = 0;
        safe_strcpy(full_match, dir_path, MAX_COMPLETION_LENGTH);
        written = strlen(full_match);

        // Only add slash if dir_path doesn't already end with one
        if (written > 0 && full_match[written - 1] != '/'
                && written + 1 < MAX_COMPLETION_LENGTH) {
            full_match[written++] = '/';
            full_match[written] = '\0';
        }
        safe_strcat(full_match, entry->d_name, MAX_COMPLETION_LENGTH);
    }
}

static void build_check_path(const struct dirent *entry, const char *dir_path, char *full_match, size_t name_len) {
    char check_path[PATH_MAX];

    if (strcmp(dir_path, ".") == 0) {
        safe_strcpy(check_path, entry->d_name, sizeof(check_path));
    } else {
        // Manually build to avoid warning
        safe_strcpy(check_path, dir_path, sizeof(check_path));
        size_t written = safe_strlen(check_path, sizeof(check_path));

        // Only add slash if dir_path doesn't already end with one
        if (written > 0 && check_path[written - 1] != '/'
                && written + 1 + name_len < sizeof(check_path)) {
            check_path[written++] = '/';
        }
        if (written + name_len < sizeof(check_path)) {
            safe_strcpy(check_path + written, entry->d_name, sizeof(check_path) - written);
        }
    }

    struct stat st;
    if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        safe_strcat(full_match, "/", MAX_COMPLETION_LENGTH);
    }
}

// Returns false if couldn't open the directory
static void handle_directory(CompletionResult *result, const char *dir_path,
        const char *filename_prefix, size_t prefix_len, const char *tilde_part,
        bool has_tilde) {
    // Open directory
    DIR *dp = opendir(dir_path);
    if (!dp) {
        return;
    }

    const struct dirent *entry;
    while ((entry = readdir(dp)) != NULL && result->count < MAX_COMPLETIONS) {
        // Skip . and .. unless explicitly requested
        if (prefix_len == 0 && (strcmp(entry->d_name, ".") == 0
                    || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }

        // Check if name matches prefix
        if (strncmp(entry->d_name, filename_prefix, prefix_len) == 0) {
            // Calculate required buffer size
            size_t dir_len = safe_strlen(dir_path, PATH_MAX);
            size_t name_len = strlen(entry->d_name);
            size_t required_size = dir_len + 1 + name_len + 1;  // dir + / + name + \0

            // Skip if path would be too long
            if (required_size > MAX_COMPLETION_LENGTH) {
                continue;
            }

            // Build full match
            char full_match[MAX_COMPLETION_LENGTH];
            build_full_match(entry, full_match, dir_path, tilde_part,
                    has_tilde);

            // Build check path for stat
            build_check_path(entry, dir_path, full_match, name_len);

            add_match(result, full_match);
        }
    }

    closedir(dp);
}

// Complete files and directories
static void complete_files(CompletionResult *result, const char *prefix) {
    // Handle tilde expansion
    char *expanded_prefix = NULL;
    const char *working_prefix = prefix;
    char tilde_part[256] = {0};  // To preserve ~/ or ~user/ in results
    bool has_tilde = false;

    if (prefix[0] == '~') {
        has_tilde = true;
        expanded_prefix = expand_tilde_path(prefix);
        if (expanded_prefix) {
            working_prefix = expanded_prefix;
            // Save the tilde part (~ or ~user) for reconstructing matches
            const char *slash = strchr(prefix, '/');
            if (slash) {
                size_t tilde_len = (size_t)(slash - prefix);
                if (tilde_len < sizeof(tilde_part)) {
                    memcpy(tilde_part, prefix, tilde_len);
                    tilde_part[tilde_len] = '\0';
                }
            } else {
                // Just ~ or ~user with no slash
                safe_strcpy(tilde_part, prefix, sizeof(tilde_part));
            }
        }
    }

    // Split prefix into directory and filename
    char dir_path[PATH_MAX];
    const char *filename_prefix;

    // Check if original prefix had a slash (not the expanded one)
    const char *original_slash = strchr(prefix, '/');

    if (has_tilde && !original_slash && expanded_prefix) {
        // Just ~ or ~user with no slash - list home directory contents
        safe_strcpy(dir_path, expanded_prefix, sizeof(dir_path));
        filename_prefix = "";
    } else {
        const char *last_slash = strrchr(working_prefix, '/');
        if (last_slash) {
            // Has directory component
            size_t dir_len = (size_t)(last_slash - working_prefix);
            if (dir_len == 0) {
                // Root directory
                safe_strcpy(dir_path, "/", sizeof(dir_path));
            } else {
                memcpy(dir_path, working_prefix, dir_len);
                dir_path[dir_len] = '\0';
            }
            filename_prefix = last_slash + 1;
        } else {
            // Current directory
            safe_strcpy(dir_path, ".", sizeof(dir_path));
            filename_prefix = prefix;
        }
    }

    size_t prefix_len = strlen(filename_prefix);

    handle_directory(result, dir_path, filename_prefix, prefix_len, tilde_part,
            has_tilde);

    free(expanded_prefix);
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
    bool is_first_word = true;
    for (size_t i = 0; i < word_start; i++) {
        if (!isspace(line[i])) {
            is_first_word = false;
            break;
        }
    }

    // If it is first word and doesn't looks like a path, complete commands
    if (is_first_word && !(word[0] == '.' || word[0] == '~' || strchr(word, '/') != NULL)) {
        complete_commands(result, word);
    } else {
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
