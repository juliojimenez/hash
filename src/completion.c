#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "completion.h"
#include "safe_string.h"
#include "config.h"
#include "builtins.h"
#include "exec.h"
#include "execute.h"
#include "expand.h"
#include "script.h"
#include "shellvar.h"
#include "utils.h"

extern char **environ;

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

// Add a match unless the same text is already there
static int add_unique(CompletionResult *result, const char *match) {
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->matches[i], match) == 0) {
            return 0;
        }
    }
    return add_match(result, match);
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

// ============================================================================
// The executables on PATH, cached
// ============================================================================
// Tab completion and did-you-mean both need every command name. The PATH scan
// is the expensive part (one stat per entry), so it is done once and kept
// while PATH is unchanged and the scan is recent. Builtins, aliases and
// functions change at the prompt, so they are read live by the callers.

#define PATH_CACHE_TTL_SECONDS 15

static char **path_names = NULL;
static size_t path_count = 0;
static size_t path_cap = 0;
static char *path_cache_key = NULL;   // the PATH the cache was built from
static time_t path_cache_time = 0;

static void path_cache_clear(void) {
    for (size_t i = 0; i < path_count; i++) {
        free(path_names[i]);
    }
    free(path_names);
    path_names = NULL;
    path_count = 0;
    path_cap = 0;
    free(path_cache_key);
    path_cache_key = NULL;
}

void completion_path_cache_invalidate(void) {
    path_cache_time = 0;
}

static bool path_cache_push(const char *name) {
    if (path_count == path_cap) {
        size_t cap = path_cap ? path_cap * 2 : 512;
        char **fresh = realloc(path_names, cap * sizeof(char *));
        if (!fresh) {
            return false;
        }
        path_names = fresh;
        path_cap = cap;
    }
    path_names[path_count] = strdup(name);
    if (!path_names[path_count]) {
        return false;
    }
    path_count++;
    return true;
}

static int compare_cstrings(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void path_cache_refresh(void) {
    const char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "";
    }
    time_t now = time(NULL);
    if (path_names && path_cache_key && strcmp(path_cache_key, path_env) == 0
            && now - path_cache_time < PATH_CACHE_TTL_SECONDS) {
        return;
    }
    path_cache_clear();
    path_cache_key = strdup(path_env);
    path_cache_time = now;

    char *path_copy = strdup(path_env);
    if (!path_copy) {
        return;
    }
    char *saveptr;
    for (char *dir = strtok_r(path_copy, ":", &saveptr); dir; dir = strtok_r(NULL, ":", &saveptr)) {
        DIR *dp = opendir(dir);
        if (!dp) {
            continue;
        }
        struct dirent *entry;
        while ((entry = readdir(dp)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && access(full_path, X_OK) == 0) {
                path_cache_push(entry->d_name);
            }
        }
        closedir(dp);
    }
    free(path_copy);

    // Sorted and unique
    if (path_count > 1) {
        qsort(path_names, path_count, sizeof(char *), compare_cstrings);
        size_t kept = 1;
        for (size_t i = 1; i < path_count; i++) {
            if (strcmp(path_names[i], path_names[kept - 1]) == 0) {
                free(path_names[i]);
            } else {
                path_names[kept++] = path_names[i];
            }
        }
        path_count = kept;
    }
}

size_t completion_path_commands(const char *const **names) {
    path_cache_refresh();
    *names = (const char *const *)path_names;
    return path_count;
}

static void add_function_names(CompletionResult *result, const char *prefix);

// Complete commands: builtins, aliases, functions and the executables on PATH
static void complete_commands(CompletionResult *result, const char *prefix) {
    size_t prefix_len = strlen(prefix);

    add_builtin_commands(result, prefix, prefix_len);
    add_aliases(result, prefix, prefix_len);
    add_function_names(result, prefix);

    const char *const *names;
    size_t count = completion_path_commands(&names);
    for (size_t i = 0; i < count && result->count < MAX_COMPLETIONS; i++) {
        if (strncmp(names[i], prefix, prefix_len) == 0) {
            add_unique(result, names[i]);
        }
    }
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

// Appends / to full_match and returns true when the entry is a directory
static bool build_check_path(const struct dirent *entry, const char *dir_path, char *full_match, size_t name_len) {
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
        return true;
    }
    return false;
}

