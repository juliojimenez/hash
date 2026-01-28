#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "syslimits.h"

// Default ARG_MAX if sysconf fails (256KB - conservative for older systems)
#define DEFAULT_ARG_MAX 262144

// Cache the ARG_MAX value (queried once)
static long cached_arg_max = 0;

extern char **environ;

long syslimits_arg_max(void) {
    if (cached_arg_max == 0) {
        cached_arg_max = sysconf(_SC_ARG_MAX);
        if (cached_arg_max <= 0) {
            // sysconf failed, use default
            cached_arg_max = DEFAULT_ARG_MAX;
        }
    }
    return cached_arg_max;
}

size_t syslimits_args_size(char **args) {
    if (!args) return 0;

    size_t total = 0;
    for (int i = 0; args[i] != NULL; i++) {
        // Each argument: string length + null terminator + pointer size
        total += strlen(args[i]) + 1;
        total += sizeof(char *);
    }
    // Final NULL pointer
    total += sizeof(char *);

    return total;
}

size_t syslimits_env_size(void) {
    if (!environ) return 0;

    size_t total = 0;
    for (int i = 0; environ[i] != NULL; i++) {
        total += strlen(environ[i]) + 1;
        total += sizeof(char *);
    }
    total += sizeof(char *);

    return total;
}

int syslimits_check_exec_args(char **args) {
    long arg_max = syslimits_arg_max();
    size_t args_size = syslimits_args_size(args);
    size_t env_size = syslimits_env_size();

    // Leave some headroom for kernel overhead
    size_t total = args_size + env_size;
    size_t limit = (size_t)arg_max;

    // Use 95% of ARG_MAX to be safe
    if (total > (limit * 95 / 100)) {
        errno = E2BIG;
        return -1;
    }

    return 0;
}
