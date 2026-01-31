#ifndef DANGER_H
#define DANGER_H

#include <stddef.h>

// Danger levels
typedef enum {
    DANGER_NONE = 0,    // Safe command
    DANGER_MEDIUM,      // Potentially dangerous (e.g., rm -rf *, chmod 777)
    DANGER_HIGH         // Very dangerous (e.g., rm -rf /, dd of=/dev/sda)
} DangerLevel;

/**
 * Check if a command line is dangerous
 *
 * @param input Full command line to check
 * @param len Length of input
 * @return Danger level of the command
 */
DangerLevel danger_check(const char *input, size_t len);

/**
 * Check if a specific command with its arguments is dangerous
 *
 * @param cmd Command name (e.g., "rm", "dd")
 * @param args Full argument string after command
 * @return Danger level of the command
 */
DangerLevel danger_check_command(const char *cmd, const char *args);

#endif // DANGER_H
