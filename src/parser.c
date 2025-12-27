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
    const char *read_pos;
    char *write_pos;
    int in_single_quote = 0;
    int in_double_quote = 0;
    size_t token_start_idx = 0;

    if (!tokens) {
        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
        exit(EXIT_FAILURE);
    }

    read_pos = line;
    write_pos = line;

    // Skip leading whitespace
    while (*read_pos && isspace(*read_pos)) {
        read_pos++;
        write_pos++;
    }

    token_start_idx = (size_t)(write_pos - line);

    while (*read_pos) {
        if (*read_pos == '\\' && *(read_pos + 1)) {
            // Escape sequence
            read_pos++; // Skip the backslash

            if (in_single_quote) {
                // In single quotes, only \' is special
                if (*read_pos == '\'') {
                    *write_pos++ = '\'';
                    read_pos++;
                } else {
                    // Keep backslash literal
                    *write_pos++ = '\\';
                    *write_pos++ = *read_pos++;
                }
            } else {
                // In double quotes or unquoted, handle common escapes
                switch (*read_pos) {
                    case '"':
                    case '\\':
                    case '\'':
                        *write_pos++ = *read_pos++;
                        break;
                    case 'n':
                        *write_pos++ = '\n';
                        read_pos++;
                        break;
                    case 't':
                        *write_pos++ = '\t';
                        read_pos++;
                        break;
                    case 'r':
                        *write_pos++ = '\r';
                        read_pos++;
                        break;
                    default:
                        // Unknown escape, keep both characters
                        *write_pos++ = '\\';
                        *write_pos++ = *read_pos++;
                        break;
                }
            }
        } else if (*read_pos == '\'' && !in_double_quote) {
            // Toggle single quote mode
            in_single_quote = !in_single_quote;
            read_pos++;
        } else if (*read_pos == '"' && !in_single_quote) {
            // Toggle double quote mode
            in_double_quote = !in_double_quote;
            read_pos++;
        } else if (isspace(*read_pos) && !in_single_quote && !in_double_quote) {
            // End of token
            *write_pos = '\0';

            // Add token to array if it's not empty
            if (line[token_start_idx] != '\0') {
                tokens[position] = &line[token_start_idx];
                position++;

                if (position >= bufsize) {
                    bufsize += MAX_ARGS;
                    tokens = realloc(tokens, bufsize * sizeof(char*));
                    if (!tokens) {
                        fprintf(stderr, "%s: allocation error\n", HASH_NAME);
                        exit(EXIT_FAILURE);
                    }
                }
            }

            // Skip whitespace
            write_pos++;
            read_pos++;
            while (*read_pos && isspace(*read_pos)) {
                read_pos++;
                write_pos++;
            }

            // Start of next token
            token_start_idx = (size_t)(write_pos - line);
        } else {
            // Regular character
            *write_pos++ = *read_pos++;
        }
    }

    // Handle last token
    *write_pos = '\0';
    if (line[token_start_idx] != '\0') {
        tokens[position] = &line[token_start_idx];
        position++;
    }

    tokens[position] = NULL;
    return tokens;
}
