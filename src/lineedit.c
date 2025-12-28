#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include "lineedit.h"
#include "hash.h"
#include "safe_string.h"

#define MAX_LINE_LENGTH 4096

// Terminal state
static struct termios orig_termios;
static int raw_mode_enabled = 0;

// Special key codes
#define KEY_NULL       0
#define KEY_CTRL_A     1
#define KEY_CTRL_B     2
#define KEY_CTRL_C     3
#define KEY_CTRL_D     4
#define KEY_CTRL_E     5
#define KEY_CTRL_F     6
#define KEY_CTRL_H     8
#define KEY_TAB        9
#define KEY_CTRL_K     11
#define KEY_CTRL_L     12
#define KEY_ENTER      13
#define KEY_CTRL_U     21
#define KEY_CTRL_W     23
#define KEY_ESC        27
#define KEY_BACKSPACE  127

// Enable raw mode
static int enable_raw_mode(void) {
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return -1;
    }

    struct termios raw = orig_termios;

    // Disable canonical mode, echo, and signals
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // Disable input processing
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    // Disable output processing
    raw.c_oflag &= ~OPOST;

    // Set character size to 8 bits
    raw.c_cflag |= CS8;

    // Read returns with 1 byte or timeout
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    raw_mode_enabled = 1;
    return 0;
}

// Disable raw mode
static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

// Read a key from stdin
static int read_key(void) {
    char c;
    int nread;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1) return -1;
    }

    // Handle escape sequences
    if (c == KEY_ESC) {
        char seq[3];

        // Read next two bytes
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                // Extended escape sequence
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return KEY_ESC;

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_CTRL_A;  // Home
                        case '3': return KEY_BACKSPACE;  // Delete
                        case '4': return KEY_CTRL_E;  // End
                    }
                }
            } else {
                // Arrow keys and other special keys
                switch (seq[1]) {
                    case 'A': return 'A' + 256;  // Up arrow
                    case 'B': return 'B' + 256;  // Down arrow
                    case 'C': return 'C' + 256;  // Right arrow
                    case 'D': return 'D' + 256;  // Left arrow
                    case 'H': return KEY_CTRL_A;  // Home
                    case 'F': return KEY_CTRL_E;  // End
                }
            }
        } else if (seq[0] == 'O') {
            // Alternative encoding for Home/End
            switch (seq[1]) {
                case 'H': return KEY_CTRL_A;  // Home
                case 'F': return KEY_CTRL_E;  // End
            }
        }

        return KEY_ESC;
    }

    return c;
}

// Refresh the line on screen
static void refresh_line(const char *buf, size_t len, size_t pos, const char *prompt) {
    ssize_t ret;
    // Move cursor to beginning of line
    ret = write(STDOUT_FILENO, "\r", 1);
    (void)ret;

    // Clear line
    ret = write(STDOUT_FILENO, "\x1b[K", 3);
    (void)ret;

    // Write prompt and current buffer
    ret = write(STDOUT_FILENO, prompt, strlen(prompt));
    (void)ret;
    ret = write(STDOUT_FILENO, buf, len);
    (void)ret;

    // Move cursor to correct position
    size_t cursor_col = strlen(prompt) + pos;
    char cursor_seq[32];
    snprintf(cursor_seq, sizeof(cursor_seq), "\r\x1b[%zuC", cursor_col);
    ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
    (void)ret;
}

// Initialize line editor
void lineedit_init(void) {
    // Nothing to do here yet
}

// Cleanup line editor
void lineedit_cleanup(void) {
    disable_raw_mode();
}