// Entries of dir_path that start with filename_prefix (directories only when dirs_only)
static void handle_directory(CompletionResult *result, const char *dir_path,
        const char *filename_prefix, size_t prefix_len, const char *tilde_part,
        bool has_tilde, bool dirs_only, bool bare_names) {
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

            // Build check path for stat; directories get a trailing slash
            bool is_dir = build_check_path(entry, dir_path, full_match, name_len);
            if (dirs_only && !is_dir) {
                continue;
            }
            if (bare_names && strncmp(full_match, "./", 2) == 0) {
                // The word had no directory part, so the match has none either
                memmove(full_match, full_match + 2, strlen(full_match + 2) + 1);
            }

            add_match(result, full_match);
        }
    }

    closedir(dp);
}

// Complete files and directories (only directories when dirs_only)
static void complete_files(CompletionResult *result, const char *prefix, bool dirs_only) {
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

    bool bare_names = !has_tilde && strchr(prefix, '/') == NULL;
    handle_directory(result, dir_path, filename_prefix, prefix_len, tilde_part,
            has_tilde, dirs_only, bare_names);

    free(expanded_prefix);
}

// ============================================================================
// Programmable completion: what the `complete` builtin records
// ============================================================================

static CompSpec *specs = NULL;

static CompSpec *spec_lookup(const char *command) {
    for (CompSpec *s = specs; s; s = s->next) {
        if (strcmp(s->command, command) == 0) {
            return s;
        }
    }
    return NULL;
}

const CompSpec *completion_spec_find(const char *command) {
    return command ? spec_lookup(command) : NULL;
}

const CompSpec *completion_spec_first(void) {
    return specs;
}

static char *dup_or_null(const char *text) {
    return (text && *text) ? strdup(text) : NULL;
}

int completion_spec_define(const char *command, const char *wordlist, const char *function,
                           unsigned actions, bool fallback_default, bool fallback_dirs) {
    if (!command || !*command) {
        return -1;
    }
    CompSpec *spec = spec_lookup(command);
    if (!spec) {
        spec = calloc(1, sizeof(*spec));
        if (!spec) {
            return -1;
        }
        spec->command = strdup(command);
        if (!spec->command) {
            free(spec);
            return -1;
        }
        // Keep definition order, so `complete` lists them as they were given
        CompSpec **tail = &specs;
        while (*tail) {
            tail = &(*tail)->next;
        }
        *tail = spec;
    }
    free(spec->wordlist);
    free(spec->function);
    spec->wordlist = dup_or_null(wordlist);
    spec->function = dup_or_null(function);
    spec->actions = actions;
    spec->fallback_default = fallback_default;
    spec->fallback_dirs = fallback_dirs;
    return 0;
}

static void spec_free(CompSpec *spec) {
    free(spec->command);
    free(spec->wordlist);
    free(spec->function);
    free(spec);
}

bool completion_spec_remove(const char *command) {
    for (CompSpec **link = &specs; *link; link = &(*link)->next) {
        if (strcmp((*link)->command, command) == 0) {
            CompSpec *gone = *link;
            *link = gone->next;
            spec_free(gone);
            return true;
        }
    }
    return false;
}

void completion_spec_remove_all(void) {
    while (specs) {
        CompSpec *gone = specs;
        specs = gone->next;
        spec_free(gone);
    }
}

// Print a word inside single quotes, the way it can be typed back
static void print_single_quoted(FILE *out, const char *text) {
    fputc('\'', out);
    for (const char *p = text; *p; p++) {
        if (*p == '\'') {
            fputs("'\\''", out);
        } else {
            fputc(*p, out);
        }
    }
    fputc('\'', out);
}

