#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <stddef.h>

/**
 * Initialize line editor
 * Sets up terminal for raw mode when needed
 */
void lineedit_init(void);

/**
 * Read a line with full editing capabilities
 * Supports:
 * - Left/Right arrows - cursor movement
 * - Up/Down arrows - command history navigation
 * - Ctrl+A - beginning of line
 * - Ctrl+E - end of line
 * - Ctrl+U - delete to beginning
 * - Ctrl+K - delete to end
 * - Ctrl+W - delete word backward
 * - Ctrl+L - clear screen
 * - Ctrl+D - EOF (exit shell if line is empty)
 * - Backspace/Delete - delete character
 * - Home/End - beginning/end of line
 * - Tab - insert 4 spaces
 *
 * @param prompt Prompt string to display (can include ANSI colors)
 * @return Newly allocated string (caller must free), or NULL on EOF
 */
char *lineedit_read_line(const char *prompt);

/**
 * Cleanup line editor
 * Restores terminal settings
 */
void lineedit_cleanup(void);

#endif // LINEEDIT_H
