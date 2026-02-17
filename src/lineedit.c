#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <wchar.h>
#include <locale.h>
#include "lineedit.h"
#include "hash.h"
#include "safe_string.h"
#include "history.h"
#include "completion.h"
#include "syntax.h"
#include "color_config.h"
#include "colors.h"
#include "autosuggest.h"

#define MAX_LINE_LENGTH 4096

// Get basename from a path (pointer to last component)
static const char *get_display_name(const char *path) {
    if (!path) return NULL;

    // Find the last slash (but not trailing slash)
    size_t len = strlen(path);
    const char *last_slash = NULL;

    // Skip trailing slash for directories
    size_t check_len = (len > 0 && path[len - 1] == '/') ? len - 1 : len;

    for (size_t i = 0; i < check_len; i++) {
        if (path[i] == '/') {
            last_slash = &path[i];
        }
    }

    if (last_slash && *(last_slash + 1) != '\0') {
        return last_slash + 1;
    }

    return path;
}

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
#define KEY_CTRL_G     7   // Cancel search
#define KEY_CTRL_R     18  // Reverse search
#define KEY_CTRL_S     19  // Forward search

// Reverse-i-search state
typedef struct {
    int active;                        // Search mode active flag
    char query[256];                   // Search query
    size_t query_len;                  // Query length
    int match_index;                   // Current match index (-1 = no match)
    int direction;                     // 1=reverse, -1=forward
    char saved_buf[MAX_LINE_LENGTH];   // Original buffer
    size_t saved_len;                  // Original length
    size_t saved_pos;                  // Original cursor position
} SearchState;

static SearchState search_state = {0};

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
    ssize_t nread;

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
                        default: break;
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
                    default: break;
                }
            }
        } else if (seq[0] == 'O') {
            // Alternative encoding for Home/End
            switch (seq[1]) {
                case 'H': return KEY_CTRL_A;  // Home
                case 'F': return KEY_CTRL_E;  // End
                default: break;
            }
        }

        return KEY_ESC;
    }

    return c;
}

// Get terminal width
static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 80;  // Default fallback
    }
    return ws.ws_col;
}