void completion_spec_print(FILE *out, const CompSpec *spec) {
    if (!spec) {
        return;
    }
    fputs("complete", out);
    static const struct { unsigned bit; char letter; } letters[] = {
        {COMP_ACTION_ALIASES, 'a'}, {COMP_ACTION_BUILTINS, 'b'}, {COMP_ACTION_COMMANDS, 'c'},
        {COMP_ACTION_DIRS, 'd'}, {COMP_ACTION_FILES, 'f'}, {COMP_ACTION_USERS, 'u'},
        {COMP_ACTION_VARIABLES, 'v'},
    };
    for (size_t i = 0; i < sizeof(letters) / sizeof(letters[0]); i++) {
        if (spec->actions & letters[i].bit) {
            fprintf(out, " -%c", letters[i].letter);
        }
    }
    if (spec->fallback_default) {
        fputs(" -o default", out);
    }
    if (spec->fallback_dirs) {
        fputs(" -o dirnames", out);
    }
    if (spec->wordlist) {
        fputs(" -W ", out);
        print_single_quoted(out, spec->wordlist);
    }
    if (spec->function) {
        fprintf(out, " -F %s", spec->function);
    }
    fprintf(out, " %s\n", spec->command);
}

// ============================================================================
// Where the cursor is: the word, the command it belongs to, its position
// ============================================================================

typedef struct {
    const char *line;
    size_t pos;
    size_t word_start;
    char word[MAX_COMPLETION_LENGTH];     // the text being completed
    char command[MAX_COMPLETION_LENGTH];  // first word of the current simple command
    char prev[MAX_COMPLETION_LENGTH];     // the word before the one being completed
    int word_index;                       // 0 when the word is the command itself
    int option_args;                      // earlier arguments that start with '-'
} CompContext;

// Does the character at i start a new simple command? ; | & and newline do;
// ( and { do when they stand on their own, so ${X} and $(...) do not count.
static bool separator_at(const char *line, size_t i) {
    char c = line[i];
    if (c == ';' || c == '|' || c == '&' || c == '\n') {
        return true;
    }
    if (c == '(' || c == '{') {
        bool after_dollar = (i > 0 && line[i - 1] == '$');
        bool at_start = (i == 0) || isspace((unsigned char)line[i - 1]) || line[i - 1] == '(';
        return at_start && !after_dollar;
    }
    return false;
}

static bool looks_like_assignment(const char *token) {
    for (size_t k = 0; token[k]; k++) {
        if (token[k] == '=') {
            return k > 0;
        }
        if (!isalnum((unsigned char)token[k]) && token[k] != '_') {
            return false;
        }
    }
    return false;
}

static void analyze_context(const char *line, size_t pos, CompContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->line = line;
    ctx->pos = pos;

    size_t word_start = pos;
    while (word_start > 0 && !isspace((unsigned char)line[word_start - 1])) {
        word_start--;
    }
    ctx->word_start = word_start;
    size_t word_len = pos - word_start;
    if (word_len >= sizeof(ctx->word)) {
        word_len = sizeof(ctx->word) - 1;
    }
    memcpy(ctx->word, line + word_start, word_len);
    ctx->word[word_len] = '\0';

    // The current simple command starts after the last separator
    size_t cmd_start = 0;
    for (size_t i = word_start; i > 0; i--) {
        if (separator_at(line, i - 1)) {
            cmd_start = i;
            break;
        }
    }

    // Walk its words up to the one being completed
    size_t i = cmd_start;
    bool have_command = false;
    while (i < word_start) {
        while (i < word_start && isspace((unsigned char)line[i])) {
            i++;
        }
        if (i >= word_start) {
            break;
        }
        size_t start = i;
        while (i < word_start && !isspace((unsigned char)line[i])) {
            i++;
        }
        size_t len = i - start;
        if (len >= MAX_COMPLETION_LENGTH) {
            len = MAX_COMPLETION_LENGTH - 1;
        }
        char token[MAX_COMPLETION_LENGTH];
        memcpy(token, line + start, len);
        token[len] = '\0';
        if (!have_command) {
            // Leading assignments and ! are not the command
            if (looks_like_assignment(token) || strcmp(token, "!") == 0) {
                continue;
            }
            safe_strcpy(ctx->command, token, sizeof(ctx->command));
            safe_strcpy(ctx->prev, token, sizeof(ctx->prev));
            have_command = true;
            ctx->word_index = 1;
            continue;
        }
        ctx->word_index++;
        if (token[0] == '-') {
            ctx->option_args++;
        }
        safe_strcpy(ctx->prev, token, sizeof(ctx->prev));
    }
    if (!have_command) {
        ctx->word_index = 0;  // the word being completed is the command
    }
}

