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
#define KEY_NULL        0
#define KEY_CTRL_A      1
#define KEY_CTRL_B      2
#define KEY_CTRL_C      3
#define KEY_CTRL_D      4
#define KEY_CTRL_E      5
#define KEY_CTRL_F      6
#define KEY_CTRL_H      8
#define KEY_TAB         9
#define KEY_CTRL_K      11
#define KEY_CTRL_L      12
#define KEY_ENTER       13
#define KEY_CTRL_U      21
#define KEY_CTRL_W      23
#define KEY_ESC         27
#define KEY_BACKSPACE   127
#define KEY_CTRL_G      7   // Cancel search
#define KEY_CTRL_R      18  // Reverse search
#define KEY_CTRL_S      19  // Forward search
#define KEY_UP_ARROW    ('A' + 256)
#define KEY_DOWN_ARROW  ('B' + 256)
#define KEY_RIGHT_ARROW ('C' + 256)
#define KEY_LEFT_ARROW  ('D' + 256)

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
                    case 'A': return KEY_UP_ARROW;  // Up arrow
                    case 'B': return KEY_DOWN_ARROW;  // Down arrow
                    case 'C': return KEY_RIGHT_ARROW;  // Right arrow
                    case 'D': return KEY_LEFT_ARROW;  // Left arrow
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