// Decode a UTF-8 character and return its byte length (1-4), or 0 on error
// Also returns the decoded codepoint via the wc parameter
static int utf8_decode(const unsigned char *p, wchar_t *wc) {
    if (!p || !*p) return 0;

    if (*p < 0x80) {
        // ASCII
        *wc = *p;
        return 1;
    } else if ((*p & 0xE0) == 0xC0) {
        // 2-byte sequence
        if ((p[1] & 0xC0) != 0x80) return 0;
        *wc = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    } else if ((*p & 0xF0) == 0xE0) {
        // 3-byte sequence
        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0;
        *wc = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return 3;
    } else if ((*p & 0xF8) == 0xF0) {
        // 4-byte sequence
        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return 0;
        *wc = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
    return 0;
}

// Calculate visible length of prompt's LAST LINE (excluding ANSI escape sequences)
// For multi-line prompts, only the last line affects cursor positioning
// Uses wcwidth() to properly handle wide characters
static size_t visible_prompt_length(const char *prompt) {
    if (!prompt) return 0;

    // Find the last newline - we only care about the last line
    const char *last_newline = strrchr(prompt, '\n');
    const char *start = last_newline ? last_newline + 1 : prompt;

    size_t visible = 0;
    const unsigned char *p = (const unsigned char *)start;
    int in_escape = 0;

    while (*p) {
        if (*p == '\x1b') {
            // Start of ANSI escape sequence
            in_escape = 1;
            p++;
            // Skip CSI introducer '[' if present
            if (*p == '[') {
                p++;
            }
        } else if (in_escape) {
            // Inside escape sequence - skip until we hit the final byte
            // CSI sequences end with a byte in range 0x40-0x7E (@ through ~)
            if (*p >= 0x40 && *p <= 0x7E) {
                in_escape = 0;
            }
            p++;
        } else if (*p >= 0x80) {
            // UTF-8 multi-byte character - use wcwidth for display width
            wchar_t wc;
            int len = utf8_decode(p, &wc);
            if (len > 0) {
                int width = wcwidth(wc);
                // wcwidth returns -1 for non-printable/unknown chars
                // Default to width 1 for unknown printable chars
                if (width < 0) width = 1;
                visible += (size_t)width;
                p += len;
            } else {
                // Invalid UTF-8, skip byte
                p++;
            }
        } else {
            // Regular ASCII character
            visible++;
            p++;
        }
    }

    return visible;
}

// Count the number of newlines in a string (to determine prompt line count)
static int count_newlines(const char *str) {
    if (!str) return 0;

    int count = 0;
    while (*str) {
        if (*str == '\n') count++;
        str++;
    }
    return count;
}

// Write string to stdout, converting \n to \r\n for raw mode
// In raw mode, \n only moves down without returning to column 0
static void write_with_crlf(const char *str) {
    if (!str) return;

    ssize_t ret;
    const char *p = str;
    const char *start = str;

    while (*p) {
        if (*p == '\n') {
            // Write everything before the newline
            if (p > start) {
                ret = write(STDOUT_FILENO, start, (size_t)(p - start));
                (void)ret;
            }
            // Write \r\n instead of just \n
            ret = write(STDOUT_FILENO, "\r\n", 2);
            (void)ret;
            start = p + 1;
        }
        p++;
    }

    // Write any remaining content after the last newline
    if (p > start) {
        ret = write(STDOUT_FILENO, start, (size_t)(p - start));
        (void)ret;
    }
}

// Set the cursor to given position.
static void set_cursor(const char *buf, size_t pos, size_t prev_pos,
        const char *prompt) {
    size_t ret;

    // Only reposition cursor if it's not at the desired position
    if (prev_pos == pos) return;

    size_t count = 0;
    if (prev_pos > pos) {
        // Calculate how many lines to go up
        for (ssize_t i = prev_pos; i >= (ssize_t)pos; --i) {
            if (buf[i] == '\n') ++count;
        }
    } else {
        // calculate how many lines to go down
        for (size_t i = prev_pos; i <= pos; ++i) {
            if (buf[i] == '\n') ++count;
        }
    }

    char cursor_seq[32];

    if (count > 0) {
        // Move cursor that many lines up
        snprintf(cursor_seq, sizeof(cursor_seq),
                (prev_pos > pos) ? "\x1b[%zuA" : "\x1b[%zuB", count);
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    }

    size_t begin = 0;
    if (pos > 0) {
        // Move cursor to correct position on that line
        // Count number of cols to move to right
        for (size_t i = pos - 1; i > 0; --i) {
            if (buf[i] == '\n') {
                begin = i + 1;
                break;
            }
        }
    }

    // Move cursor to the begining of line
    ret = write(STDOUT_FILENO, "\r", 1);
    (void)ret;

    if (begin == 0) {
        // Skip the prompt
        size_t visible_prompt = visible_prompt_length(prompt);
        snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%zuC", visible_prompt);
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    }

    if (pos - begin > 0) {
        // Go to pos
        snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%zuC", (pos - begin));
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    }
}

// Refresh the line on screen (supports multi-line prompt and buffer)
static void refresh_line(const char *buf, size_t len, size_t pos, const char *prompt,
        size_t prev_buffer_lines) {
    ssize_t ret;

    // Count how many lines up the cursor is
    size_t count = 0;
    for (ssize_t i = len - 1; i >= (ssize_t)pos; --i) {
        if (buf[i] == '\n') ++count;
    }

    // Count new lines in prompt and previous buffer
    size_t prompt_lines = count_newlines(prompt) + prev_buffer_lines - count;

    // For multi-line prompt and buffer, move cursor up to where the prompt started
    if (prompt_lines > 0) {
        char up_seq[32];
        snprintf(up_seq, sizeof(up_seq), "\x1b[%zuA", prompt_lines);
        ret = write(STDOUT_FILENO, up_seq, strlen(up_seq));
        (void)ret;
    }

    // Move cursor to beginning of line
    ret = write(STDOUT_FILENO, "\r", 1);
    (void)ret;

    // Clear from cursor to end of screen (handles multi-line prompts)
    ret = write(STDOUT_FILENO, "\x1b[J", 3);
    (void)ret;

    // Write prompt and current buffer (with proper newline handling)
    write_with_crlf(prompt);

    // Apply syntax highlighting if enabled
    if (colors_enabled && color_config.syntax_highlight_enabled && len > 0) {
        char *highlighted = syntax_render(buf, len);
        if (highlighted) {
            write_with_crlf(highlighted);
            free(highlighted);
        } else {
            write_with_crlf(buf);
        }
    } else {
        write_with_crlf(buf);
    }

    size_t previous_pos = len;

    // Show autosuggestion if enabled and cursor is at end of buffer
    if (colors_enabled && color_config.autosuggestion_enabled && pos == len && len > 0) {
        const char *suggestion = autosuggest_get(buf, len);
        if (suggestion && suggestion[0]) {
            // Write suggestion in dim color
            const char *suggest_color = color_config_get(color_config.suggestion);
            ret = write(STDOUT_FILENO, suggest_color, strlen(suggest_color));
            previous_pos += ret;
            ret = write(STDOUT_FILENO, suggestion, strlen(suggestion));
            previous_pos += ret;
            // Reset color
            const char *reset = color_code(COLOR_RESET);
            ret = write(STDOUT_FILENO, reset, strlen(reset));
            previous_pos += ret;

            // Move the cursor up if there was a new line
            for (size_t i = 0; suggestion[i]; ++i) {
                if (suggestion[i] == '\n') {
                    ret = write(STDOUT_FILENO, "\x1b[A", 3);
                    (void)ret;
                }
            }
        }
    }


    set_cursor(buf, pos, previous_pos, prompt);
}

// Initialize line editor
void lineedit_init(void) {
    // Set locale for proper wcwidth() behavior with Unicode
    setlocale(LC_CTYPE, "");
}

// Cleanup line editor
void lineedit_cleanup(void) {
    disable_raw_mode();
}

bool inside_quote(const char *buf, size_t len) {
    char single_quote = 0;
    char double_quote = 0;
    // Loop through buffer to check whether we are inside the quote
    // If we are already inside one type of quote, then other type of quote is ignored.
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] == '\'' && !double_quote) single_quote ^= 1;
        else if (buf[i] == '"' && !single_quote) double_quote ^= 1;
    }
    return single_quote || double_quote;
}