// ============================================================================
// Completers
// ============================================================================

typedef struct {
    CompletionResult *result;
    const char *prefix;
    size_t prefix_len;
    const char *lead;   // text before the name in the match ($, ${ or nothing)
    const char *trail;  // text after it (} or nothing)
} NameCollector;

static void collect_name(NameCollector *nc, const char *name) {
    if (!name || strncmp(name, nc->prefix, nc->prefix_len) != 0) {
        return;
    }
    char match[MAX_COMPLETION_LENGTH];
    snprintf(match, sizeof(match), "%s%s%s", nc->lead, name, nc->trail);
    add_unique(nc->result, match);
}

static void collect_shellvar(const char *name, const char *value, int attrs, void *ctx) {
    (void)value;
    (void)attrs;
    collect_name((NameCollector *)ctx, name);
}

// Variable names from the shell table and the environment, wrapped in lead/trail
static void add_variable_names(CompletionResult *result, const char *prefix, const char *lead, const char *trail) {
    NameCollector nc = { result, prefix, strlen(prefix), lead, trail };
    shellvar_foreach(collect_shellvar, &nc);
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq || eq == *e) {
            continue;
        }
        char name[MAX_COMPLETION_LENGTH];
        size_t len = (size_t)(eq - *e);
        if (len >= sizeof(name)) {
            continue;
        }
        memcpy(name, *e, len);
        name[len] = '\0';
        collect_name(&nc, name);
    }
}

// $NAME and ${NAME}: the word starts with $
static void complete_variables(CompletionResult *result, const char *word) {
    if (word[1] == '{') {
        add_variable_names(result, word + 2, "${", "}");
    } else {
        add_variable_names(result, word + 1, "$", "");
    }
}

static void add_function_names(CompletionResult *result, const char *prefix) {
    size_t len = strlen(prefix);
    for (int i = 0; i < script_state.function_count; i++) {
        const char *name = script_state.functions[i].name;
        if (name && strncmp(name, prefix, len) == 0) {
            add_unique(result, name);
        }
    }
}

// ~user: login names, each as ~name/
static void complete_users(CompletionResult *result, const char *word) {
    const char *prefix = word + 1;
    size_t len = strlen(prefix);
    setpwent();
    struct passwd *pw;
    while ((pw = getpwent()) != NULL && result->count < MAX_COMPLETIONS) {
        if (pw->pw_name && strncmp(pw->pw_name, prefix, len) == 0) {
            char match[MAX_COMPLETION_LENGTH];
            snprintf(match, sizeof(match), "~%s/", pw->pw_name);
            add_unique(result, match);
        }
    }
    endpwent();
}

// Lines printed by a command, each a candidate when it starts with the prefix
static void add_lines_from_command(CompletionResult *result, const char *command, const char *prefix) {
    FILE *fp = popen(command, "r");
    if (!fp) {
        return;
    }
    char line[MAX_COMPLETION_LENGTH];
    size_t len = strlen(prefix);
    while (fgets(line, sizeof(line), fp) && result->count < MAX_COMPLETIONS) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] && strncmp(line, prefix, len) == 0) {
            add_unique(result, line);
        }
    }
    pclose(fp);
}

// The words of a `complete -W` list that start with the prefix
static void add_words(CompletionResult *result, const char *words, const char *prefix) {
    size_t len = strlen(prefix);
    const char *p = words;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
        size_t n = (size_t)(p - start);
        if (n > 0 && n < MAX_COMPLETION_LENGTH) {
            char word[MAX_COMPLETION_LENGTH];
            memcpy(word, start, n);
            word[n] = '\0';
            if (strncmp(word, prefix, len) == 0) {
                add_unique(result, word);
            }
        }
    }
}