// Compute the visual row and column for a given buffer position,
// accounting for prompt width and terminal-width wrapping.
// row 0 is the first line of the prompt's last line.
static void visual_pos(const char *buf, size_t pos, size_t prompt_width,
                       int term_width, size_t *out_row, size_t *out_col) {
    size_t col = prompt_width;
    size_t row = 0;

    // Walk through buffer up to pos
    for (size_t i = 0; i < pos; i++) {
        if (buf[i] == '\n') {
            // Explicit newline resets to column 0 on next row
            row++;
            col = 0;
            // No need to check for wrap
            continue;
        } else if ((unsigned char)buf[i] >= 0x80) {
            // UTF-8 continuation bytes don't start a new character
            // Only count width at the start byte
            if (((unsigned char)buf[i] & 0xC0) != 0x80) {
                wchar_t wc;
                int len = utf8_decode((const unsigned char *)buf + i, &wc);
                if (len > 0) {
                    int w = wcwidth(wc);
                    if (w < 0) {
                        w = 1;
                    }
                    col += (size_t)w;
                } else {
                    col++;
                }
            }
        } else {
            col++;
        }

        // Check for wrap
        if (term_width > 0 && col >= (size_t)term_width) {
            row += col / (size_t)term_width;
            col = col % (size_t)term_width;
        }
    }

    // Final wrap check (handles prompt_width >= term_width when pos=0,
    // or accumulated width after last newline)
    if (term_width > 0 && col >= (size_t)term_width) {
        row += col / (size_t)term_width;
        col = col % (size_t)term_width;
    }

    *out_row = row;
    *out_col = col;
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

// Track the visual row of the cursor from the prompt start across refreshes
static size_t last_cursor_visual_row = 0;

// Reset refresh state (call when starting a new line edit session)
static void refresh_line_reset(void) {
    last_cursor_visual_row = 0;
}

typedef struct {
    char buf[MAX_LINE_LENGTH];
    size_t len;
    size_t pos;
    bool last_was_tab;
    const char *prompt;
} LineEdit;

// Set the cursor to given position using visual row/col calculation.
// edit->pos should be previous position
static void set_cursor(LineEdit *edit, size_t new_pos) {
    ssize_t ret;

    // Only reposition cursor if it's not at the desired position
    if (edit->pos == new_pos) return;

    int term_width = get_terminal_width();
    size_t prompt_width = visible_prompt_length(edit->prompt);

    size_t prev_row, prev_col, new_row, new_col;
    visual_pos(edit->buf, edit->pos, prompt_width, term_width, &prev_row, &prev_col);
    visual_pos(edit->buf, new_pos, prompt_width, term_width, &new_row, &new_col);

    char cursor_seq[32];

    // Move vertically
    if (prev_row > new_row) {
        snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%zuA", prev_row - new_row);
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    } else if (new_row > prev_row) {
        snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%zuB", new_row - prev_row);
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    }

    // Move to correct column (absolute positioning via \r then forward)
    ret = write(STDOUT_FILENO, "\r", 1);
    (void)ret;

    if (new_col > 0) {
        snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%zuC", new_col);
        ret = write(STDOUT_FILENO, cursor_seq, safe_strlen(cursor_seq, sizeof(cursor_seq)));
        (void)ret;
    }

    // Update tracked cursor row
    last_cursor_visual_row = new_row;

    // Update the pos
    edit->pos = new_pos;
}

// Refresh the line on screen (supports multi-line prompt and buffer)
static void refresh_line(const LineEdit *edit) {
    ssize_t ret;
    int term_width = get_terminal_width();
    size_t prompt_width = visible_prompt_length(edit->prompt);
    size_t prompt_newlines = (size_t)count_newlines(edit->prompt);

    // Move cursor up from its current visual row to the prompt start
    size_t total_up = last_cursor_visual_row + prompt_newlines;
    if (total_up > 0) {
        char up_seq[32];
        snprintf(up_seq, sizeof(up_seq), "\x1b[%zuA", total_up);
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
    write_with_crlf(edit->prompt);

    // Apply syntax highlighting if enabled
    if (colors_enabled && color_config.syntax_highlight_enabled && edit->len > 0) {
        char *highlighted = syntax_render(edit->buf, edit->len);
        if (highlighted) {
            write_with_crlf(highlighted);
            free(highlighted);
        } else {
            write_with_crlf(edit->buf);
        }
    } else {
        write_with_crlf(edit->buf);
    }

    // Calculate where the end of buffer is visually (for autosuggestion handling)
    size_t end_row, end_col;
    visual_pos(edit->buf, edit->len, prompt_width, term_width, &end_row, &end_col);

    // Handle deferred wrap: when content fills exactly to the terminal edge,
    // the terminal cursor is in a "pending wrap" state (visually at end of
    // current row, not yet on the next row). Force it to actually wrap so
    // our row/col calculations match the real cursor position.
    if (term_width > 0 && end_col == 0 && end_row > 0 &&
            (edit->len == 0 || edit->buf[edit->len - 1] != '\n')) {
        // Write space to force wrap, \r to go to col 0 of the new line
        ret = write(STDOUT_FILENO, " \r", 2);
        (void)ret;
    }

    // Show autosuggestion if enabled and cursor is at end of buffer
    if (colors_enabled && color_config.autosuggestion_enabled && edit->pos == edit->len && edit->len > 0) {
        const char *suggestion = autosuggest_get(edit->buf, edit->len);
        if (suggestion && suggestion[0]) {
            // Write suggestion in dim color
            const char *suggest_color = color_config_get(color_config.suggestion);
            ret = write(STDOUT_FILENO, suggest_color, strlen(suggest_color));
            (void)ret;
            write_with_crlf(suggestion);
            // Reset color
            const char *reset = color_code(COLOR_RESET);
            ret = write(STDOUT_FILENO, reset, strlen(reset));
            (void)ret;

            // Move cursor back up past suggestion lines (newlines + wraps)
            // Count visual lines in the suggestion
            size_t suggest_row = 0;
            size_t suggest_col = end_col;
            const unsigned char *sp = (const unsigned char *)suggestion;
            while (*sp) {
                if (*sp == '\n') {
                    suggest_row++;
                    suggest_col = 0;
                } else if (*sp >= 0x80) {
                    wchar_t wc;
                    int slen = utf8_decode(sp, &wc);
                    if (slen > 0) {
                        int w = wcwidth(wc);
                        if (w < 0) {
                            w = 1;
                        }
                        suggest_col += (size_t)w;
                        sp += slen - 1;
                    }
                } else {
                    suggest_col++;
                }
                if (term_width > 0 && suggest_col >= (size_t)term_width) {
                    suggest_row += suggest_col / (size_t)term_width;
                    suggest_col = suggest_col % (size_t)term_width;
                }
                sp++;
            }
            if (suggest_row > 0) {
                char up_seq[32];
                snprintf(up_seq, sizeof(up_seq), "\x1b[%zuA", suggest_row);
                ret = write(STDOUT_FILENO, up_seq, strlen(up_seq));
                (void)ret;
            }
            // Move to correct column (end of buffer)
            ret = write(STDOUT_FILENO, "\r", 1);
            (void)ret;
            if (end_col > 0) {
                char col_seq[32];
                snprintf(col_seq, sizeof(col_seq), "\x1b[%zuC", end_col);
                ret = write(STDOUT_FILENO, col_seq, strlen(col_seq));
                (void)ret;
            }
        }
    }

    // Now cursor is at the end of buffer; move it to desired pos
    size_t pos_row, pos_col;
    visual_pos(edit->buf, edit->pos, prompt_width, term_width, &pos_row, &pos_col);

    if (end_row > pos_row) {
        char up_seq[32];
        snprintf(up_seq, sizeof(up_seq), "\x1b[%zuA", end_row - pos_row);
        ret = write(STDOUT_FILENO, up_seq, strlen(up_seq));
        (void)ret;
    } else if (pos_row > end_row) {
        char down_seq[32];
        snprintf(down_seq, sizeof(down_seq), "\x1b[%zuB", pos_row - end_row);
        ret = write(STDOUT_FILENO, down_seq, strlen(down_seq));
        (void)ret;
    }
    ret = write(STDOUT_FILENO, "\r", 1);
    (void)ret;
    if (pos_col > 0) {
        char col_seq[32];
        snprintf(col_seq, sizeof(col_seq), "\x1b[%zuC", pos_col);
        ret = write(STDOUT_FILENO, col_seq, strlen(col_seq));
        (void)ret;
    }

    // Remember the cursor's visual row for next refresh
    last_cursor_visual_row = pos_row;
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
    bool single_quote = 0;
    bool double_quote = 0;
    // Loop through buffer to check whether we are inside the quote
    // If we are already inside one type of quote, then other type of quote is ignored.
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] == '\'' && !double_quote) single_quote = !single_quote;
        else if (buf[i] == '"' && !single_quote) double_quote = !double_quote;
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
static void search_refresh_line(const LineEdit *edit, bool has_match) {
    // Build search prompt
    char search_prompt[512];
    const char *mode = (search_state.direction == 1) ? "reverse" : "forward";
    const char *status = has_match ? "" : "failing ";

    snprintf(search_prompt, sizeof(search_prompt),
             "(%s%s-i-search)`%s': ", status, mode, search_state.query);

    refresh_line(edit);
}

// Perform search and update display
static void search_update(LineEdit *edit) {
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
        safe_strcpy(edit->buf, match, MAX_LINE_LENGTH);
        edit->len = safe_strlen(edit->buf, MAX_LINE_LENGTH);
        edit->pos = edit->len;
        search_state.match_index = result_idx;
    } else if (search_state.query_len == 0) {
        // Empty query - show empty buffer
        edit->buf[0] = '\0';
        edit->len = 0;
        edit->pos = 0;
        search_state.match_index = -1;
    }

    search_refresh_line(edit, match != NULL || search_state.query_len == 0);
}