// Initialize reverse-i-search state
static void search_init(const char *buf, size_t len, size_t pos) {
    search_state.active = 1;
    search_state.query[0] = '\0';
    search_state.query_len = 0;
    search_state.match_index = -1;
    search_state.direction = 1;  // Default to reverse

    // Save current buffer state
    memcpy(search_state.saved_buf, buf, len + 1);
    search_state.saved_len = len;
    search_state.saved_pos = pos;
}

// Reset search state
static void search_cleanup(void) {
    search_state.active = 0;
    search_state.query[0] = '\0';
    search_state.query_len = 0;
    search_state.match_index = -1;
}

// Refresh line with search prompt
static void search_refresh_line(const char *buf, size_t len, size_t pos,
                                 size_t prev_buffer_lines, int has_match) {
    // Build search prompt
    char search_prompt[512];
    const char *mode = (search_state.direction == 1) ? "reverse" : "forward";
    const char *status = has_match ? "" : "failing ";

    snprintf(search_prompt, sizeof(search_prompt),
             "(%s%s-i-search)`%s': ", status, mode, search_state.query);

    refresh_line(buf, len, pos, search_prompt, prev_buffer_lines);
}

// Perform search and update display
static void search_update(char *buf, size_t *len, size_t *pos,
                          size_t newline_count) {
    int result_idx = -1;
    const char *match = NULL;

    if (search_state.query_len > 0) {
        // Determine starting point for search
        int start;
        if (search_state.match_index >= 0) {
            // Continue from current match position
            start = search_state.match_index;
        } else {
            // Start from end for reverse, beginning for forward
            start = (search_state.direction == 1) ? history_count() - 1 : 0;
        }

        match = history_search_substring(search_state.query, start,
                                          search_state.direction, &result_idx);
    }

    // Update buffer with match
    if (match) {
        safe_strcpy(buf, match, MAX_LINE_LENGTH);
        *len = safe_strlen(buf, MAX_LINE_LENGTH);
        *pos = *len;
        search_state.match_index = result_idx;
    } else if (search_state.query_len == 0) {
        // Empty query - show empty buffer
        buf[0] = '\0';
        *len = 0;
        *pos = 0;
        search_state.match_index = -1;
    }

    search_refresh_line(buf, *len, *pos, newline_count, match != NULL || search_state.query_len == 0);
}

