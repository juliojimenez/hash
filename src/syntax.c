#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "syntax.h"
#include "colors.h"
#include "color_config.h"
#include "builtins.h"
#include "config.h"
#include "safe_string.h"
#include "danger.h"

// Command cache for performance
#define CMD_CACHE_SIZE 128

typedef struct {
    char name[64];
    int result;  // 0=invalid, 1=external, 2=builtin, 3=alias
} CmdCacheEntry;

static CmdCacheEntry cmd_cache[CMD_CACHE_SIZE];
static int cmd_cache_count = 0;

// Simple hash function for command names
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Initialize syntax highlighting
void syntax_init(void) {
    cmd_cache_count = 0;
    memset(cmd_cache, 0, sizeof(cmd_cache));
}

// Clear command cache
void syntax_cache_clear(void) {
    cmd_cache_count = 0;
    memset(cmd_cache, 0, sizeof(cmd_cache));
}

// Check command validity with caching
int syntax_check_command(const char *cmd) {
    if (!cmd || !*cmd) return 0;

    // Check cache first
    unsigned int idx = hash_string(cmd) % CMD_CACHE_SIZE;
    if (cmd_cache[idx].name[0] && strcmp(cmd_cache[idx].name, cmd) == 0) {
        return cmd_cache[idx].result;
    }

    int result = 0;

    // Check if it's a builtin
    if (is_builtin(cmd)) {
        result = 2;
    }
    // Check if it's an alias
    else if (config_get_alias(cmd) != NULL) {
        result = 3;
    }
    // Check if it's in PATH
    else {
        char *path = find_in_path(cmd);
        if (path) {
            result = 1;
            free(path);
        }
    }

    // Cache the result
    safe_strcpy(cmd_cache[idx].name, cmd, sizeof(cmd_cache[idx].name));
    cmd_cache[idx].result = result;

    return result;
}

// Add a segment to the result
static void add_segment(SyntaxResult *result, size_t start, size_t end, SyntaxTokenType type) {
    if (start >= end) return;

    // Grow capacity if needed
    if (result->count >= result->capacity) {
        int new_cap = result->capacity * 2;
        if (new_cap < 16) new_cap = 16;
        SyntaxSegment *new_segs = realloc(result->segments, new_cap * sizeof(SyntaxSegment));
        if (!new_segs) return;
        result->segments = new_segs;
        result->capacity = new_cap;
    }

    result->segments[result->count].start = start;
    result->segments[result->count].end = end;
    result->segments[result->count].type = type;
    result->count++;
}

// Check if character is an operator character
static bool is_operator_char(char c) {
    return c == '|' || c == '&' || c == ';';
}

// Check if character starts a redirection
static bool is_redirect_char(char c) {
    return c == '>' || c == '<';
}

