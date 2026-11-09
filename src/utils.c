#include "utils.h"
#include <ctype.h>
#include <stdlib.h>

// Search for character in the string
bool char_in_string(char c, const char *str) {
    for (int i = 0; str[i] || i == 0; ++i) {
        if (str[i] == c) {
            return true;
        }
    }

    return false;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t put_utf8(char *out, unsigned long cp) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp > 0x10FFFF) cp = 0xFFFD;
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

const char *ansi_c_decode(const char *s, char *out, size_t *out_len) {
    const char *p = s + 1;
    size_t n = 0;
    while (*p && *p != '\'') {
        if (*p != '\\' || !p[1]) {
            out[n++] = *p++;
            continue;
        }
        p++;
        char c = *p++;
        switch (c) {
            case 'a': out[n++] = '\a'; break;
            case 'b': out[n++] = '\b'; break;
            case 'e': case 'E': out[n++] = 27; break;
            case 'f': out[n++] = '\f'; break;
            case 'n': out[n++] = '\n'; break;
            case 'r': out[n++] = '\r'; break;
            case 't': out[n++] = '\t'; break;
            case 'v': out[n++] = '\v'; break;
            case '\\': out[n++] = '\\'; break;
            case '\'': out[n++] = '\''; break;
            case '"': out[n++] = '"'; break;
            case '?': out[n++] = '?'; break;
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
                unsigned v = (unsigned)(c - '0');
                for (int k = 1; k < 3 && *p >= '0' && *p <= '7'; k++) {
                    v = v * 8 + (unsigned)(*p++ - '0');
                }
                out[n++] = (char)v;
                break;
            }
            case 'x': {
                unsigned v = 0;
                int k = 0;
                while (k < 2 && hex_value(*p) >= 0) {
                    v = v * 16 + (unsigned)hex_value(*p++);
                    k++;
                }
                if (k == 0) {
                    out[n++] = '\\';
                    out[n++] = 'x';
                } else {
                    out[n++] = (char)v;
                }
                break;
            }
            case 'u': case 'U': {
                int max = c == 'u' ? 4 : 8;
                unsigned long v = 0;
                int k = 0;
                while (k < max && hex_value(*p) >= 0) {
                    v = v * 16 + (unsigned long)hex_value(*p++);
                    k++;
                }
                if (k == 0) {
                    out[n++] = '\\';
                    out[n++] = c;
                } else {
                    n += put_utf8(out + n, v);
                }
                break;
            }
            case 'c':
                if (*p) {
                    out[n++] = (char)(toupper((unsigned char)*p) ^ 0x40);
                    p++;
                } else {
                    out[n++] = '\\';
                    out[n++] = 'c';
                }
                break;
            default:
                out[n++] = '\\';
                out[n++] = c;
                break;
        }
    }
    if (*p != '\'') return NULL;
    *out_len = n;
    return p + 1;
}
