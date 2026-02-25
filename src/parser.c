#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "parser.h"
#include "lineedit.h"

typedef struct {
    int bufsize;
    int position;
    char **tokens;
    char *output;
    const char *read_pos;
    char *write_pos;
    int in_single_quote;
    int in_double_quote;
    size_t token_start_idx;
    int token_has_content; // Track if current token has any content (including empty quotes)
} Parser;

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

static void skip_whitespace(Parser *parser) {
    while (*parser->read_pos && isspace(*parser->read_pos)) {
        parser->read_pos++;
    }
}

static void mark_and_write_char(Parser *parser) {
    // Special characters inside quotes - use SOH marker (\x01) to prevent special handling
    // Handles: $ in single quotes, ~ in any quotes, glob chars (*, ?, [) in any quotes,
    // redirect/pipe operators (<, >, |, &, ;) in any quotes
    *parser->write_pos++ = '\x01';
    *parser->write_pos++ = *parser->read_pos++;
}

static bool char_in_string(char c, const char *str) {
    for (int i = 0; str[i]; ++i) {
        if (c == str[i]) {
            return true;
        }
    }

    return false;
}

static void handle_backslash(Parser *parser) {
    // In single quotes, backslash has NO special meaning - treat as regular char
    // BUT we need to mark it so varexpand doesn't process it
    if (parser->in_single_quote) {
        /* Add marker before backslash if next char is $, `, or \
           so varexpand knows this is literal and not an escape sequence */
        if (*(parser->read_pos + 1) == '$' || *(parser->read_pos + 1) == '`' || *(parser->read_pos + 1) == '\\') {
            mark_and_write_char(parser); // Marker for literal backslash
            return;
        }
        *parser->write_pos++ = *parser->read_pos++;
        return;
    }

    if (!(*(parser->read_pos + 1))) return;

    // Escape sequence (only outside single quotes)
    parser->read_pos++; // Skip the backslash

    if (parser->in_double_quote) {
        // In double quotes, backslash is special only before $ ` " \ newline
        switch (*parser->read_pos) {
            case '$':
                // Use marker to prevent variable expansion
                mark_and_write_char(parser);
                break;
            case '`':
            case '"':
            case '\\':
                // Remove backslash, keep the character
                *parser->write_pos++ = *parser->read_pos++;
                break;
            case '\n':
                // Line continuation - skip both backslash and newline
                parser->read_pos++;
                break;
            default:
                // Keep both backslash and character
                *parser->write_pos++ = '\\';
                *parser->write_pos++ = *parser->read_pos++;
                break;
        }
    } else {
        // Unquoted: backslash quotes the next character (removes itself)
        if (*parser->read_pos == '\n') {
            // Line continuation - skip both backslash and newline
            parser->read_pos++;
        } else {
            // Remove backslash, keep the character
            // If it's a glob character, redirection operator, or tilde, mark it to prevent special handling
            if (char_in_string(*parser->read_pos, "*?[<>|&;~")) {
                mark_and_write_char(parser); // Marker to prevent special interpretation
                return;
            }
            *parser->write_pos++ = *parser->read_pos++;
        }
    }
}

