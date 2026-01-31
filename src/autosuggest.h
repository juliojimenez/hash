#ifndef AUTOSUGGEST_H
#define AUTOSUGGEST_H

#include <stddef.h>

/**
 * Get autosuggestion for the current input
 * Returns the completion portion only (not including the prefix)
 *
 * @param input Current input buffer
 * @param len Length of input
 * @return Suggestion suffix (static buffer), or NULL if no suggestion
 */
const char *autosuggest_get(const char *input, size_t len);

/**
 * Clear the suggestion cache
 * Call when history changes
 */
void autosuggest_invalidate(void);

#endif // AUTOSUGGEST_H
