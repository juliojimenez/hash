// ulimit: resource limits of the shell and the commands it starts, through
// getrlimit/setrlimit. Its own file so that the non-POSIX resources (-l -m -u
// -v) are visible: builtins.c asks for _POSIX_C_SOURCE only.
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/resource.h>

#include "builtins.h"
#include "execute.h"   // last_command_exit_code
#include "hash.h"

typedef struct {
    char letter;
    int resource;
    const char *name;
    const char *unit;    // shown in -a listings
    rlim_t scale;        // the unit's size in bytes, 1 for counts
} UlimitEntry;

static const UlimitEntry ulimit_table[] = {
    {'c', RLIMIT_CORE,   "core file size",      "blocks",  512},
    {'d', RLIMIT_DATA,   "data seg size",       "kbytes",  1024},
    {'f', RLIMIT_FSIZE,  "file size",           "blocks",  512},
#ifdef RLIMIT_MEMLOCK
    {'l', RLIMIT_MEMLOCK, "max locked memory",  "kbytes",  1024},
#endif
#ifdef RLIMIT_RSS
    {'m', RLIMIT_RSS,    "max memory size",     "kbytes",  1024},
#endif
    {'n', RLIMIT_NOFILE, "open files",          "",        1},
    {'s', RLIMIT_STACK,  "stack size",          "kbytes",  1024},
    {'t', RLIMIT_CPU,    "cpu time",            "seconds", 1},
#ifdef RLIMIT_NPROC
    {'u', RLIMIT_NPROC,  "max user processes",  "",        1},
#endif
#ifdef RLIMIT_AS
    {'v', RLIMIT_AS,     "virtual memory",      "kbytes",  1024},
#endif
};

static void ulimit_print_value(rlim_t value, rlim_t scale) {
    if (value == RLIM_INFINITY) {
        printf("unlimited\n");
    } else {
        printf("%llu\n", (unsigned long long)(value / scale));
    }
}

int shell_ulimit(char **args) {
    bool hard = false, soft = false, all = false;
    const UlimitEntry *chosen = NULL;
    int i = 1;
    while (args[i] && args[i][0] == '-' && args[i][1]) {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        for (const char *o = args[i] + 1; *o; o++) {
            if (*o == 'H') {
                hard = true;
            } else if (*o == 'S') {
                soft = true;
            } else if (*o == 'a') {
                all = true;
            } else {
                const UlimitEntry *e = NULL;
                for (size_t k = 0; k < sizeof(ulimit_table) / sizeof(ulimit_table[0]); k++) {
                    if (ulimit_table[k].letter == *o) e = &ulimit_table[k];
                }
                if (!e) {
                    fprintf(stderr, "%s: ulimit: -%c: invalid option\n", HASH_NAME, *o);
                    fprintf(stderr, "ulimit: usage: ulimit [-HS] [-a | -cdfnstuv] [limit]\n");
                    last_command_exit_code = 2;
                    return 1;
                }
                if (chosen && chosen != e) {
                    fprintf(stderr, "%s: ulimit: one resource at a time\n", HASH_NAME);
                    last_command_exit_code = 2;
                    return 1;
                }
                chosen = e;
            }
        }
        i++;
    }
    if (!chosen) {
        chosen = &ulimit_table[2];  // -f, as POSIX says
    }

    if (all) {
        if (args[i]) {
            fprintf(stderr, "%s: ulimit: -a takes no limit\n", HASH_NAME);
            last_command_exit_code = 2;
            return 1;
        }
        for (size_t k = 0; k < sizeof(ulimit_table) / sizeof(ulimit_table[0]); k++) {
            const UlimitEntry *e = &ulimit_table[k];
            struct rlimit rl;
            if (getrlimit(e->resource, &rl) != 0) continue;
            char tag[32];
            if (e->unit[0]) {
                snprintf(tag, sizeof(tag), "(%s, -%c)", e->unit, e->letter);
            } else {
                snprintf(tag, sizeof(tag), "(-%c)", e->letter);
            }
            printf("%-21s %14s ", e->name, tag);
            ulimit_print_value(hard ? rl.rlim_max : rl.rlim_cur, e->scale);
        }
        return listing_status("ulimit");
    }

    struct rlimit rl;
    if (getrlimit(chosen->resource, &rl) != 0) {
        fprintf(stderr, "%s: ulimit: %s\n", HASH_NAME, strerror(errno));
        last_command_exit_code = 1;
        return 1;
    }

    if (args[i] == NULL) {
        // Show: the soft limit unless -H asks for the hard one
        ulimit_print_value(hard ? rl.rlim_max : rl.rlim_cur, chosen->scale);
        return listing_status("ulimit");
    }
    if (args[i + 1] != NULL) {
        fprintf(stderr, "%s: ulimit: too many arguments\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }

    rlim_t value;
    if (strcmp(args[i], "unlimited") == 0) {
        value = RLIM_INFINITY;
    } else if (strcmp(args[i], "hard") == 0) {
        value = rl.rlim_max;
    } else if (strcmp(args[i], "soft") == 0) {
        value = rl.rlim_cur;
    } else {
        char *end = NULL;
        errno = 0;
        unsigned long long n = strtoull(args[i], &end, 10);
        if (args[i][0] == '\0' || *end != '\0' || errno != 0 || args[i][0] == '-') {
            fprintf(stderr, "%s: ulimit: %s: invalid number\n", HASH_NAME, args[i]);
            last_command_exit_code = 2;
            return 1;
        }
        // rlim_t is unsigned on Linux and macOS and signed on FreeBSD: the
        // arithmetic is done in unsigned long long and checked for overflow
        // before it becomes an rlim_t
        unsigned long long scaled = n * (unsigned long long)chosen->scale;
        if ((chosen->scale > 1 && scaled / (unsigned long long)chosen->scale != n) ||
            scaled > (unsigned long long)RLIM_INFINITY) {
            value = RLIM_INFINITY;  // beyond what the type holds: no limit
        } else {
            value = (rlim_t)scaled;
        }
    }

    // Both limits unless one is named (POSIX: -S and -H default to both)
    if (!hard && !soft) {
        hard = soft = true;
    }
    if (soft) rl.rlim_cur = value;
    if (hard) rl.rlim_max = value;
    if (soft && !hard && rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max) {
        fprintf(stderr, "%s: ulimit: %s: cannot raise the soft limit above the hard limit\n", HASH_NAME, args[i]);
        last_command_exit_code = 1;
        return 1;
    }
    if (setrlimit(chosen->resource, &rl) != 0) {
        fprintf(stderr, "%s: ulimit: %s: %s\n", HASH_NAME, args[i], strerror(errno));
        last_command_exit_code = 1;
        return 1;
    }
    last_command_exit_code = 0;
    return 1;
}

