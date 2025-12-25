#ifndef SAFE_STRING_H
#define SAFE_STRING_H

#include <stddef.h>

/**
 * Safe string copy - always null-terminates
 * Similar to strlcpy but portable
 *
 * @param dst Destination buffer
 * @param src Source string
 * @param size Size of destination buffer
 * @return Length of src (for truncation detection)
 */
size_t safe_strcpy(char *dst, const char *src, size_t size);

/**
 * Safe string length - bounds-checked
 * Returns length of string or maxlen if no null terminator found
 *
 * @param str String to measure
 * @param maxlen Maximum length to search
 * @return Length of string (up to maxlen)
 */
size_t safe_strlen(const char *str, size_t maxlen);

/**
 * Safe string append - always null-terminates
 *
 * @param dst Destination buffer
 * @param src Source string to append
 * @param size Size of destination buffer
 * @return Total length after append
 */
size_t safe_strcat(char *dst, const char *src, size_t size);

/**
 * Safe string compare with length limit
 *
 * @param s1 First string
 * @param s2 Second string
 * @param maxlen Maximum length to compare
 * @return 0 if equal, non-zero otherwise
 */
int safe_strcmp(const char *s1, const char *s2, size_t maxlen);

#endif // SAFE_STRING_H