static void handle_doller(Parser *parser) {
    if (parser->in_single_quote) {
        mark_and_write_char(parser);
        return;
    }

    // Handle $(...) command substitution, $((...)) arithmetic, and ${...} parameter expansion
    if (*(parser->read_pos + 1) == '(' && *(parser->read_pos + 2) == '(') {
        // $((...)) arithmetic - keep everything until matching ))
        *parser->write_pos++ = *parser->read_pos++;  // $
        *parser->write_pos++ = *parser->read_pos++;  // (
        *parser->write_pos++ = *parser->read_pos++;  // (
        int depth = 1;
        while (*parser->read_pos && depth > 0) {
            if (*parser->read_pos == '(' && *(parser->read_pos + 1) == '(') {
                depth++;
                *parser->write_pos++ = *parser->read_pos++;
                *parser->write_pos++ = *parser->read_pos++;
            } else if (*parser->read_pos == ')' && *(parser->read_pos + 1) == ')') {
                depth--;
                *parser->write_pos++ = *parser->read_pos++;
                *parser->write_pos++ = *parser->read_pos++;
            } else {
                *parser->write_pos++ = *parser->read_pos++;
            }
        }
    } else if (*(parser->read_pos + 1) == '(') {
        // $(...) command substitution - keep everything until matching )
        // Mark as quoted if inside double quotes - don't glob the expansion
        if (parser->in_double_quote) {
            *parser->write_pos++ = '\x02';
        }
        *parser->write_pos++ = *parser->read_pos++;  // $
        *parser->write_pos++ = *parser->read_pos++;  // (
        int depth = 1;
        while (*parser->read_pos && depth > 0) {
            if (*parser->read_pos == '(') {
                depth++;
            } else if (*parser->read_pos == ')') {
                depth--;
            }
            *parser->write_pos++ = *parser->read_pos++;
        }
    } else if (*(parser->read_pos + 1) == '{') {
        // ${...} parameter expansion - keep everything until matching }
        // Must track nested braces for constructs like ${var:-${default}}
        if (parser->in_double_quote) {
            *parser->write_pos++ = '\x02';
        }
        *parser->write_pos++ = *parser->read_pos++;  // $
        *parser->write_pos++ = *parser->read_pos++;  // {
        int depth = 1;
        while (*parser->read_pos && depth > 0) {
            if (*parser->read_pos == '\\' && *(parser->read_pos + 1)) {
                // Escaped character - copy both
                *parser->write_pos++ = *parser->read_pos++;
                *parser->write_pos++ = *parser->read_pos++;
            } else if (*parser->read_pos == '\'' && !parser->in_double_quote) {
                // Single quote inside ${...} - strip quotes and mark glob chars as literal
                parser->read_pos++;  // Skip opening quote
                while (*parser->read_pos && *parser->read_pos != '\'') {
                    if (char_in_string(*parser->read_pos, "*?[")) {
                        // Glob character inside quotes - add marker for literal match
                        mark_and_write_char(parser);
                    } else {
                        *parser->write_pos++ = *parser->read_pos++;
                    }
                }
                if (*parser->read_pos == '\'') {
                    parser->read_pos++;  // Skip closing quote
                }
            } else if (*parser->read_pos == '"') {
                // Double quote inside ${...} - strip quotes and mark glob chars as literal
                parser->read_pos++;  // Skip opening quote
                while (*parser->read_pos && *parser->read_pos != '"') {
                    if (*parser->read_pos == '\\' && *(parser->read_pos + 1)) {
                        // Escaped character - keep the backslash escape
                        *parser->write_pos++ = *parser->read_pos++;
                        *parser->write_pos++ = *parser->read_pos++;
                    } else if (char_in_string(*parser->read_pos, "*?[")) {
                        // Glob character inside quotes - add marker for literal match
                        mark_and_write_char(parser);
                    } else {
                        *parser->write_pos++ = *parser->read_pos++;
                    }
                }
                if (*parser->read_pos == '"') {
                    parser->read_pos++;  // Skip closing quote
                }
            } else if (*parser->read_pos == '{') {
                depth++;
                *parser->write_pos++ = *parser->read_pos++;
            } else if (*parser->read_pos == '}') {
                depth--;
                *parser->write_pos++ = *parser->read_pos++;
            } else {
                *parser->write_pos++ = *parser->read_pos++;
            }
        }
    } else {
        // Just a $ (variable)
        if (parser->in_double_quote) {
            // Mark as quoted variable - \x02$ means "don't glob the expansion"
            *parser->write_pos++ = '\x02';
        }
        *parser->write_pos++ = *parser->read_pos++;
    }
}

static void handle_backtick(Parser *parser) {
    if (!parser->in_single_quote) {
        // Backtick command substitution - keep everything until matching `
        *parser->write_pos++ = *parser->read_pos++;  // Opening `
        while (*parser->read_pos && *parser->read_pos != '`') {
            if (*parser->read_pos == '\\' && *(parser->read_pos + 1) == '`') {
                // Escaped backtick inside backticks
                *parser->write_pos++ = *parser->read_pos++;  // backslash
                *parser->write_pos++ = *parser->read_pos++;  // `
            } else {
                *parser->write_pos++ = *parser->read_pos++;
            }
        }
        if (*parser->read_pos == '`') {
            *parser->write_pos++ = *parser->read_pos++;  // Closing `
        }
    }
}

