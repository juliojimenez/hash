#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>

#define HISTORY_DEFAULT_SIZE 1000
#define HISTORY_DEFAULT_FILESIZE 2000
#define HISTORY_MAX_LINE 4096

/**
 * Initialize history system
 * Loads history from file specified by HISTFILE (or ~/.hash_history)
 * Respects HISTSIZE and HISTFILESIZE environment variables
 */
void history_init(void);

/**
 * Add a command to history
 * Respects HISTCONTROL environment variable:
 * - ignorespace: Skip commands starting with space
 * - ignoredups: Skip if duplicate of previous
 * - ignoreboth: Both ignorespace and ignoredups
 * - erasedups: Remove all previous instances before adding
 *
 * Saves to file immediately if configured
 *
 * @param line Command line to add
 */
void history_add(const char *line);

/**
 * Get a history entry by index
 * 0 = oldest, history_count()-1 = newest
 *
 * @param index Index in history
 * @return Command string, or NULL if index invalid
 */
const char *history_get(int index);

/**
 * Get number of commands in history
 *
 * @return Number of history entries
 */
int history_count(void);

/**
 * Get current history position (for up/down navigation)
 * -1 means at the end (new command)
 *
 * @return Current position
 */
int history_get_position(void);

/**
 * Set history position (for up/down navigation)
 *
 * @param pos New position (-1 for end)
 */
void history_set_position(int pos);

/**
 * Move up in history (older)
 *
 * @return Previous command, or NULL if at oldest
 */
const char *history_prev(void);

/**
 * Move down in history (newer)
 *
 * @return Next command, or NULL if at newest
 */
const char *history_next(void);

/**
 * Reset position to end (for new command)
 */
void history_reset_position(void);

/**
 * Search history backwards for command starting with prefix
 *
 * @param prefix Prefix to search for
 * @return Matching command, or NULL if not found
 */
const char *history_search_prefix(const char *prefix);

/**
 * Search history for command containing substring
 * Searches from start_index in specified direction
 *
 * @param substring Substring to search for
 * @param start_index Index to start from (-1 means end for reverse)
 * @param direction 1 for reverse (older), -1 for forward (newer)
 * @param result_index Output: index of match, or -1 if not found
 * @return Matching command, or NULL if not found
 */
const char *history_search_substring(const char *substring, int start_index,
                                      int direction, int *result_index);

/**
 * Expand history references (!!, !n, !-n, !prefix)
 * Returns newly allocated string (caller must free)
 *
 * @param line Line containing history references
 * @return Expanded line, or NULL if expansion failed
 */
char *history_expand(const char *line);

/**
 * Save history to ~/.hash_history
 */
void history_save(void);

/**
 * Load history from ~/.hash_history
 */
void history_load(void);

/**
 * Clear all history
 */
void history_clear(void);

#endif // HISTORY_H