// Analyze input and return syntax segments
SyntaxResult *syntax_analyze(const char *input, size_t len) {
    SyntaxResult *result = malloc(sizeof(SyntaxResult));
    if (!result) return NULL;

    result->segments = NULL;
    result->count = 0;
    result->capacity = 0;

    if (!input || len == 0) return result;

    size_t i = 0;
    bool at_command_pos = true;  // True when next word is a command

    while (i < len) {
        // Skip whitespace
        while (i < len && (input[i] == ' ' || input[i] == '\t')) {
            i++;
        }
        if (i >= len) break;
        size_t start = i;

        // Comment
        if (input[i] == '#') {
            while (i < len && input[i] != '\n') i++;
            add_segment(result, start, i, SYN_COMMENT);
            continue;
        }

        // Operators: |, ||, &&, ;, &
        if (is_operator_char(input[i])) {
            if (input[i] == '|') {
                i++;
                if (i < len && input[i] == '|') i++;  // ||
            } else if (input[i] == '&') {
                i++;
                if (i < len && input[i] == '&') i++;  // &&
            } else {
                i++;  // ;
            }
            add_segment(result, start, i, SYN_OPERATOR);
            at_command_pos = true;
            continue;
        }

        // Redirections: >, >>, <, <<, 2>, 2>>, &>, etc.
        if (is_redirect_char(input[i]) ||
            (i + 1 < len && isdigit(input[i]) && is_redirect_char(input[i + 1]))) {
            // Handle fd prefix (e.g., 2>)
            if (isdigit(input[i])) i++;
            // Handle &> or >&
            if (input[i] == '&') i++;
            // Handle > or <
            if (i < len && is_redirect_char(input[i])) {
                i++;
                // Handle >> or <<
                if (i < len && (input[i] == '>' || input[i] == '<')) i++;
                // Handle &1 in 2>&1
                if (i < len && input[i] == '&') {
                    i++;
                    while (i < len && isdigit(input[i])) i++;
                }
            }
            add_segment(result, start, i, SYN_REDIRECT);
            continue;
        }

        // Single-quoted string
        if (input[i] == '\'') {
            i++;
            while (i < len && input[i] != '\'') i++;
            if (i < len) i++;  // Include closing quote
            add_segment(result, start, i, SYN_STRING_SINGLE);
            at_command_pos = false;
            continue;
        }

        // Double-quoted string
        if (input[i] == '"') {
            i++;
            while (i < len && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < len) i++;  // Skip escaped char
                i++;
            }
            if (i < len) i++;  // Include closing quote
            add_segment(result, start, i, SYN_STRING_DOUBLE);
            at_command_pos = false;
            continue;
        }

        // Variable: $VAR, ${VAR}, $(...), $((...))
        if (input[i] == '$') {
            i++;
            if (i < len) {
                if (input[i] == '{') {
                    // ${...}
                    i++;
                    int depth = 1;
                    while (i < len && depth > 0) {
                        if (input[i] == '{') depth++;
                        else if (input[i] == '}') depth--;
                        i++;
                    }
                } else if (input[i] == '(') {
                    // $(...) or $((...))
                    i++;
                    int depth = 1;
                    bool is_arith = (i < len && input[i] == '(');
                    if (is_arith) {
                        i++;
                        depth = 2;
                    }
                    while (i < len && depth > 0) {
                        if (input[i] == '(') depth++;
                        else if (input[i] == ')') depth--;
                        else if (input[i] == '\\' && i + 1 < len) i++;
                        i++;
                    }
                } else {
                    // $VAR or special: $?, $$, $!, $#, $@, $*, $0-$9
                    if (input[i] == '?' || input[i] == '$' || input[i] == '!' ||
                        input[i] == '#' || input[i] == '@' || input[i] == '*' ||
                        isdigit(input[i])) {
                        i++;
                    } else {
                        // Regular variable name
                        while (i < len && (isalnum(input[i]) || input[i] == '_')) i++;
                    }
                }
            }
            add_segment(result, start, i, SYN_VARIABLE);
            at_command_pos = false;
            continue;
        }

        // Glob characters when standalone
        if (input[i] == '*' || input[i] == '?') {
            i++;
            add_segment(result, start, i, SYN_GLOB);
            at_command_pos = false;
            continue;
        }

        // Word (command or argument)
        while (i < len && !isspace(input[i]) &&
               !is_operator_char(input[i]) &&
               !is_redirect_char(input[i]) &&
               input[i] != '\'' && input[i] != '"' &&
               input[i] != '$' && input[i] != '#') {
            // Handle backslash escapes
            if (input[i] == '\\' && i + 1 < len) {
                i += 2;
            } else {
                i++;
            }
        }

        if (i > start) {
            // Extract the word
            size_t word_len = i - start;
            char word[256];
            if (word_len < sizeof(word)) {
                memcpy(word, input + start, word_len);
                word[word_len] = '\0';

                SyntaxTokenType type;
                if (at_command_pos) {
                    // Check what kind of command it is
                    int cmd_type = syntax_check_command(word);
                    switch (cmd_type) {
                        case 2:  type = SYN_BUILTIN; break;
                        case 3:  type = SYN_ALIAS; break;
                        case 1:  type = SYN_COMMAND; break;
                        default: type = SYN_INVALID_CMD; break;
                    }
                    at_command_pos = false;
                } else {
                    // Check if word contains glob characters
                    bool has_glob = false;
                    for (size_t j = 0; j < word_len; j++) {
                        if (word[j] == '*' || word[j] == '?' || word[j] == '[') {
                            has_glob = true;
                            break;
                        }
                    }
                    type = has_glob ? SYN_GLOB : SYN_ARGUMENT;
                }
                add_segment(result, start, i, type);
            } else {
                add_segment(result, start, i, SYN_ARGUMENT);
                at_command_pos = false;
            }
        }
    }

    return result;
}