// Returns result or NULL
static char *handle_enter_key(LineEdit *edit) {
    ssize_t ret;

    // Exit search mode if active, keep the current buffer
    if (search_state.active) {
        search_cleanup();
    }

    // Check for new line escape and quotes
    if (edit->len > 0 && (edit->buf[edit->len - 1] == '\\' || inside_quote(edit->buf, edit->len))) {
        edit->last_was_tab = false;
        if (edit->len < MAX_LINE_LENGTH - 1) {
            edit->buf[edit->len] = '\n';
            edit->len++;
            edit->pos = edit->len;
            edit->buf[edit->len] = '\0';

            // Write crlf to termial
            ret = write(STDOUT_FILENO, "\r\n", 2);
            (void)ret;
        }
        return NULL;
    }

    // Refresh line and move the cursor to end of line
    // Hack to disable auto suggestion, move cursor one position back
    if (edit->len > 0) {
        set_cursor(edit, edit->pos - 1);
        refresh_line(edit);
        set_cursor(edit, edit->len);
    }

    disable_raw_mode();

    // Write newline and flush
    ret = write(STDOUT_FILENO, "\r\n", 2);
    (void)ret;
    fflush(stdout);

    char *result = malloc(edit->len + 1);
    if (result) {
        memcpy(result, edit->buf, edit->len);
        result[edit->len] = '\0';
    }

    return result;
}

