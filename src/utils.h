#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#include <stdbool.h>

/**
 * Check whether character is in the string.
 *
 * @param c The character to look for
 * @param str The string to search in
 *
 * @return Returns true if string has character.
 *
 * @note If the string includes NULL character (`\0`) for comparison,
 *      It should be the first character in the string.
 * @note The str should not be ""
 * @note If str[0] == '\0', it is compared with the character (`c`) and not considered as the string termination.
 * @note str should not be NULL.
 *
 * @example char_in_string(c, "\0abc?!");
 * char_in_string(c, "ab\0c?!"); // wrong
 */
bool char_in_string(char c, const char *str);

/**
 * The bytes the shell uses as internal markers while a word is expanded:
 * \x01 "the next byte is literal", \x02 "quoted $", \x03 and \x04 field
 * boundaries. A real byte in this range (from a $(printf '\001') result, a
 * variable, or the input itself) travels as \x01 + byte so it cannot be read
 * as a marker, and comes out as the byte again when the markers are removed.
 */
static inline bool is_marker_byte(char c) {
    return (unsigned char)c >= 1 && (unsigned char)c <= 4;
}

/**
 * $'...' (dollar-single-quotes: POSIX.1-2024, bash, ksh, zsh). s points at the
 * opening quote. Decodes the C-style escapes (\a \b \e \f \n \r \t \v \\ \' \"
 * \? \nnn \xHH \uHHHH \UHHHHHHHH \cX; an unknown one is kept as written) into
 * out, which needs strlen(s) bytes, sets *out_len, and returns the position
 * after the closing quote, or NULL when the input ends before it.
 */
const char *ansi_c_decode(const char *s, char *out, size_t *out_len);

#endif // UTILS_H