// Read a line with editing capabilities
char *lineedit_read_line(const char *prompt) {
    static char buf[MAX_LINE_LENGTH];
    size_t len = 0;      // Length of current line
    size_t pos = 0;      // Cursor position
    ssize_t ret;

    // Display initial prompt
    if (prompt) {
        ret = write(STDOUT_FILENO, prompt, strlen(prompt));
        (void)ret;
    }

    // Enable raw mode
    if (enable_raw_mode() == -1) {
        // Fallback to simple getline if raw mode fails
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

    // Clear buffer
    memset(buf, 0, sizeof(buf));

    const char *prompt_str = prompt ? prompt : "";

    while (1) {
        int c = read_key();

        if (c == -1) break;

        switch (c) {
            case KEY_ENTER:
                // Submit line
                disable_raw_mode();
                ret = write(STDOUT_FILENO, "\n", 1);
                (void)ret;

                char *result = malloc(len + 1);
                if (result) {
                    memcpy(result, buf, len);
                    result[len] = '\0';
                }
                return result;

            case KEY_CTRL_D:
                // EOF - exit if line is empty
                if (len == 0) {
                    disable_raw_mode();
                    return NULL;
                }
                break;

            case KEY_CTRL_C:
                // Cancel line
                len = 0;
                pos = 0;
                ret = write(STDOUT_FILENO, "^C\n", 3);
                (void)ret;
                disable_raw_mode();

                char *empty = malloc(1);
                if (empty) empty[0] = '\0';
                return empty;

            case KEY_BACKSPACE:
            case KEY_CTRL_H:
                // Delete character before cursor
                if (pos > 0) {
                    memmove(buf + pos - 1, buf + pos, len - pos);
                    pos--;
                    len--;
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str);
                }
                break;

            case 'C' + 256:  // Right arrow
                if (pos < len) {
                    pos++;
                    ret = write(STDOUT_FILENO, "\x1b[C", 3);
                    (void)ret;
                }
                break;

            case 'D' + 256:  // Left arrow
                if (pos > 0) {
                    pos--;
                    ret = write(STDOUT_FILENO, "\x1b[D", 3);
                    (void)ret;
                }
                break;

            case KEY_CTRL_A:  // Beginning of line
                while (pos > 0) {
                    pos--;
                    ret = write(STDOUT_FILENO, "\x1b[D", 3);
                    (void)ret;
                }
                break;

            case KEY_CTRL_E:  // End of line
                while (pos < len) {
                    pos++;
                    ret = write(STDOUT_FILENO, "\x1b[C", 3);
                    (void)ret;
                }
                break;

            case KEY_CTRL_U:  // Delete from cursor to beginning
                if (pos > 0) {
                    memmove(buf, buf + pos, len - pos);
                    len -= pos;
                    pos = 0;
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str);
                }
                break;

            case KEY_CTRL_K:  // Delete from cursor to end
                len = pos;
                buf[len] = '\0';
                refresh_line(buf, len, pos, prompt_str);
                break;

            case KEY_CTRL_W:  // Delete word backward
                if (pos > 0) {
                    size_t old_pos = pos;

                    // Skip trailing whitespace
                    while (pos > 0 && isspace(buf[pos - 1])) {
                        pos--;
                    }

                    // Delete word
                    while (pos > 0 && !isspace(buf[pos - 1])) {
                        pos--;
                    }

                    // Remove deleted chars
                    memmove(buf + pos, buf + old_pos, len - old_pos);
                    len -= (old_pos - pos);
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str);
                }
                break;

            case KEY_CTRL_L:  // Clear screen
                ret = write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);
                (void)ret;
                refresh_line(buf, len, pos, prompt_str);
                break;

            case KEY_TAB:
                // Insert 4 spaces instead of tab
                if (len < MAX_LINE_LENGTH - 1) {
                    const char *spaces = "    ";
                    size_t spaces_len = 4;

                    if (len + spaces_len < MAX_LINE_LENGTH) {
                        memmove(buf + pos + spaces_len, buf + pos, len - pos);
                        memcpy(buf + pos, spaces, spaces_len);
                        pos += spaces_len;
                        len += spaces_len;
                        buf[len] = '\0';
                        refresh_line(buf, len, pos, prompt_str);
                    }
                }
                break;

            default:
                // Regular character
                if (c >= 32 && c < 127 && len < MAX_LINE_LENGTH - 1) {
                    // Insert character at cursor position
                    memmove(buf + pos + 1, buf + pos, len - pos);
                    buf[pos] = c;
                    pos++;
                    len++;
                    buf[len] = '\0';

                    // Redraw line if we're not at the end
                    if (pos < len) {
                        refresh_line(buf, len, pos, prompt_str);
                    } else {
                        // Just write the character (optimization)
                        ret = write(STDOUT_FILENO, &c, 1);
                        (void)ret;
                    }
                }
                break;
        }
    }

    disable_raw_mode();
    return NULL;
}