static char *handle_ctrl_c(LineEdit *edit) {
    ssize_t ret;
    if (search_state.active) {
        // Cancel search and restore original buffer
        memcpy(edit->buf, search_state.saved_buf, search_state.saved_len + 1);
        edit->len = search_state.saved_len;
        edit->pos = search_state.saved_pos;
        search_cleanup();
        refresh_line(edit);
        return NULL;
    }

    disable_raw_mode();

    // Move to beginning and write ^C on a fresh line
    ret = write(STDOUT_FILENO, "\r\x1b[K^C\n", 8);
    (void)ret;

    char *empty = malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
}

static void handle_backspace(LineEdit *edit) {
    if (search_state.active) {
        // Delete last character from search query
        if (search_state.query_len > 0) {
            search_state.query_len--;
            search_state.query[search_state.query_len] = '\0';
            search_state.match_index = -1;  // Reset to search from end
            search_update(edit);
        }
        return;
    }

    if (edit->pos > 0) {
        memmove(edit->buf + edit->pos - 1, edit->buf + edit->pos, edit->len - edit->pos);
        edit->pos--;
        edit->len--;
        edit->buf[edit->len] = '\0';
        refresh_line(edit);
    }
}

static void handle_right_arrow(LineEdit *edit) {
    if (search_state.active) {
        // Exit search mode, keep command for editing
        search_cleanup();
        refresh_line(edit);
        return;
    }

    if (edit->pos < edit->len) {
        set_cursor(edit, edit->pos + 1);
    } else if (edit->pos == edit->len && colors_enabled && color_config.autosuggestion_enabled) {
        // At end of buffer - accept autosuggestion if available
        const char *suggestion = autosuggest_get(edit->buf, edit->len);
        if (suggestion && suggestion[0]) {
            size_t sug_len = strlen(suggestion);
            if (edit->len + sug_len < MAX_LINE_LENGTH - 1) {
                // Append suggestion to buffer
                memcpy(edit->buf + edit->len, suggestion, sug_len);
                edit->len += sug_len;
                edit->pos = edit->len;
                edit->buf[edit->len] = '\0';
                refresh_line(edit);
                // Invalidate cache since buffer changed
                autosuggest_invalidate();
            }
        }
    }
}

static void handle_left_arrow(LineEdit *edit) {
    if (search_state.active) {
        // Exit search mode, keep command for editing
        search_cleanup();
        refresh_line(edit);
        return;
    }

    if (edit->pos > 0) {
        set_cursor(edit, edit->pos - 1);
    }
}

static void handle_up_arrow(LineEdit *edit) {
    const char *prev = history_prev();
    if (prev) {
        // Clear current line and load history entry
        safe_strcpy(edit->buf, prev, sizeof(edit->buf));
        edit->len = safe_strlen(edit->buf, sizeof(edit->buf));
        edit->pos = edit->len;
        refresh_line(edit);
        // update the new line count to match current buffer
    }
}

