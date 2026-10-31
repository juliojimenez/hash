#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "input.h"

// A regular file can be read ahead and repositioned; anything else (pipe,
// terminal, socket, device) is read one byte at a time so that nothing past
// the newline is taken from the next reader
static bool can_reposition(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
           lseek(fd, 0, SEEK_CUR) != (off_t)-1;
}

// read(2) that retries after a signal
static ssize_t read_retry(int fd, void *buf, size_t n) {
    for (;;) {
        ssize_t r = read(fd, buf, n);
        if (r >= 0 || errno != EINTR) {
            return r;
        }
    }
}

ssize_t input_read_line(int fd, char *buf, size_t cap) {
    if (cap < 2) {
        errno = EINVAL;
        return -1;
    }
    bool chunked = can_reposition(fd);
    size_t n = 0;
    while (n + 1 < cap) {
        size_t want = chunked ? cap - 1 - n : 1;
        ssize_t r = read_retry(fd, buf + n, want);
        if (r < 0) {
            if (n > 0) {
                break;  // hand back what was read; the next call reports the error
            }
            buf[0] = '\0';
            return -1;
        }
        if (r == 0) {
            break;  // end of input
        }
        char *nl = memchr(buf + n, '\n', (size_t)r);
        if (nl) {
            size_t keep = (size_t)(nl - (buf + n)) + 1;
            off_t extra = (off_t)((size_t)r - keep);
            if (extra > 0) {
                // Only a chunked read can have gone past the newline
                lseek(fd, -extra, SEEK_CUR);
            }
            n += keep;
            break;
        }
        n += (size_t)r;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

char *input_read_line_alloc(int fd, size_t *len) {
    size_t cap = 256;
    size_t n = 0;
    char *line = malloc(cap);
    if (!line) {
        return NULL;
    }
    for (;;) {
        ssize_t got = input_read_line(fd, line + n, cap - n);
        if (got < 0) {
            int saved = errno;
            free(line);
            errno = saved;
            return NULL;
        }
        if (got == 0) {
            break;  // end of input, possibly after a last line without newline
        }
        n += (size_t)got;
        if (line[n - 1] == '\n') {
            break;
        }
        if (n + 1 >= cap) {
            cap *= 2;
            char *grown = realloc(line, cap);
            if (!grown) {
                free(line);
                errno = ENOMEM;
                return NULL;
            }
            line = grown;
        }
    }
    if (n == 0) {
        free(line);
        errno = 0;
        return NULL;
    }
    if (len) {
        *len = n;
    }
    return line;
}
