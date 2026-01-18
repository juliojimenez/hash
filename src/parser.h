#ifndef PARSER_H
#define PARSER_H

// Result of parsing a line - contains both tokens and the underlying buffer
typedef struct {
    char *buffer;    // The buffer containing parsed token data
    char **tokens;   // NULL-terminated array of pointers into buffer
} ParseResult;

// Read a line from stdin with editing support
char *read_line(const char *prompt);

// Parse line into tokens
// Returns a ParseResult containing tokens and the underlying buffer.
// Caller must call parse_result_free() when done with the result.
ParseResult parse_line(const char *line);

// Free a ParseResult and all associated memory
void parse_result_free(ParseResult *result);

#endif