static void handle_down_arrow(LineEdit *edit) {
    const char *next = history_next();
    if (next) {
        // Load next history entry
        safe_strcpy(edit->buf, next, sizeof(edit->buf));
        edit->len = safe_strlen(edit->buf, sizeof(edit->buf));
        edit->pos = edit->len;
        refresh_line(edit);
        // update the new line count to match current buffer
    } else {
        // At end of history, clear line
        edit->len = 0;
        edit->pos = 0;
        edit->buf[0] = '\0';
        refresh_line(edit);
        // update the new line count
    }
}

static void handle_ctrl_a(LineEdit *edit) {
    edit->last_was_tab = false;
    size_t new_pos = edit->pos;

    while (new_pos > 0 && edit->buf[new_pos - 1] != '\n') {
        new_pos--;
    }

    if (new_pos != edit->pos) {
        set_cursor(edit, new_pos);
    }
}

static void handle_ctrl_e(LineEdit *edit) {
    edit->last_was_tab = false;
    size_t new_pos = edit->pos;

    while (new_pos < edit->len && edit->buf[new_pos + 1] != '\n') {
        new_pos++;
    }

    if (new_pos != edit->pos) {
        set_cursor(edit, new_pos);
    }
}

static void handle_ctrl_u(LineEdit *edit) {
    edit->last_was_tab = false;
    if (edit->pos > 0) {
        memmove(edit->buf, edit->buf + edit->pos, edit->len - edit->pos);
        edit->len -= edit->pos;
        edit->pos = 0;
        edit->buf[edit->len] = '\0';
        refresh_line(edit);
    }
}

static void handle_ctrl_k(LineEdit *edit) {
    edit->last_was_tab = false;
    edit->len = edit->pos;
    edit->buf[edit->len] = '\0';
    refresh_line(edit);
}

static void handle_ctrl_w(LineEdit *edit) {
    edit->last_was_tab = false;
    if (edit->pos > 0) {
        size_t old_pos = edit->pos;

        while (edit->pos > 0 && isspace(edit->buf[edit->pos - 1])) {
            edit->pos--;
        }

        while (edit->pos > 0 && !isspace(edit->buf[edit->pos - 1])) {
            edit->pos--;
        }

        memmove(edit->buf + edit->pos, edit->buf + old_pos, edit->len - old_pos);
        edit->len -= (old_pos - edit->pos);
        edit->buf[edit->len] = '\0';
        refresh_line(edit);
    }
}

static void handle_ctrl_l(LineEdit *edit) {
    edit->last_was_tab = false;
    ssize_t ret = write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);
    (void)ret;
    if (search_state.active) {
        search_refresh_line(edit, search_state.match_index >= 0 || search_state.query_len == 0);
    } else {
        refresh_line(edit);
    }
}

static void handle_ctrl_r(LineEdit *edit) {
    edit->last_was_tab = false;
    if (!search_state.active) {
        // Enter search mode
        search_init(edit->buf, edit->len, edit->pos);
        search_refresh_line(edit, true);
    } else {
        // Cycle to next older match
        search_state.direction = 1;
        if (search_state.match_index > 0) {
            search_state.match_index--;
        } else if (search_state.match_index < 0 && history_count() > 0) {
            search_state.match_index = history_count() - 1;
        }
        search_update(edit);
    }
}

static void handle_ctrl_s(LineEdit *edit) {
    edit->last_was_tab = false;
    if (search_state.active) {
        // Cycle to next newer match
        search_state.direction = -1;
        if (search_state.match_index >= 0 && search_state.match_index < history_count() - 1) {
            search_state.match_index++;
        }
        search_update(edit);
    }
}

static void handle_ctrl_g(LineEdit *edit) {
    edit->last_was_tab = false;
    if (search_state.active) {
        // Restore original buffer
        memcpy(edit->buf, search_state.saved_buf, search_state.saved_len + 1);
        edit->len = search_state.saved_len;
        edit->pos = search_state.saved_pos;
        search_cleanup();
        refresh_line(edit);
    }
}