// Free syntax result
void syntax_result_free(SyntaxResult *result) {
    if (result) {
        free(result->segments);
        free(result);
    }
}

// Get color for a token type
static const char *get_token_color(SyntaxTokenType type) {
    switch (type) {
        case SYN_COMMAND:
            return color_config_get(color_config.syn_command);
        case SYN_BUILTIN:
            return color_config_get(color_config.syn_builtin);
        case SYN_ALIAS:
            return color_config_get(color_config.syn_command);  // Same as command
        case SYN_INVALID_CMD:
            return color_config_get(color_config.syn_invalid);
        case SYN_STRING_SINGLE:
        case SYN_STRING_DOUBLE:
            return color_config_get(color_config.syn_string);
        case SYN_VARIABLE:
            return color_config_get(color_config.syn_variable);
        case SYN_OPERATOR:
            return color_config_get(color_config.syn_operator);
        case SYN_REDIRECT:
            return color_config_get(color_config.syn_redirect);
        case SYN_COMMENT:
            return color_config_get(color_config.syn_comment);
        case SYN_GLOB:
            return color_config_get(color_config.syn_variable);  // Use variable color for globs
        case SYN_ARGUMENT:
        case SYN_NONE:
        default:
            return "";  // No color for arguments/plain text
    }
}

// Render input with syntax highlighting
char *syntax_render(const char *input, size_t len) {
    if (!input || len == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    // Check danger level if enabled
    DangerLevel danger_level = DANGER_NONE;
    if (color_config.danger_highlight_enabled) {
        danger_level = danger_check(input, len);
    }

    // Analyze the input
    SyntaxResult *result = syntax_analyze(input, len);
    if (!result) return NULL;

    // Calculate output size (input + color codes)
    // Each segment could add ~20 bytes for color codes
    size_t max_size = len + (result->count + 1) * 32 + 1;
    char *output = malloc(max_size);
    if (!output) {
        syntax_result_free(result);
        return NULL;
    }

    size_t out_pos = 0;
    size_t in_pos = 0;
    const char *reset = color_code(COLOR_RESET);
    size_t reset_len = strlen(reset);
    bool applied_danger = false;  // Track if we've applied danger color to first command

    for (int i = 0; i < result->count; i++) {
        const SyntaxSegment *seg = &result->segments[i];

        // Copy any text before this segment (whitespace)
        while (in_pos < seg->start && out_pos < max_size - 1) {
            output[out_pos++] = input[in_pos++];
        }

        // Get color for this segment
        const char *color = get_token_color(seg->type);

        // Override with danger color for first command if dangerous
        if (!applied_danger && danger_level != DANGER_NONE &&
            (seg->type == SYN_COMMAND || seg->type == SYN_BUILTIN ||
             seg->type == SYN_ALIAS || seg->type == SYN_INVALID_CMD)) {
            if (danger_level == DANGER_HIGH) {
                color = color_config_get(color_config.danger_high);
            } else {
                color = color_config_get(color_config.danger);
            }
            applied_danger = true;
        }
        size_t color_len = strlen(color);

        // Add color code
        if (color_len > 0 && out_pos + color_len < max_size) {
            memcpy(output + out_pos, color, color_len);
            out_pos += color_len;
        }

        // Copy segment text
        size_t seg_len = seg->end - seg->start;
        if (out_pos + seg_len < max_size) {
            memcpy(output + out_pos, input + seg->start, seg_len);
            out_pos += seg_len;
            in_pos = seg->end;
        }

        // Add reset after colored segment
        if (color_len > 0 && out_pos + reset_len < max_size) {
            memcpy(output + out_pos, reset, reset_len);
            out_pos += reset_len;
        }
    }

    // Copy any remaining text
    while (in_pos < len && out_pos < max_size - 1) {
        output[out_pos++] = input[in_pos++];
    }

    output[out_pos] = '\0';

    syntax_result_free(result);
    return output;
}
