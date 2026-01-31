#include <string.h>
#include <ctype.h>
#include "danger.h"
#include "safe_string.h"

// Check if string contains a pattern (case-insensitive word boundary check)
static int contains_pattern(const char *str, const char *pattern) {
    if (!str || !pattern) return 0;
    return strstr(str, pattern) != NULL;
}

// Check if rm command is dangerous
static DangerLevel check_rm_danger(const char *args) {
    if (!args) return DANGER_NONE;

    // Check for -rf or -fr flags (recursive force)
    int has_recursive = contains_pattern(args, "-r") ||
                        contains_pattern(args, "-R") ||
                        contains_pattern(args, "--recursive");
    int has_force = contains_pattern(args, "-f") || contains_pattern(args, "--force");
    int has_rf = contains_pattern(args, "-rf") || contains_pattern(args, "-fr") ||
                 contains_pattern(args, "-Rf") || contains_pattern(args, "-fR");

    if (!has_rf && !(has_recursive && has_force)) {
        return DANGER_NONE;
    }

    // HIGH danger: rm -rf on root, home, or current directory
    if (contains_pattern(args, " /") ||
        contains_pattern(args, " /*") ||
        contains_pattern(args, "\t/") ||
        strstr(args, "/ ") != NULL ||
        (strlen(args) > 0 && args[strlen(args)-1] == '/' && !strchr(args + 1, '/'))) {
        // Check if it's literally just "/" at the end
        const char *slash = strrchr(args, '/');
        if (slash && (slash[1] == '\0' || slash[1] == ' ' || slash[1] == '*')) {
            // Check it's not a subdirectory
            const char *p = args;
            while (*p && (*p == '-' || isalpha(*p) || isspace(*p))) p++;
            if (*p == '/' && (p[1] == '\0' || p[1] == ' ' || p[1] == '*')) {
                return DANGER_HIGH;
            }
        }
    }

    // HIGH danger: rm -rf ~ or $HOME
    if (contains_pattern(args, " ~") ||
        contains_pattern(args, "\t~") ||
        contains_pattern(args, "$HOME")) {
        return DANGER_HIGH;
    }

    // HIGH danger: rm -rf .  (current directory)
    // Look for standalone . or ./ (not ./subdir)
    const char *p = args;
    while (*p) {
        if ((*p == ' ' || *p == '\t') && p[1] == '.') {
            // Check for "." alone
            if (p[2] == '\0' || p[2] == ' ' || p[2] == '\t') {
                return DANGER_HIGH;
            }
            // Check for "./" with nothing after (or just trailing space)
            if (p[2] == '/') {
                if (p[3] == '\0' || p[3] == ' ' || p[3] == '\t') {
                    return DANGER_HIGH;
                }
                // "./something" is ok - skip
            }
        }
        p++;
    }

    // MEDIUM danger: rm -rf * (wildcard in current directory)
    if (contains_pattern(args, " *") || contains_pattern(args, "\t*")) {
        return DANGER_MEDIUM;
    }

    return DANGER_NONE;
}

// Check if chmod command is dangerous
static DangerLevel check_chmod_danger(const char *args) {
    if (!args) return DANGER_NONE;

    // Check for 777 permissions (world writable)
    if (contains_pattern(args, "777")) {
        // More dangerous if recursive
        if (contains_pattern(args, "-R") || contains_pattern(args, "--recursive")) {
            return DANGER_HIGH;
        }
        return DANGER_MEDIUM;
    }

    // Check for 666 (world writable files)
    if (contains_pattern(args, "666")) {
        return DANGER_MEDIUM;
    }

    return DANGER_NONE;
}

// Check if dd command is dangerous
static DangerLevel check_dd_danger(const char *args) {
    if (!args) return DANGER_NONE;

    // HIGH danger: writing to device files
    if (contains_pattern(args, "of=/dev/")) {
        return DANGER_HIGH;
    }

    // MEDIUM danger: writing to any device-like path
    if (contains_pattern(args, "of=/dev")) {
        return DANGER_MEDIUM;
    }

    return DANGER_NONE;
}

