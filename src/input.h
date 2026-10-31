#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>
#include <sys/types.h>

/**
 * Line input from a file descriptor that never consumes more than the line
 * it returns. The shell reads the commands on its standard input with these
 * functions, and so does the `read` builtin, so a command run from a piped
 * script can read the input that follows it, and a redirection of descriptor
 * 0 leaves nothing buffered behind for the next reader. A regular file is
 * read in chunks and repositioned just after the newline; a pipe, terminal
 * or other stream is read one byte at a time.
 */

/**
 * Read one line, newline included, into buf (at most cap-1 bytes plus the
 * terminator). A longer line comes back in pieces, as fgets() returns it.
 * Returns the number of bytes stored, 0 at end of input, -1 on error.
 */
ssize_t input_read_line(int fd, char *buf, size_t cap);

/**
 * Read one line of any length, newline included, into a malloc'd buffer.
 * Returns NULL at end of input (errno 0) or on error (errno set); *len
 * receives the length when a line is returned and may be NULL.
 */
char *input_read_line_alloc(int fd, size_t *len);

#endif
