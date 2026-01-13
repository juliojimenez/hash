#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "parser.h"
#include "lineedit.h"

// Read a line from stdin
char *read_line(const char *prompt) {
    // Use line editor for interactive input
    char *line = lineedit_read_line(prompt);

    if (!line) {
        // EOF received (Ctrl+D on empty line)
        exit(EXIT_SUCCESS);
    }

    return line;
}

char **parse_line(const char *line) {
    int bufsize = MAX_ARGS;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    const char *read_pos;
    char *write_pos;
    int in_single_quote = 0;
    int in_double_quote = 0;
    size_t token_start_idx = 0;
    int token_has_content = 0;  // Track if current token has any content (including empty quotes)

    if (!tokens) {
        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
        exit(EXIT_FAILURE);
    }

    // Allocate output buffer - worst case is 2x input size (every char gets a marker)
    size_t line_len = strlen(line);
    char *output = malloc(line_len * 2 + 1);
    if (!output) {
        free(tokens);
        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
        exit(EXIT_FAILURE);
    }

    read_pos = line;
    write_pos = output;

    // Skip leading whitespace
    while (*read_pos && isspace(*read_pos)) {
        read_pos++;
    }

    token_start_idx = (size_t)(write_pos - output);

    while (*read_pos) {
        if (*read_pos == '\\' && *(read_pos + 1)) {
            // Escape sequence
            read_pos++; // Skip the backslash

            if (in_single_quote) {
                // In single quotes, backslash has no special meaning (POSIX)
                // A single quote CANNOT occur within single quotes, so if the
                // character after backslash is a quote, it ends the string
                if (*read_pos == '\'') {
                    // Write the backslash and DON'T consume the quote
                    // Let the main loop handle the quote to close the string
                    *write_pos++ = '\\';
                    // Don't advance read_pos - the quote will be handled in next iteration
                } else {
                    // Keep both backslash and the following character
                    // Use marker before backslash when followed by $ or ` to prevent
                    // varexpand from processing \$ or \` as escape sequences
                    if (*read_pos == '$' || *read_pos == '`') {
                        *write_pos++ = '\x01';  // Marker to protect the backslash
                    }
                    *write_pos++ = '\\';
                    *write_pos++ = *read_pos++;
                }
            } else if (in_double_quote) {
                // In double quotes, backslash is special only before $ ` " \ newline
                switch (*read_pos) {
                    case '$':
                        // Use marker to prevent variable expansion
                        *write_pos++ = '\x01';
                        *write_pos++ = *read_pos++;
                        break;
                    case '`':
                    case '"':
                    case '\\':
                        // Remove backslash, keep the character
                        *write_pos++ = *read_pos++;
                        break;
                    case '\n':
                        // Line continuation - skip both backslash and newline
                        read_pos++;
                        break;
                    default:
                        // Keep both backslash and character
                        *write_pos++ = '\\';
                        *write_pos++ = *read_pos++;
                        break;
                }
            } else {
                // Unquoted: backslash quotes the next character (removes itself)
                if (*read_pos == '\n') {
                    // Line continuation - skip both backslash and newline
                    read_pos++;
                } else {
                    // Remove backslash, keep the character
                    *write_pos++ = *read_pos++;
                }
            }
        } else if (*read_pos == '\'' && !in_double_quote) {
            // Toggle single quote mode
            in_single_quote = !in_single_quote;
            token_has_content = 1;  // Mark that this token exists (even if empty after quotes)
            read_pos++;
        } else if (*read_pos == '"' && !in_single_quote) {
            // Toggle double quote mode
            in_double_quote = !in_double_quote;
            token_has_content = 1;  // Mark that this token exists (even if empty after quotes)
            read_pos++;
        } else if (*read_pos == '$' && !in_single_quote) {
            // Handle $(...) command substitution and $((...)) arithmetic
            if (*(read_pos + 1) == '(' && *(read_pos + 2) == '(') {
                // $((...)) arithmetic - keep everything until matching ))
                *write_pos++ = *read_pos++;  // $
                *write_pos++ = *read_pos++;  // (
                *write_pos++ = *read_pos++;  // (
                int depth = 1;
                while (*read_pos && depth > 0) {
                    if (*read_pos == '(' && *(read_pos + 1) == '(') {
                        depth++;
                        *write_pos++ = *read_pos++;
                        *write_pos++ = *read_pos++;
                    } else if (*read_pos == ')' && *(read_pos + 1) == ')') {
                        depth--;
                        *write_pos++ = *read_pos++;
                        *write_pos++ = *read_pos++;
                    } else {
                        *write_pos++ = *read_pos++;
                    }
                }
            } else if (*(read_pos + 1) == '(') {
                // $(...) command substitution - keep everything until matching )
                *write_pos++ = *read_pos++;  // $
                *write_pos++ = *read_pos++;  // (
                int depth = 1;
                while (*read_pos && depth > 0) {
                    if (*read_pos == '(') {
                        depth++;
                    } else if (*read_pos == ')') {
                        depth--;
                    }
                    *write_pos++ = *read_pos++;
                }
            } else {
                // Just a $ (variable)
                if (in_double_quote) {
                    // Mark as quoted variable - \x02$ means "don't glob the expansion"
                    *write_pos++ = '\x02';
                }
                *write_pos++ = *read_pos++;
            }
        } else if (*read_pos == '`' && !in_single_quote) {
            // Backtick command substitution - keep everything until matching `
            *write_pos++ = *read_pos++;  // Opening `
            while (*read_pos && *read_pos != '`') {
                if (*read_pos == '\\' && *(read_pos + 1) == '`') {
                    // Escaped backtick inside backticks
                    *write_pos++ = *read_pos++;  // backslash
                    *write_pos++ = *read_pos++;  // `
                } else {
                    *write_pos++ = *read_pos++;
                }
            }
            if (*read_pos == '`') {
                *write_pos++ = *read_pos++;  // Closing `
            }
        } else if (isspace(*read_pos) && !in_single_quote && !in_double_quote) {
            // End of token
            *write_pos = '\0';

            // Add token to array if it has content (including empty quoted strings)
            if (output[token_start_idx] != '\0' || token_has_content) {
                tokens[position] = &output[token_start_idx];
                position++;

                if (position >= bufsize) {
                    bufsize += MAX_ARGS;
                    char **new_tokens = realloc(tokens, bufsize * sizeof(char*));
                    if (!new_tokens) {
                        free(tokens);
                        free(output);
                        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                        exit(EXIT_FAILURE);
                    }
                    tokens = new_tokens;
                }
            }

            // Skip whitespace
            write_pos++;
            read_pos++;
            while (*read_pos && isspace(*read_pos)) {
                read_pos++;
            }

            // Start of next token
            token_start_idx = (size_t)(write_pos - output);
            token_has_content = 0;  // Reset for next token
        } else if ((*read_pos == '$' && in_single_quote) ||
                   ((*read_pos == '~' || *read_pos == '*' || *read_pos == '?' || *read_pos == '[') &&
                    (in_single_quote || in_double_quote))) {
            // Special characters inside quotes - use SOH marker (\x01) to prevent expansion
            // Handles: $ in single quotes, ~ in any quotes, glob chars (*, ?, [) in any quotes
            *write_pos++ = '\x01';
            *write_pos++ = *read_pos++;
        } else if ((*read_pos == '>' || *read_pos == '<') && !in_single_quote && !in_double_quote) {
            // Redirection operator - ends current token and starts a new one
            // But check if current token is just digits (fd number for redirection like 2>file)
            size_t current_token_len = (size_t)(write_pos - output) - token_start_idx;
            bool token_is_fd_number = false;
            if (current_token_len > 0 && current_token_len <= 2) {
                // Check if all characters in current token are digits (fd number)
                token_is_fd_number = true;
                for (size_t i = 0; i < current_token_len; i++) {
                    if (!isdigit((unsigned char)output[token_start_idx + i])) {
                        token_is_fd_number = false;
                        break;
                    }
                }
            }
            // If current token is a fd number, keep it as part of the redirection
            // Otherwise, end current token first
            if (!token_is_fd_number && (current_token_len > 0 || token_has_content)) {
                *write_pos++ = '\0';
                tokens[position] = &output[token_start_idx];
                position++;
                if (position >= bufsize) {
                    bufsize += MAX_ARGS;
                    char **new_tokens = realloc(tokens, bufsize * sizeof(char*));
                    if (!new_tokens) {
                        free(tokens);
                        free(output);
                        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                        exit(EXIT_FAILURE);
                    }
                    tokens = new_tokens;
                }
                token_start_idx = (size_t)(write_pos - output);
            }
            // Now collect the redirection operator (>, >>, <, <<, etc.)
            *write_pos++ = *read_pos++;
            // Check for >> or << or <<- or >& or <&
            while (*read_pos == '>' || *read_pos == '<' || *read_pos == '&' || *read_pos == '-') {
                *write_pos++ = *read_pos++;
            }
            // For >&N or <&N patterns, also collect the fd number
            // For >file or <file patterns, also collect the filename (until whitespace)
            while (*read_pos && !isspace(*read_pos) && *read_pos != '>' && *read_pos != '<' &&
                   *read_pos != '|' && *read_pos != '&' && *read_pos != ';' &&
                   *read_pos != '(' && *read_pos != ')') {
                *write_pos++ = *read_pos++;
            }
            // End this token
            *write_pos++ = '\0';
            tokens[position] = &output[token_start_idx];
            position++;
            if (position >= bufsize) {
                bufsize += MAX_ARGS;
                char **new_tokens = realloc(tokens, bufsize * sizeof(char*));
                if (!new_tokens) {
                    free(tokens);
                    free(output);
                    fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                    exit(EXIT_FAILURE);
                }
                tokens = new_tokens;
            }
            token_start_idx = (size_t)(write_pos - output);
            token_has_content = 0;
        } else {
            // Regular character
            *write_pos++ = *read_pos++;
        }
    }

    // Handle last token
    *write_pos = '\0';
    if (output[token_start_idx] != '\0' || token_has_content) {
        tokens[position] = &output[token_start_idx];
        position++;
    }

    tokens[position] = NULL;

    // Note: output buffer is intentionally not freed here.
    // The tokens array contains pointers into the output buffer, and callers
    // may hold references to tokens across multiple parse_line calls (e.g., during
    // alias expansion). The memory will be reclaimed when the process exits.
    // This matches typical shell behavior where input buffers persist.

    return tokens;
}
