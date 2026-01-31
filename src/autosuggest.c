#include <string.h>
#include "autosuggest.h"
#include "history.h"
#include "safe_string.h"

// Cache for the last suggestion to avoid repeated lookups
static char cached_prefix[4096];
static char cached_suggestion[4096];
static int cache_valid = 0;

// Get autosuggestion for the current input
const char *autosuggest_get(const char *input, size_t len) {
    if (!input || len == 0) {
        return NULL;
    }

    // Check cache first
    if (cache_valid && len < sizeof(cached_prefix) &&
        strncmp(input, cached_prefix, len) == 0 &&
        cached_prefix[len] == '\0') {
        return cached_suggestion[0] ? cached_suggestion : NULL;
    }

    // Search history for a match
    const char *match = history_search_prefix(input);
    if (!match) {
        // Update cache with no suggestion
        safe_strcpy(cached_prefix, input, sizeof(cached_prefix));
        cached_suggestion[0] = '\0';
        cache_valid = 1;
        return NULL;
    }

    // Make sure the match is longer than input
    size_t match_len = strlen(match);
    if (match_len <= len) {
        safe_strcpy(cached_prefix, input, sizeof(cached_prefix));
        cached_suggestion[0] = '\0';
        cache_valid = 1;
        return NULL;
    }

    // Return the suffix (part after the current input)
    safe_strcpy(cached_prefix, input, sizeof(cached_prefix));
    safe_strcpy(cached_suggestion, match + len, sizeof(cached_suggestion));
    cache_valid = 1;

    return cached_suggestion;
}

// Clear the suggestion cache
void autosuggest_invalidate(void) {
    cache_valid = 0;
    cached_prefix[0] = '\0';
    cached_suggestion[0] = '\0';
}
