#ifndef COMPLETION_H
#define COMPLETION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_COMPLETIONS 256
#define MAX_COMPLETION_LENGTH 1024

// Completion result
typedef struct {
    char **matches;      // Array of matching strings
    int count;           // Number of matches
    char *common_prefix; // Common prefix of all matches
} CompletionResult;

/* Actions a `complete` spec can ask for, combinable */
#define COMP_ACTION_FILES      0x01  /* -f */
#define COMP_ACTION_DIRS       0x02  /* -d */
#define COMP_ACTION_COMMANDS   0x04  /* -c */
#define COMP_ACTION_VARIABLES  0x08  /* -v */
#define COMP_ACTION_ALIASES    0x10  /* -a */
#define COMP_ACTION_BUILTINS   0x20  /* -b */
#define COMP_ACTION_USERS      0x40  /* -u */

/* What `complete` recorded for one command */
typedef struct CompSpec {
    char *command;
    char *wordlist;        /* -W: candidates separated by blanks, filtered by the word */
    char *function;        /* -F: a shell function; its stdout lines that start with the word are the candidates */
    unsigned actions;      /* COMP_ACTION_* */
    bool fallback_default; /* -o default: files when nothing else matched */
    bool fallback_dirs;    /* -o dirnames: directories when nothing else matched */
    struct CompSpec *next;
} CompSpec;

/**
 * Initialize completion system
 */
void completion_init(void);

/* The executables on PATH, scanned once and kept until PATH changes or the
   scan is older than a few seconds; sorted, without duplicates. */
size_t completion_path_commands(const char *const **names);
void completion_path_cache_invalidate(void);

/* Damerau-Levenshtein distance (adjacent transpositions count one), for did-you-mean */
int completion_edit_distance(const char *a, const char *b);

/* The closest command names to a misspelt one: builtins, aliases, functions and
   PATH executables within a small edit distance. Fills out with up to max
   strdup'd names, best first; returns how many. */
int completion_suggest_commands(const char *name, char **out, int max);

/* Programmable completion: define, remove, look up and print specs */
int completion_spec_define(const char *command, const char *wordlist, const char *function,
                           unsigned actions, bool fallback_default, bool fallback_dirs);
bool completion_spec_remove(const char *command);
void completion_spec_remove_all(void);
const CompSpec *completion_spec_find(const char *command);
const CompSpec *completion_spec_first(void);
void completion_spec_print(FILE *out, const CompSpec *spec);

/**
 * Generate completions for the current input
 *
 * Completes:
 * - Commands (first word, from PATH + built-ins + aliases)
 * - Files/directories (subsequent words)
 * - Aliases
 * - Environment variables (after $)
 *
 * @param line Current input line
 * @param pos Cursor position in line
 * @return CompletionResult (caller must free with completion_free_result)
 */
CompletionResult *completion_generate(const char *line, size_t pos);

/**
 * Free completion result
 *
 * @param result Result to free
 */
void completion_free_result(CompletionResult *result);

/**
 * Find common prefix of all matches
 * Returns newly allocated string (caller must free)
 *
 * @param matches Array of strings
 * @param count Number of strings
 * @return Common prefix, or NULL if none
 */
char *completion_common_prefix(char **matches, int count);

#endif // COMPLETION_H