// Run a `complete -F` function with stdout captured: each line that starts
// with the word being completed is a candidate, so a function may print its
// whole vocabulary or filter it itself. $1 is the command, $2 the word being
// completed, $3 the word before it; COMP_LINE, COMP_POINT and COMP_CWORD
// describe the line.
static void add_from_function(CompletionResult *result, const char *name, const CompContext *ctx) {
    ShellFunction *func = script_get_function(name);
    if (!func) {
        return;
    }
    char path[] = "/tmp/hash-complete-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return;
    }
    unlink(path);
    fflush(stdout);
    int saved_out = fcntl(STDOUT_FILENO, F_DUPFD, 20);
    if (saved_out < 0) {
        close(fd);
        return;
    }
    dup2(fd, STDOUT_FILENO);

    int saved_status = last_command_exit_code;
    char number[32];
    shellvar_set("COMP_LINE", ctx->line);
    snprintf(number, sizeof(number), "%zu", ctx->pos);
    shellvar_set("COMP_POINT", number);
    snprintf(number, sizeof(number), "%d", ctx->word_index);
    shellvar_set("COMP_CWORD", number);

    char *argv[5];
    argv[0] = func->name;
    argv[1] = (char *)ctx->command;
    argv[2] = (char *)ctx->word;
    argv[3] = (char *)ctx->prev;
    argv[4] = NULL;
    exec_run_function(func, 4, argv);

    shellvar_unset("COMP_LINE");
    shellvar_unset("COMP_POINT");
    shellvar_unset("COMP_CWORD");
    last_command_exit_code = saved_status;

    fflush(stdout);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_out);

    lseek(fd, 0, SEEK_SET);
    FILE *fp = fdopen(fd, "r");
    if (!fp) {
        close(fd);
        return;
    }
    char line[MAX_COMPLETION_LENGTH];
    size_t word_len = strlen(ctx->word);
    while (fgets(line, sizeof(line), fp) && result->count < MAX_COMPLETIONS) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] && strncmp(line, ctx->word, word_len) == 0) {
            add_unique(result, line);
        }
    }
    fclose(fp);
}

static void complete_from_spec(CompletionResult *result, const CompSpec *spec, const CompContext *ctx) {
    const char *word = ctx->word;
    if (spec->actions & COMP_ACTION_ALIASES) {
        add_aliases(result, word, strlen(word));
    }
    if (spec->actions & COMP_ACTION_BUILTINS) {
        add_builtin_commands(result, word, strlen(word));
    }
    if (spec->actions & COMP_ACTION_COMMANDS) {
        complete_commands(result, word);
    }
    if (spec->actions & COMP_ACTION_VARIABLES) {
        add_variable_names(result, word, "", "");
    }
    if (spec->actions & COMP_ACTION_USERS) {
        char tilde[MAX_COMPLETION_LENGTH];
        snprintf(tilde, sizeof(tilde), "~%s", word);
        complete_users(result, tilde);
    }
    if (spec->actions & COMP_ACTION_DIRS) {
        complete_files(result, word, true);
    }
    if (spec->actions & COMP_ACTION_FILES) {
        complete_files(result, word, false);
    }
    if (spec->wordlist) {
        add_words(result, spec->wordlist, word);
    }
    if (spec->function) {
        add_from_function(result, spec->function, ctx);
    }
    if (result->count == 0 && spec->fallback_dirs) {
        complete_files(result, word, true);
    }
    if (result->count == 0 && spec->fallback_default) {
        complete_files(result, word, false);
    }
}