static void handle_space(Parser *parser) {
    if (parser->in_single_quote || parser->in_double_quote) {
        *parser->write_pos++ = *parser->read_pos++;
        return;
    }

    // End of token
    *parser->write_pos = '\0';

    // Add token to array if it has content (including empty quoted strings)
    if (parser->output[parser->token_start_idx] != '\0' || parser->token_has_content) {
        parser->tokens[parser->position] = &parser->output[parser->token_start_idx];
        parser->position++;

        if (parser->position >= parser->bufsize) {
            parser->bufsize += MAX_ARGS;
            char **new_tokens = realloc(parser->tokens, parser->bufsize * sizeof(char*));
            if (!new_tokens) {
                free(parser->tokens);
                free(parser->output);
                fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                exit(EXIT_FAILURE);
            }
            parser->tokens = new_tokens;
        }
    }

    // Skip whitespace
    parser->write_pos++;
    parser->read_pos++;
    while (*parser->read_pos && isspace(*parser->read_pos)) {
        parser->read_pos++;
    }

    // Start of next token
    parser->token_start_idx = (size_t)(parser->write_pos - parser->output);
    parser->token_has_content = 0;  // Reset for next token
}

static void handle_angle_bracket(Parser *parser) {
    if (parser->in_single_quote || parser->in_double_quote) {
        mark_and_write_char(parser);
        return;
    }

    // Redirection operator - ends current token and starts a new one
    // But check if current token is just digits (fd number for redirection like 2>file)
    size_t current_token_len = (size_t)(parser->write_pos - parser->output) - parser->token_start_idx;
    bool token_is_fd_number = false;
    if (current_token_len > 0 && current_token_len <= 2) {
        // Check if all characters in current token are digits (fd number)
        token_is_fd_number = true;
        for (size_t i = 0; i < current_token_len; i++) {
            if (!isdigit((unsigned char)parser->output[parser->token_start_idx + i])) {
                token_is_fd_number = false;
                break;
            }
        }
    }
    // If current token is a fd number, keep it as part of the redirection
    // Otherwise, end current token first
    if (!token_is_fd_number && (current_token_len > 0 || parser->token_has_content)) {
        *parser->write_pos++ = '\0';
        parser->tokens[parser->position] = &parser->output[parser->token_start_idx];
        parser->position++;
        if (parser->position >= parser->bufsize) {
            parser->bufsize += MAX_ARGS;
            char **new_tokens = realloc(parser->tokens, parser->bufsize * sizeof(char*));
            if (!new_tokens) {
                free(parser->tokens);
                free(parser->output);
                fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                exit(EXIT_FAILURE);
            }
            parser->tokens = new_tokens;
        }
        parser->token_start_idx = (size_t)(parser->write_pos - parser->output);
    }
    // Now collect the redirection operator (>, >>, <, <<, etc.)
    *parser->write_pos++ = *parser->read_pos++;
    // Check for >> or << or <<- or >& or <&
    while (char_in_string(*parser->read_pos, "><&-")) {
        *parser->write_pos++ = *parser->read_pos++;
    }
    // For >&N or <&N patterns, also collect the fd number
    // For >file or <file patterns, also collect the filename (until whitespace)
    while (*parser->read_pos && !isspace(*parser->read_pos) && !char_in_string(*parser->read_pos, "><|&;()")) {
        *parser->write_pos++ = *parser->read_pos++;
    }
    // End this token
    *parser->write_pos++ = '\0';
    parser->tokens[parser->position] = &parser->output[parser->token_start_idx];
    parser->position++;
    if (parser->position >= parser->bufsize) {
        parser->bufsize += MAX_ARGS;
        char **new_tokens = realloc(parser->tokens, parser->bufsize * sizeof(char*));
        if (!new_tokens) {
            free(parser->tokens);
            free(parser->output);
            fprintf(stderr, "%s: allocation error\n", HASH_NAME);
            exit(EXIT_FAILURE);
        }
        parser->tokens = new_tokens;
    }
    parser->token_start_idx = (size_t)(parser->write_pos - parser->output);
    parser->token_has_content = 0;
}

