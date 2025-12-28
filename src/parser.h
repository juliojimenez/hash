#ifndef PARSER_H
#define PARSER_H

// Read a line from stdin with editing support
char *read_line(const char *prompt);

// Parse line into tokens
char **parse_line(char *line);

#endif