static bool in_list(const char *word, const char *const *list) {
    for (int i = 0; list[i]; i++) {
        if (strcmp(word, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

// The first argument of the current command that does not start with -
static void first_operand(const CompContext *ctx, char *out, size_t size) {
    out[0] = '\0';
    const char *line = ctx->line;
    size_t i = 0;
    bool past_command = false;
    while (i < ctx->word_start) {
        while (i < ctx->word_start && isspace((unsigned char)line[i])) {
            i++;
        }
        size_t start = i;
        while (i < ctx->word_start && !isspace((unsigned char)line[i])) {
            i++;
        }
        if (i == start) {
            break;
        }
        size_t n = i - start;
        if (!past_command) {
            if (n == strlen(ctx->command) && strncmp(line + start, ctx->command, n) == 0) {
                past_command = true;
            }
            continue;
        }
        if (line[start] == '-') {
            continue;
        }
        if (n < size) {
            memcpy(out, line + start, n);
            out[n] = '\0';
        }
        return;
    }
}

// git: subcommands first, then refs, remotes or files depending on the subcommand
static const char *const git_subcommands[] = {
    "add", "am", "apply", "archive", "bisect", "blame", "branch", "bundle", "checkout",
    "cherry-pick", "clean", "clone", "commit", "config", "describe", "diff", "difftool",
    "fetch", "format-patch", "gc", "grep", "help", "init", "log", "merge", "mv", "notes",
    "pull", "push", "range-diff", "rebase", "reflog", "remote", "reset", "restore",
    "revert", "rm", "shortlog", "show", "sparse-checkout", "stash", "status", "submodule",
    "switch", "tag", "worktree", NULL,
};

static void complete_git(CompletionResult *result, const CompContext *ctx) {
    const char *word = ctx->word;
    if (ctx->word_index - ctx->option_args == 1) {
        size_t len = strlen(word);
        for (int i = 0; git_subcommands[i]; i++) {
            if (strncmp(git_subcommands[i], word, len) == 0) {
                add_unique(result, git_subcommands[i]);
            }
        }
        return;
    }
    char sub[MAX_COMPLETION_LENGTH];
    first_operand(ctx, sub, sizeof(sub));
    static const char *const ref_commands[] = {
        "checkout", "switch", "merge", "rebase", "branch", "log", "diff", "reset",
        "cherry-pick", "show", "revert", "rev-parse", "tag", NULL,
    };
    static const char *const remote_commands[] = { "push", "pull", "fetch", "remote", NULL };
    static const char *const file_commands[] = {
        "add", "rm", "mv", "restore", "checkout", "diff", "show", "blame", "grep", "log", NULL,
    };
    if (in_list(sub, ref_commands)) {
        add_lines_from_command(result,
            "git for-each-ref --format='%(refname:short)' refs/heads refs/remotes refs/tags 2>/dev/null", word);
    }
    if (in_list(sub, remote_commands)) {
        add_lines_from_command(result, "git remote 2>/dev/null", word);
    }
    if (in_list(sub, file_commands) || result->count == 0) {
        complete_files(result, word, false);
    }
}

// make: targets of the makefile in the current directory
static void add_make_targets_from(CompletionResult *result, const char *path, const char *prefix) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }
    size_t len = strlen(prefix);
    char line[4096];
    while (fgets(line, sizeof(line), fp) && result->count < MAX_COMPLETIONS) {
        if (line[0] == '\t' || line[0] == '#' || line[0] == '\n') {
            continue;  // recipe, comment, blank
        }
        char *colon = strchr(line, ':');
        if (!colon || colon == line || colon[1] == '=') {
            continue;  // no rule, or an assignment with :=
        }
        *colon = '\0';
        if (strpbrk(line, "=$%#") != NULL) {
            continue;  // assignment, variable or pattern rule
        }
        char *save = NULL;
        for (char *name = strtok_r(line, " \t", &save); name; name = strtok_r(NULL, " \t", &save)) {
            if (name[0] == '.' || name[0] == '\0') {
                continue;  // .PHONY and friends
            }
            if (strncmp(name, prefix, len) == 0) {
                add_unique(result, name);
            }
        }
    }
    fclose(fp);
}

static void complete_make(CompletionResult *result, const CompContext *ctx) {
    const char *word = ctx->word;
    if (strcmp(ctx->prev, "-f") == 0 || strcmp(ctx->prev, "-C") == 0 || strchr(word, '/')) {
        complete_files(result, word, strcmp(ctx->prev, "-C") == 0);
        return;
    }
    static const char *const names[] = { "GNUmakefile", "makefile", "Makefile", NULL };
    for (int i = 0; names[i]; i++) {
        if (access(names[i], R_OK) == 0) {
            add_make_targets_from(result, names[i], word);
            break;
        }
    }
    if (result->count == 0) {
        complete_files(result, word, false);
    }
}

// man: page names from the manual directories
static void add_man_pages_from(CompletionResult *result, const char *dir, const char *prefix) {
    DIR *dp = opendir(dir);
    if (!dp) {
        return;
    }
    size_t len = strlen(prefix);
    const struct dirent *section;
    while ((section = readdir(dp)) != NULL && result->count < MAX_COMPLETIONS) {
        if (strncmp(section->d_name, "man", 3) != 0) {
            continue;
        }
        char sub[PATH_MAX];
        snprintf(sub, sizeof(sub), "%s/%s", dir, section->d_name);
        DIR *sp = opendir(sub);
        if (!sp) {
            continue;
        }
        const struct dirent *page;
        while ((page = readdir(sp)) != NULL && result->count < MAX_COMPLETIONS) {
            if (page->d_name[0] == '.' || strncmp(page->d_name, prefix, len) != 0) {
                continue;
            }
            char name[MAX_COMPLETION_LENGTH];
            safe_strcpy(name, page->d_name, sizeof(name));
            // Strip a compression suffix, then the section
            static const char *const zips[] = { ".gz", ".bz2", ".xz", ".Z", ".lzma", ".zst", NULL };
            for (int z = 0; zips[z]; z++) {
                size_t zl = strlen(zips[z]);
                size_t nl = strlen(name);
                if (nl > zl && strcmp(name + nl - zl, zips[z]) == 0) {
                    name[nl - zl] = '\0';
                    break;
                }
            }
            char *dot = strrchr(name, '.');
            if (dot && dot != name) {
                *dot = '\0';
            }
            if (name[0] && strncmp(name, prefix, len) == 0) {
                add_unique(result, name);
            }
        }
        closedir(sp);
    }
    closedir(dp);
}

static void complete_man(CompletionResult *result, const char *word) {
    char dirs[4096] = "";
    const char *env = getenv("MANPATH");
    if (env && *env) {
        safe_strcpy(dirs, env, sizeof(dirs));
    } else {
        FILE *fp = popen("manpath 2>/dev/null", "r");
        if (fp) {
            if (fgets(dirs, sizeof(dirs), fp)) {
                dirs[strcspn(dirs, "\r\n")] = '\0';
            }
            pclose(fp);
        }
        if (dirs[0] == '\0') {
            safe_strcpy(dirs, "/usr/share/man:/usr/local/share/man:/opt/homebrew/share/man", sizeof(dirs));
        }
    }
    char *save = NULL;
    for (char *dir = strtok_r(dirs, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        if (*dir) {
            add_man_pages_from(result, dir, word);
        }
    }
}

// Commands whose first operand is itself a command
static const char *const command_prefixes[] = {
    "command", "exec", "builtin", "type", "which", "sudo", "doas", "nohup", "nice", "time",
    "env", "xargs", "watch", NULL,
};

static void complete_argument(CompletionResult *result, const CompContext *ctx) {
    const char *word = ctx->word;
    const char *cmd = ctx->command;

    const CompSpec *spec = spec_lookup(cmd);
    if (spec) {
        complete_from_spec(result, spec, ctx);
        return;
    }
    if (word[0] == '-') {
        return;  // options are not completed
    }
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "pushd") == 0 || strcmp(cmd, "rmdir") == 0) {
        complete_files(result, word, true);
    } else if (strcmp(cmd, "unalias") == 0) {
        add_aliases(result, word, strlen(word));
    } else if (strcmp(cmd, "unset") == 0) {
        if (strcmp(ctx->prev, "-f") == 0) {
            add_function_names(result, word);
        } else {
            add_variable_names(result, word, "", "");
        }
    } else if (strcmp(cmd, "export") == 0 || strcmp(cmd, "readonly") == 0 || strcmp(cmd, "local") == 0) {
        add_variable_names(result, word, "", "");
    } else if (in_list(cmd, command_prefixes)) {
        if (ctx->word_index - ctx->option_args == 1) {
            complete_commands(result, word);
        } else {
            complete_files(result, word, false);
        }
    } else if (strcmp(cmd, "git") == 0) {
        complete_git(result, ctx);
    } else if (strcmp(cmd, "make") == 0 || strcmp(cmd, "gmake") == 0) {
        complete_make(result, ctx);
    } else if (strcmp(cmd, "man") == 0) {
        complete_man(result, word);
    } else {
        complete_files(result, word, false);
    }
}

// ============================================================================
// Did you mean: the closest command names to a misspelt one
// ============================================================================

#define SUGGEST_MAX_NAME 64

int completion_edit_distance(const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la > SUGGEST_MAX_NAME || lb > SUGGEST_MAX_NAME) {
        return INT_MAX;
    }
    // Optimal string alignment: insert, delete, substitute, swap neighbours
    int rows[SUGGEST_MAX_NAME + 1][SUGGEST_MAX_NAME + 1];
    for (size_t i = 0; i <= la; i++) rows[i][0] = (int)i;
    for (size_t j = 0; j <= lb; j++) rows[0][j] = (int)j;
    for (size_t i = 1; i <= la; i++) {
        for (size_t j = 1; j <= lb; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int best = rows[i - 1][j] + 1;
            if (rows[i][j - 1] + 1 < best) best = rows[i][j - 1] + 1;
            if (rows[i - 1][j - 1] + cost < best) best = rows[i - 1][j - 1] + cost;
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]
                    && rows[i - 2][j - 2] + 1 < best) {
                best = rows[i - 2][j - 2] + 1;
            }
            rows[i][j] = best;
        }
    }
    return rows[la][lb];
}