static void handle_tab(LineEdit *edit) {
    ssize_t ret;
    // Generate completions
    CompletionResult *comp = completion_generate(edit->buf, edit->pos);

    if (comp && comp->count > 0) {
        if (comp->count == 1) {
            // Single match - complete it
            // Find start of word being completed
            size_t word_start = edit->pos;
            while (word_start > 0 && !isspace(edit->buf[word_start - 1])) {
                word_start--;
            }

            // Remove old word
            memmove(edit->buf + word_start, edit->buf + edit->pos, edit->len - edit->pos);
            edit->len -= (edit->pos - word_start);
            edit->pos = word_start;

            // Insert completion
            const char *match = comp->matches[0];
            size_t match_len = strlen(match);

            if (edit->len + match_len < MAX_LINE_LENGTH) {
                memmove(edit->buf + edit->pos + match_len, edit->buf + edit->pos, edit->len - edit->pos);
                memcpy(edit->buf + edit->pos, match, match_len);
                edit->pos += match_len;
                edit->len += match_len;
                edit->buf[edit->len] = '\0';

                // Add space after completion, but NOT for directories
                // (directories end with '/' and user likely wants to continue typing)
                if (edit->len < MAX_LINE_LENGTH - 1 && match_len > 0 && match[match_len - 1] != '/') {
                    memmove(edit->buf + edit->pos + 1, edit->buf + edit->pos, edit->len - edit->pos);
                    edit->buf[edit->pos] = ' ';
                    edit->pos++;
                    edit->len++;
                    edit->buf[edit->len] = '\0';
                }

                refresh_line(edit);
            }

            edit->last_was_tab = false;
        } else {
            // Multiple matches
            if (edit->last_was_tab) {
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
                refresh_line(edit);
                edit->last_was_tab = false;
            } else {
                // First TAB - complete common prefix
                if (comp->common_prefix) {
                    size_t word_start = edit->pos;
                    while (word_start > 0 && !isspace(edit->buf[word_start - 1])) {
                        word_start--;
                    }

                    size_t prefix_len = strlen(comp->common_prefix);
                    size_t current_word_len = edit->pos - word_start;

                    // Only insert if common prefix is longer than current word
                    if (prefix_len > current_word_len) {
                        // Remove current word
                        memmove(edit->buf + word_start, edit->buf + edit->pos, edit->len - edit->pos);
                        edit->len -= (edit->pos - word_start);
                        edit->pos = word_start;

                        // Insert common prefix
                        if (edit->len + prefix_len < MAX_LINE_LENGTH) {
                            memmove(edit->buf + edit->pos + prefix_len, edit->buf + edit->pos, edit->len - edit->pos);
                            memcpy(edit->buf + edit->pos, comp->common_prefix, prefix_len);
                            edit->pos += prefix_len;
                            edit->len += prefix_len;
                            edit->buf[edit->len] = '\0';
                            refresh_line(edit);
                        }
                    }
                }
                edit->last_was_tab = true;
            }
        }
    } else {
        // No matches - beep
        ret = write(STDOUT_FILENO, "\a", 1);
        (void)ret;
        edit->last_was_tab = false;
    }

    completion_free_result(comp);
}