// Check if mkfs command is dangerous (any mkfs is dangerous)
static DangerLevel check_mkfs_danger(const char *cmd) {
    if (!cmd) return DANGER_NONE;

    // Any mkfs command is high danger
    if (strncmp(cmd, "mkfs", 4) == 0) {
        return DANGER_HIGH;
    }

    return DANGER_NONE;
}

// Check for fork bomb patterns
static DangerLevel check_fork_bomb(const char *input) {
    if (!input) return DANGER_NONE;

    // Classic bash fork bomb: :(){ :|:& };:
    if (contains_pattern(input, ":|:") ||
        contains_pattern(input, ": &};") ||
        contains_pattern(input, "(){") ||
        contains_pattern(input, "() {")) {
        // More specific check for fork bomb structure
        if ((contains_pattern(input, ":|:") || contains_pattern(input, "| :")) &&
            contains_pattern(input, "&")) {
            return DANGER_HIGH;
        }
    }

    return DANGER_NONE;
}

// Check for dangerous redirections
static DangerLevel check_redirect_danger(const char *input) {
    if (!input) return DANGER_NONE;

    // HIGH danger: overwriting device files
    if (contains_pattern(input, "> /dev/sd") ||
        contains_pattern(input, "> /dev/hd") ||
        contains_pattern(input, "> /dev/nvme") ||
        contains_pattern(input, ">/dev/sd") ||
        contains_pattern(input, ">/dev/hd") ||
        contains_pattern(input, ">/dev/nvme")) {
        return DANGER_HIGH;
    }

    return DANGER_NONE;
}

// Main danger check function
DangerLevel danger_check(const char *input, size_t len) {
    if (!input || len == 0) return DANGER_NONE;

    // Create null-terminated copy if needed
    char buf[4096];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, input, len);
    buf[len] = '\0';

    DangerLevel level = DANGER_NONE;
    DangerLevel check;

    // Check for fork bombs
    check = check_fork_bomb(buf);
    if (check > level) level = check;

    // Check for dangerous redirections
    check = check_redirect_danger(buf);
    if (check > level) level = check;

    // Find command and args
    const char *p = buf;

    // Skip leading whitespace
    while (*p && isspace(*p)) p++;

    // Find end of command
    const char *cmd_start = p;
    while (*p && !isspace(*p) && *p != '|' && *p != '&' && *p != ';') p++;

    // Extract command name
    size_t cmd_len = (size_t)(p - cmd_start);
    char cmd[256];
    if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
    memcpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';

    // Get args (rest of the string)
    const char *args = p;

    // Check specific commands
    check = danger_check_command(cmd, args);
    if (check > level) level = check;

    return level;
}

// Check specific command with arguments
DangerLevel danger_check_command(const char *cmd, const char *args) {
    if (!cmd) return DANGER_NONE;

    // Get basename if cmd is a path
    const char *basename = strrchr(cmd, '/');
    if (basename) {
        basename++;
    } else {
        basename = cmd;
    }

    // Check rm command
    if (strcmp(basename, "rm") == 0) {
        return check_rm_danger(args);
    }

    // Check chmod command
    if (strcmp(basename, "chmod") == 0) {
        return check_chmod_danger(args);
    }

    // Check dd command
    if (strcmp(basename, "dd") == 0) {
        return check_dd_danger(args);
    }

    // Check mkfs commands
    if (strncmp(basename, "mkfs", 4) == 0) {
        return check_mkfs_danger(basename);
    }

    // Check shred command on important paths
    if (strcmp(basename, "shred") == 0 && args) {
        if (contains_pattern(args, "/dev/") ||
            contains_pattern(args, " /") ||
            contains_pattern(args, " ~")) {
            return DANGER_HIGH;
        }
    }

    return DANGER_NONE;
}