// Read a line with editing capabilities
char *lineedit_read_line(const char *prompt) {
    static char buf[MAX_LINE_LENGTH];
    static int last_was_tab = 0;
    size_t len = 0;
    size_t pos = 0;
    ssize_t ret;

    // Clear buffer and state
    memset(buf, 0, sizeof(buf));
    len = 0;
    pos = 0;

    // Enable raw mode first
    if (enable_raw_mode() == -1) {
        // Fallback to simple getline
        char *line = NULL;
        size_t bufsize = 0;

        if (prompt) {
            // POSIX: interactive shell prompts go to stderr
            fprintf(stderr, "%s", prompt);
            fflush(stderr);
        }

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

    // Display prompt after entering raw mode
    // Use write_with_crlf to handle newlines properly in raw mode
    if (prompt) {
        write_with_crlf(prompt);
        fflush(stdout);
    }

    const char *prompt_str = prompt ? prompt : "";
    size_t newline_count = 0;

    while (1) {
        int c = read_key();

        if (c == -1) break;

        switch (c) {
            case KEY_ENTER:
                // Exit search mode if active, keep the current buffer
                if (search_state.active) {
                    search_cleanup();
                }

                // Check for new line escape and quotes
                if (len > 0 && (buf[len - 1] == '\\' || inside_quote(buf, len))) {
                    last_was_tab = 0;
                    if (len < MAX_LINE_LENGTH - 1) {
                        buf[len] = '\n';
                        len++;
                        pos = len;
                        buf[len] = '\0';
                        newline_count++;

                        // Write crlf to termial
                        ret = write(STDOUT_FILENO, "\r\n", 2);
                        (void)ret;
                    }
                    break;
                }

                // Refresh line and move the cursor to end of line
                // Hack to disable auto suggestion, move cursor one position back
                if (len > 0) {
                    set_cursor(buf, pos - 1, pos, prompt_str);
                    pos--;
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    set_cursor(buf, len, pos, prompt_str);
                }

                disable_raw_mode();

                // Write newline and flush
                ret = write(STDOUT_FILENO, "\r\n", 2);
                (void)ret;
                fflush(stdout);

                char *result = malloc(len + 1);
                if (result) {
                    memcpy(result, buf, len);
                    result[len] = '\0';
                }
                return result;

            case KEY_CTRL_D:
                if (len == 0) {
                    disable_raw_mode();
                    return NULL;
                }
                break;

            case KEY_CTRL_C:
                if (search_state.active) {
                    // Cancel search and restore original buffer
                    memcpy(buf, search_state.saved_buf, search_state.saved_len + 1);
                    len = search_state.saved_len;
                    pos = search_state.saved_pos;
                    search_cleanup();
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    break;
                }

                disable_raw_mode();

                // Move to beginning and write ^C on a fresh line
                ret = write(STDOUT_FILENO, "\r\x1b[K^C\n", 8);
                (void)ret;

                char *empty = malloc(1);
                if (empty) empty[0] = '\0';
                return empty;

            case KEY_BACKSPACE:
            case KEY_CTRL_H:
                if (search_state.active) {
                    // Delete last character from search query
                    if (search_state.query_len > 0) {
                        search_state.query_len--;
                        search_state.query[search_state.query_len] = '\0';
                        search_state.match_index = -1;  // Reset to search from end
                        search_update(buf, &len, &pos, newline_count);
                    }
                    break;
                }
                if (pos > 0) {
                    char removed_char = buf[pos - 1];
                    memmove(buf + pos - 1, buf + pos, len - pos);
                    pos--;
                    len--;
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    if (removed_char == '\n') newline_count--;
                }
                break;

            case 'C' + 256:  // Right arrow
                if (search_state.active) {
                    // Exit search mode, keep command for editing
                    search_cleanup();
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    break;
                }
                if (pos < len) {
                    pos++;
                    // If in the end of current line in multi line buffer, move to next line
                    if (buf[pos - 1] == '\n') {
                        ret = write(STDOUT_FILENO, "\x1b[E", 3);
                        (void)ret;
                        break;
                    }
                    ret = write(STDOUT_FILENO, "\x1b[C", 3);
                    (void)ret;
                } else if (pos == len && colors_enabled && color_config.autosuggestion_enabled) {
                    // At end of buffer - accept autosuggestion if available
                    const char *suggestion = autosuggest_get(buf, len);
                    if (suggestion && suggestion[0]) {
                        size_t sug_len = strlen(suggestion);
                        if (len + sug_len < MAX_LINE_LENGTH - 1) {
                            // Append suggestion to buffer
                            memcpy(buf + len, suggestion, sug_len);
                            len += sug_len;
                            pos = len;
                            buf[len] = '\0';
                            refresh_line(buf, len, pos, prompt_str, newline_count);
                            for (size_t i = 0; i < sug_len; ++i) {
                                if (suggestion[i] == '\n') {
                                    newline_count++;
                                }
                            }
                            // Invalidate cache since buffer changed
                            autosuggest_invalidate();
                        }
                    }
                }
                break;

            case 'D' + 256:  // Left arrow
                if (search_state.active) {
                    // Exit search mode, keep command for editing
                    search_cleanup();
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    break;
                }
                if (pos > 0) {
                    pos--;
                    // If n the begining of current line in multi line buffer, move to previous line
                    if (buf[pos] == '\n') {
                        set_cursor(buf, pos, pos + 1, prompt);
                        break;
                    }
                    ret = write(STDOUT_FILENO, "\x1b[D", 3);
                    (void)ret;
                }
                break;

            case 'A' + 256:  // Up arrow - previous history
                {
                    const char *prev = history_prev();
                    if (prev) {
                        // Clear current line and load history entry
                        safe_strcpy(buf, prev, sizeof(buf));
                        len = safe_strlen(buf, sizeof(buf));
                        pos = len;
                        refresh_line(buf, len, pos, prompt_str, newline_count);
                        // update the new line count to match current buffer
                        newline_count = count_newlines(buf);
                    }
                }
                break;

            case 'B' + 256:  // Down arrow - next history
                {
                    const char *next = history_next();
                    if (next) {
                        // Load next history entry
                        safe_strcpy(buf, next, sizeof(buf));
                        len = safe_strlen(buf, sizeof(buf));
                        pos = len;
                        refresh_line(buf, len, pos, prompt_str, newline_count);
                        // update the new line count to match current buffer
                        newline_count = count_newlines(buf);
                    } else {
                        // At end of history, clear line
                        len = 0;
                        pos = 0;
                        buf[0] = '\0';
                        refresh_line(buf, len, pos, prompt_str, newline_count);
                        // update the new line count
                        newline_count = 0;
                    }
                }
                break;

            case KEY_CTRL_A:  // Beginning
                last_was_tab = 0;
                while (pos > 0 && buf[pos - 1] != '\n') {
                    pos--;
                    ret = write(STDOUT_FILENO, "\x1b[D", 3);
                    (void)ret;
                }
                break;

            case KEY_CTRL_E:  // End
                last_was_tab = 0;
                while (pos < len && buf[pos + 1] != '\n') {
                    pos++;
                    ret = write(STDOUT_FILENO, "\x1b[C", 3);
                    (void)ret;
                }
                break;

            case KEY_CTRL_U:  // Delete to beginning
                last_was_tab = 0;
                if (pos > 0) {
                    memmove(buf, buf + pos, len - pos);
                    len -= pos;
                    pos = 0;
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    newline_count = count_newlines(buf);
                }
                break;

            case KEY_CTRL_K:  // Delete to end
                last_was_tab = 0;
                len = pos;
                buf[len] = '\0';
                refresh_line(buf, len, pos, prompt_str, newline_count);
                newline_count = count_newlines(buf);
                break;

            case KEY_CTRL_W:  // Delete word backward
                last_was_tab = 0;
                if (pos > 0) {
                    size_t old_pos = pos;

                    while (pos > 0 && isspace(buf[pos - 1])) {
                        pos--;
                    }

                    while (pos > 0 && !isspace(buf[pos - 1])) {
                        pos--;
                    }

                    memmove(buf + pos, buf + old_pos, len - old_pos);
                    len -= (old_pos - pos);
                    buf[len] = '\0';
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                    newline_count = count_newlines(buf);
                }
                break;

            case KEY_CTRL_L:  // Clear screen
                last_was_tab = 0;
                ret = write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);
                (void)ret;
                if (search_state.active) {
                    search_refresh_line(buf, len, pos, newline_count, search_state.match_index >= 0 || search_state.query_len == 0);
                } else {
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                }
                newline_count = count_newlines(buf);
                break;

            case KEY_CTRL_R:  // Reverse incremental search
                last_was_tab = 0;
                if (!search_state.active) {
                    // Enter search mode
                    search_init(buf, len, pos);
                    search_refresh_line(buf, len, pos, newline_count, 1);
                } else {
                    // Cycle to next older match
                    search_state.direction = 1;
                    if (search_state.match_index > 0) {
                        search_state.match_index--;
                    } else if (search_state.match_index < 0 && history_count() > 0) {
                        search_state.match_index = history_count() - 1;
                    }
                    search_update(buf, &len, &pos, newline_count);
                }
                break;

            case KEY_CTRL_S:  // Forward incremental search
                last_was_tab = 0;
                if (search_state.active) {
                    // Cycle to next newer match
                    search_state.direction = -1;
                    if (search_state.match_index >= 0 && search_state.match_index < history_count() - 1) {
                        search_state.match_index++;
                    }
                    search_update(buf, &len, &pos, newline_count);
                }
                break;

            case KEY_CTRL_G:  // Cancel search
                last_was_tab = 0;
                if (search_state.active) {
                    // Restore original buffer
                    memcpy(buf, search_state.saved_buf, search_state.saved_len + 1);
                    len = search_state.saved_len;
                    pos = search_state.saved_pos;
                    search_cleanup();
                    refresh_line(buf, len, pos, prompt_str, newline_count);
                }
                break;

            case KEY_TAB:
                {
                    // Generate completions
                    CompletionResult *comp = completion_generate(buf, pos);

                    if (comp && comp->count > 0) {
                        if (comp->count == 1) {
                            // Single match - complete it
                            // Find start of word being completed
                            size_t word_start = pos;
                            while (word_start > 0 && !isspace(buf[word_start - 1])) {
                                word_start--;
                            }

                            // Remove old word
                            memmove(buf + word_start, buf + pos, len - pos);
                            len -= (pos - word_start);
                            pos = word_start;

                            // Insert completion
                            const char *match = comp->matches[0];
                            size_t match_len = strlen(match);

                            if (len + match_len < MAX_LINE_LENGTH) {
                                memmove(buf + pos + match_len, buf + pos, len - pos);
                                memcpy(buf + pos, match, match_len);
                                pos += match_len;
                                len += match_len;
                                buf[len] = '\0';

                                // Add space after completion, but NOT for directories
                                // (directories end with '/' and user likely wants to continue typing)
                                if (len < MAX_LINE_LENGTH - 1 && match_len > 0 && match[match_len - 1] != '/') {
                                    memmove(buf + pos + 1, buf + pos, len - pos);
                                    buf[pos] = ' ';
                                    pos++;
                                    len++;
                                    buf[len] = '\0';
                                }

                                refresh_line(buf, len, pos, prompt_str, newline_count);
                                // newline_count = count_newlines(buf);
                            }

                            last_was_tab = 0;
                        } else {
                            // Multiple matches
                            if (last_was_tab) {
                                // Second TAB - show all matches (basenames only)
                                ret = write(STDOUT_FILENO, "\r\n", 2);
                                (void)ret;

                                // Calculate column layout using display names (basenames)
                                int term_width = get_terminal_width();
                                size_t max_len = 0;

                                // Find longest display name
                                for (int i = 0; i < comp->count; i++) {
                                    const char *display = get_display_name(comp->matches[i]);
                                    size_t mlen = strlen(display);
                                    if (mlen > max_len) max_len = mlen;
                                }

                                // Add 2 spaces padding between columns
                                size_t col_width = max_len + 2;
                                int cols_per_row = term_width / col_width;
                                if (cols_per_row < 1) cols_per_row = 1;

                                // Display matches in columns (basenames only)
                                for (int i = 0; i < comp->count; i++) {
                                    const char *display = get_display_name(comp->matches[i]);

                                    // Colorize directories
                                    struct stat st;
                                    int is_dir = (stat(comp->matches[i], &st) == 0 &&
                                                  S_ISDIR(st.st_mode));
                                    if (is_dir) {
                                        const char *dcolor = color_config_get(
                                            color_config.comp_directory);
                                        ret = write(STDOUT_FILENO, dcolor, strlen(dcolor));
                                        (void)ret;
                                    }

                                    ret = write(STDOUT_FILENO, display, strlen(display));
                                    (void)ret;

                                    if (is_dir) {
                                        ret = write(STDOUT_FILENO, COLOR_RESET,
                                                    strlen(COLOR_RESET));
                                        (void)ret;
                                    }

                                    // Add padding to align columns
                                    size_t display_len = strlen(display);
                                    if ((i + 1) % cols_per_row != 0 && i < comp->count - 1) {
                                        // Not end of row, add padding
                                        for (size_t pad = 0; pad < col_width - display_len; pad++) {
                                            ret = write(STDOUT_FILENO, " ", 1);
                                            (void)ret;
                                        }
                                    } else {
                                        // End of row or last item
                                        ret = write(STDOUT_FILENO, "\r\n", 2);
                                        (void)ret;
                                    }
                                }

                                // Ensure we end with newline
                                if (comp->count % cols_per_row != 0) {
                                    ret = write(STDOUT_FILENO, "\r\n", 2);
                                    (void)ret;
                                }

                                // Redraw prompt and line
                                refresh_line(buf, len, pos, prompt_str, newline_count);
                                // newline_count = count_newlines(buf);
                                last_was_tab = 0;
                            } else {
                                // First TAB - complete common prefix
                                if (comp->common_prefix) {
                                    size_t word_start = pos;
                                    while (word_start > 0 && !isspace(buf[word_start - 1])) {
                                        word_start--;
                                    }

                                    size_t prefix_len = strlen(comp->common_prefix);
                                    size_t current_word_len = pos - word_start;

                                    // Only insert if common prefix is longer than current word
                                    if (prefix_len > current_word_len) {
                                        // Remove current word
                                        memmove(buf + word_start, buf + pos, len - pos);
                                        len -= (pos - word_start);
                                        pos = word_start;

                                        // Insert common prefix
                                        if (len + prefix_len < MAX_LINE_LENGTH) {
                                            memmove(buf + pos + prefix_len, buf + pos, len - pos);
                                            memcpy(buf + pos, comp->common_prefix, prefix_len);
                                            pos += prefix_len;
                                            len += prefix_len;
                                            buf[len] = '\0';
                                            refresh_line(buf, len, pos, prompt_str, newline_count);
                                            // newline_count = count_newlines(buf);
                                        }
                                    }
                                }
                                last_was_tab = 1;
                            }
                        }
                    } else {
                        // No matches - beep
                        ret = write(STDOUT_FILENO, "\a", 1);
                        (void)ret;
                        last_was_tab = 0;
                    }

                    completion_free_result(comp);
                }
                break;

            default:
                // Regular character - reset tab tracking
                last_was_tab = 0;
                if (search_state.active) {
                    // Add character to search query
                    if (c >= 32 && c < 127 && search_state.query_len < sizeof(search_state.query) - 1) {
                        search_state.query[search_state.query_len++] = (char)c;
                        search_state.query[search_state.query_len] = '\0';
                        search_state.match_index = -1;  // Reset to search from end
                        search_update(buf, &len, &pos, newline_count);
                    }
                    break;
                }
                if (c >= 32 && c < 127 && len < MAX_LINE_LENGTH - 1) {
                    memmove(buf + pos + 1, buf + pos, len - pos);
                    buf[pos] = (char)c;
                    pos++;
                    len++;
                    buf[len] = '\0';

                    // Always refresh when syntax highlighting is on, or when inserting mid-line
                    if (pos < len || (colors_enabled && color_config.syntax_highlight_enabled)) {
                        refresh_line(buf, len, pos, prompt_str, newline_count);
                    } else {
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