static void handle_tilde(Parser *parser) {
    if (parser->in_single_quote || parser->in_double_quote) {
        mark_and_write_char(parser);
        return;
    }

    if ((size_t)(parser->write_pos - parser->output) == parser->token_start_idx) {
        // Tilde at start of word - check if any characters before / or end are quoted
        // POSIX: if any character in tilde-prefix is quoted, don't expand
        const char *lookahead = parser->read_pos + 1;
        bool has_quoted_chars = false;
        while (*lookahead && !isspace(*lookahead) && *lookahead != '/') {
            if (char_in_string(*lookahead, "'\"")) {
                has_quoted_chars = true;
                break;
            }
            if (*lookahead == '\\' && *(lookahead + 1)) {
                // Escaped character counts as quoted
                has_quoted_chars = true;
                break;
            }
            lookahead++;
        }
        if (has_quoted_chars) {
            // Mark tilde to prevent expansion
            *parser->write_pos++ = '\x01';
        }
        *parser->write_pos++ = *parser->read_pos++;
    } else {
        // Regular character
        *parser->write_pos++ = *parser->read_pos++;
    }
}

ParseResult parse_line(const char *line) {
    Parser parser = {
        .bufsize = MAX_ARGS,
        .position = 0,
        .in_single_quote = 0,
        .in_double_quote = 0,
        .token_start_idx = 0,
        .token_has_content = 0,
    };
    parser.tokens = malloc(parser.bufsize * sizeof(char*));
    if (!parser.tokens) {
        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
        exit(EXIT_FAILURE);
    }

    // Allocate output buffer - worst case is 2x input size (every char gets a marker)
    size_t line_len = strlen(line);
    parser.output = malloc(line_len * 2 + 1);
    if (!parser.output) {
        free(parser.tokens);
        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
        exit(EXIT_FAILURE);
    }

    parser.read_pos = line;
    parser.write_pos = parser.output;

    // Skip leading whitespace
    skip_whitespace(&parser);

    parser.token_start_idx = (size_t)(parser.write_pos - parser.output);

    while (*parser.read_pos) {
        switch (*parser.read_pos) {
            case '\\':
                handle_backslash(&parser);
                continue;

            case '\'':
                if (!parser.in_double_quote) {
                    // Toggle single quote mode
                    parser.in_single_quote = !parser.in_single_quote;
                    parser.token_has_content = 1;  // Mark that this token exists (even if empty after quotes)
                    parser.read_pos++;
                    continue;
                }
                break;

            case '"':
                if (!parser.in_single_quote) {
                    // Toggle double quote mode
                    parser.in_double_quote = !parser.in_double_quote;
                    parser.token_has_content = 1;  // Mark that this token exists (even if empty after quotes)
                    parser.read_pos++;
                    continue;
                }
                break;

            case '$':
                handle_doller(&parser);
                continue;

            case '`':
                handle_backtick(&parser);
                continue;

            case '<':
            case '>':
                handle_angle_bracket(&parser);
                continue;

            case '~':
                handle_tilde(&parser);
                continue;

            case '*':
            case '?':
            case '[':
            case '|':
            case '&':
            case ';':
                if (parser.in_single_quote || parser.in_double_quote) {
                    mark_and_write_char(&parser);
                    continue;
                }
                break;

            default:
                if (isspace(*parser.read_pos)) {
                    handle_space(&parser);
                    continue;
                }
                break;
        }

        // Regular character
        *parser.write_pos++ = *parser.read_pos++;
    }


    // Handle last token
    *parser.write_pos = '\0';
    if (parser.output[parser.token_start_idx] != '\0' || parser.token_has_content) {
        parser.tokens[parser.position] = &parser.output[parser.token_start_idx];
        parser.position++;
    }

    parser.tokens[parser.position] = NULL;

    ParseResult result = {
        .buffer = parser.output,
        .tokens = parser.tokens
    };
    return result;
}

void parse_result_free(ParseResult *result) {
    if (result) {
        free(result->buffer);
        free(result->tokens);
        result->buffer = NULL;
        result->tokens = NULL;
    }
}