static void handle_default(LineEdit *edit, int c) {
    ssize_t ret;
    // Regular character - reset tab tracking
    edit->last_was_tab = false;
    if (search_state.active) {
        // Add character to search query
        if (c >= 32 && c < 127 && search_state.query_len < sizeof(search_state.query) - 1) {
            search_state.query[search_state.query_len++] = (char)c;
            search_state.query[search_state.query_len] = '\0';
            search_state.match_index = -1;  // Reset to search from end
            search_update(edit);
        }
        return;
    }
    if (c >= 32 && c < 127 && edit->len < MAX_LINE_LENGTH - 1) {
        memmove(edit->buf + edit->pos + 1, edit->buf + edit->pos, edit->len - edit->pos);
        edit->buf[edit->pos] = (char)c;
        edit->pos++;
        edit->len++;
        edit->buf[edit->len] = '\0';

        // Always refresh when syntax highlighting is on, or when inserting mid-line
        if (edit->pos < edit->len || (colors_enabled && color_config.syntax_highlight_enabled)) {
            refresh_line(edit);
        } else {
            // Fast path: append at end without syntax highlighting
            ret = write(STDOUT_FILENO, &c, 1);
            (void)ret;
            // Update visual row tracking since terminal may wrap
            int tw = get_terminal_width();
            size_t pw = visible_prompt_length(edit->prompt);
            size_t vr, vc;
            visual_pos(edit->buf, edit->pos, pw, tw, &vr, &vc);
            last_cursor_visual_row = vr;
            // Force deferred wrap if at exact terminal edge
            if (tw > 0 && vc == 0 && vr > 0) {
                ret = write(STDOUT_FILENO, " \r", 2);
                (void)ret;
            }
        }
    }
}

// Read a line with editing capabilities
char *lineedit_read_line(const char *prompt) {
    static LineEdit edit = (LineEdit){0};
    char *result = NULL;

    // Clear buffer and state
    memset(edit.buf, 0, sizeof(edit.buf));
    edit.len = 0;
    edit.pos = 0;
    edit.prompt = prompt ? prompt : "";

    refresh_line_reset();

    // Enable raw mode first
    if (enable_raw_mode() == -1) {
        // Fallback to simple getline
        char *line = NULL;
        size_t bufsize = 0;

        // POSIX: interactive shell prompts go to stderr
        fprintf(stderr, "%s", edit.prompt);
        fflush(stderr);

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
    write_with_crlf(edit.prompt);
    fflush(stdout);

    while (1) {
        int c = read_key();

        if (c == -1) break;

        switch (c) {
            case KEY_ENTER:
                if ((result = handle_enter_key(&edit)) != NULL) {
                    return result;
                }
                break;

            case KEY_CTRL_D:
                if (edit.len == 0) {
                    disable_raw_mode();
                    return NULL;
                }
                break;

            case KEY_CTRL_C:
                if ((result = handle_ctrl_c(&edit)) != NULL) {
                    return result;
                }
                break;

            case KEY_BACKSPACE:
            case KEY_CTRL_H:
                handle_backspace(&edit);
                break;

            case KEY_RIGHT_ARROW:  // Right arrow
                handle_right_arrow(&edit);
                break;

            case KEY_LEFT_ARROW:  // Left arrow
                handle_left_arrow(&edit);
                break;

            case KEY_UP_ARROW:  // Up arrow - previous history
                handle_up_arrow(&edit);
                break;

            case KEY_DOWN_ARROW:  // Down arrow - next history
                handle_down_arrow(&edit);
                break;

            case KEY_CTRL_A:  // Beginning
                handle_ctrl_a(&edit);
                break;

            case KEY_CTRL_E:  // End
                handle_ctrl_e(&edit);
                break;

            case KEY_CTRL_U:  // Delete to beginning
                handle_ctrl_u(&edit);
                break;

            case KEY_CTRL_K:  // Delete to end
                handle_ctrl_k(&edit);
                break;

            case KEY_CTRL_W:  // Delete word backward
                handle_ctrl_w(&edit);
                break;

            case KEY_CTRL_L:  // Clear screen
                handle_ctrl_l(&edit);
                break;

            case KEY_CTRL_R:  // Reverse incremental search
                handle_ctrl_r(&edit);
                break;

            case KEY_CTRL_S:  // Forward incremental search
                handle_ctrl_s(&edit);
                break;

            case KEY_CTRL_G:  // Cancel search
                handle_ctrl_g(&edit);
                break;

            case KEY_TAB:
                handle_tab(&edit);
                break;

            default:
                handle_default(&edit, c);
                break;
        }
    }

    disable_raw_mode();
    return NULL;
}
