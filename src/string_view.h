#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>

typedef struct {
    const char *str;
    size_t len;
} StringView;

#endif
