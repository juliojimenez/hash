#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "parser.h"

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char **parse_line(char *line) {
    int bufsize = MAX_ARGS;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token_start;
    char *current;
    char *write_pos;
    int in_single_quote = 0;
    int in_double_quote = 0;

    if (!tokens) {
        fprintf(stderr, "%s: allocation error\n", SHELL_NAME);
        exit(EXIT_FAILURE);
    }

    current = line;

    // Skip leading whitespace
    while (*current && isspace(*current)) {
        current++;
    }

    while (*current) {
        // Start of a new token
        token_start = current;
        write_pos = current;

        // Process characters until we hit the end of the token
        while (*current) {
            if (*current == '\\' && *(current + 1)) {
                // Escape sequence
                current++; // Skip the backslash

                if (in_single_quote) {
                    // In single quotes, only \' is special
                    if (*current == '\'') {
                        *write_pos++ = '\'';
                        current++;
                    } else {
                        // Keep backslash literal
                        *write_pos++ = '\\';
                        *write_pos++ = *current++;
                    }
                } else {
                    // In double quotes or unquoted, handle common escapes
                    switch (*current) {
                        case '"':
                        case '\\':
                        case '\'':
                            *write_pos++ = *current++;
                            break;
                        case 'n':
                            *write_pos++ = '\n';
                            current++;
                            break;
                        case 't':
                            *write_pos++ = '\t';
                            current++;
                            break;
                        case 'r':
                            *write_pos++ = '\r';
                            current++;
                            break;
                        default:
                            // Unknown escape, keep both characters
                            *write_pos++ = '\\';
                            *write_pos++ = *current++;
                            break;
                    }
                }
                continue;
            } else if (*current == '\'' && !in_double_quote) {
                // Toggle single quote mode
                in_single_quote = !in_single_quote;
                current++;
                continue;
            } else if (*current == '"' && !in_single_quote) {
                // Toggle double quote mode
                in_double_quote = !in_double_quote;
                current++;
                continue;
            } else if (isspace(*current) && !in_single_quote && !in_double_quote) {
                // End of token
                break;
            }

            // Regular character
            *write_pos++ = *current++;
        }

        // Null-terminate the token
        *write_pos = '\0';

        // Move past whitespace
        if (*current) {
            current++;
        }

        // Add token to array if it's not empty
        if (*token_start) {
            tokens[position] = token_start;
            position++;

            if (position >= bufsize) {
                bufsize += MAX_ARGS;
                tokens = realloc(tokens, bufsize * sizeof(char*));
                if (!tokens) {
                    fprintf(stderr, "%s: allocation error\n", SHELL_NAME);
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Skip whitespace before next token
        while (*current && isspace(*current)) {
            current++;
        }
    }

    tokens[position] = NULL;
    return tokens;
}
