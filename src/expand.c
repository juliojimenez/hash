#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include "expand.h"
#include "safe_string.h"

// Get home directory for a user
static char *get_home_dir(const char *username) {
    static char home[PATH_MAX];

    if (!username || *username == '\0') {
        // Get current user's home
        const char *home_env = getenv("HOME");
        if (home_env) {
            safe_strcpy(home, home_env, sizeof(home));
            return home;
        }

        // Use getpwuid_r for thread safety
        struct passwd pw;
        struct passwd *result = NULL;
        char buf[1024];

        if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
            safe_strcpy(home, pw.pw_dir, sizeof(home));
            return home;
        }

        return NULL;
    }

    // Get specified user's home directory
    struct passwd pw;
    struct passwd *result = NULL;
    char buf[1024];

    if (getpwnam_r(username, &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
        safe_strcpy(home, pw.pw_dir, sizeof(home));
        return home;
    }

    return NULL;
}

// Expand a single path with tilde
char *expand_tilde_path(const char *path) {
    if (!path || path[0] != '~') {
        return NULL;  // No expansion needed
    }

    // Find where the username ends (at / or end of string)
    const char *slash = strchr(path, '/');
    size_t username_len = slash ? (size_t)(slash - path - 1) : strlen(path) - 1;

    // Extract username (empty string for current user)
    char username[256] = {0};
    if (username_len > 0 && username_len < sizeof(username)) {
        memcpy(username, path + 1, username_len);
        username[username_len] = '\0';
    }

    // Get home directory for user
    char *home = get_home_dir(username[0] != '\0' ? username : NULL);
    if (!home) {
        return NULL;  // Couldn't get home directory
    }

    // Build expanded path
    char *expanded = malloc(PATH_MAX);
    if (!expanded) {
        return NULL;
    }

    if (slash) {
        // ~/path or ~user/path
        snprintf(expanded, PATH_MAX, "%s%s", home, slash);
    } else {
        // Just ~ or ~user
        safe_strcpy(expanded, home, PATH_MAX);
    }

    return expanded;
}

// Expand tilde in all arguments
int expand_tilde(char **args) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (args[i][0] == '~') {
            char *expanded = expand_tilde_path(args[i]);
            if (expanded) {
                // Replace the argument with expanded version
                args[i] = expanded;
            }
            // If expansion failed, keep original
        }
    }

    return 0;
}