typedef struct {
    const char *name;
    char **out;
    int *distances;
    int max;
    int count;
    int threshold;
} Suggestions;

// Keep candidate when it is close enough and among the best so far (distance, then name)
static void suggest_consider(Suggestions *s, const char *candidate) {
    if (!candidate || strcmp(candidate, s->name) == 0) {
        return;
    }
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->out[i], candidate) == 0) {
            return;
        }
    }
    int d = completion_edit_distance(s->name, candidate);
    if (d > s->threshold) {
        return;
    }
    int at = s->count;
    while (at > 0 && (s->distances[at - 1] > d
                      || (s->distances[at - 1] == d && strcmp(s->out[at - 1], candidate) > 0))) {
        at--;
    }
    if (at >= s->max) {
        return;
    }
    if (s->count == s->max) {
        free(s->out[s->count - 1]);
        s->count--;
    }
    for (int i = s->count; i > at; i--) {
        s->out[i] = s->out[i - 1];
        s->distances[i] = s->distances[i - 1];
    }
    s->out[at] = strdup(candidate);
    s->distances[at] = d;
    if (s->out[at]) {
        s->count++;
    }
}

int completion_suggest_commands(const char *name, char **out, int max) {
    if (!name || !*name || max <= 0 || strlen(name) > SUGGEST_MAX_NAME) {
        return 0;
    }
    int distances[16];
    if (max > 16) {
        max = 16;
    }
    Suggestions s = { name, out, distances, max, 0, strlen(name) <= 3 ? 1 : 2 };
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        suggest_consider(&s, builtins[i].name);
    }
    for (int i = 0; i < shell_config.alias_count; i++) {
        suggest_consider(&s, shell_config.aliases[i].name);
    }
    for (int i = 0; i < script_state.function_count; i++) {
        suggest_consider(&s, script_state.functions[i].name);
    }
    const char *const *names;
    size_t count = completion_path_commands(&names);
    for (size_t i = 0; i < count; i++) {
        suggest_consider(&s, names[i]);
    }
    // Only the closest: a one-edit match makes two-edit ones noise
    while (s.count > 1 && distances[s.count - 1] > distances[0]) {
        free(out[s.count - 1]);
        s.count--;
    }
    return s.count;
}

static int compare_matches(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Generate completions
CompletionResult *completion_generate(const char *line, size_t pos) {
    if (!line) return NULL;

    CompletionResult *result = create_result();
    if (!result) return NULL;

    CompContext ctx;
    analyze_context(line, pos, &ctx);
    const char *word = ctx.word;

    if (word[0] == '$' && word[1] != '(') {
        complete_variables(result, word);
    } else if (word[0] == '~' && word[1] != '\0' && strchr(word, '/') == NULL) {
        complete_users(result, word);
    } else if (ctx.word_index == 0) {
        // The command: a path when it looks like one, otherwise names
        if (char_in_string(word[0], ".~/") || strchr(word, '/') != NULL) {
            complete_files(result, word, false);
        } else {
            complete_commands(result, word);
        }
    } else {
        complete_argument(result, &ctx);
    }

    if (result->count > 1) {
        qsort(result->matches, (size_t)result->count, sizeof(char *), compare_matches);
    }
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
