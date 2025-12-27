#include "safe_string.h"
#include <string.h>

// Safe string copy - always null-terminates
size_t safe_strcpy(char *dst, const char *src, size_t size) {
    if (!dst || !src || size == 0) {
        return 0;
    }

    size_t src_len = 0;
    size_t i;

    // Copy up to size-1 characters
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
        src_len++;
    }

    // Always null-terminate
    dst[i] = '\0';

    // Count remaining source characters for truncation detection
    while (src[src_len] != '\0') {
        src_len++;
    }

    return src_len;
}

// Safe string length - bounds-checked
size_t safe_strlen(const char *str, size_t maxlen) {
    if (!str) {
        return 0;
    }

    size_t len = 0;
    while (len < maxlen && str[len] != '\0') {
        len++;
    }

    return len;
}

// Safe string append - always null-terminates
size_t safe_strcat(char *dst, const char *src, size_t size) {
    if (!dst || !src || size == 0) {
        return 0;
    }

    // Find current end of dst (up to size-1)
    size_t dst_len = 0;
    while (dst_len < size - 1 && dst[dst_len] != '\0') {
        dst_len++;
    }

    // If dst is already full, can't append
    if (dst_len >= size - 1) {
        dst[size - 1] = '\0';
        return dst_len;
    }

    // Append src
    size_t i;
    for (i = 0; dst_len + i < size - 1 && src[i] != '\0'; i++) {
        dst[dst_len + i] = src[i];
    }

    // Null-terminate
    dst[dst_len + i] = '\0';

    return dst_len + i;
}

// Safe string compare with length limit
int safe_strcmp(const char *s1, const char *s2, size_t maxlen) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : -1;
    }

    for (size_t i = 0; i < maxlen; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }

        if (s1[i] == '\0') {
            // Both strings ended
            return 0;
        }
    }

    // Reached maxlen without difference
    return 0;
}
