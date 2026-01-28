#ifndef SYSLIMITS_H
#define SYSLIMITS_H

#include <stddef.h>

// Get the system's ARG_MAX limit (max bytes for execve arguments + environment)
// Returns the limit in bytes, or a reasonable default if unavailable
long syslimits_arg_max(void);

// Calculate the total size of arguments array (including null terminators and pointers)
size_t syslimits_args_size(char **args);

// Calculate the total size of environment (for execve limit checking)
size_t syslimits_env_size(void);

// Check if arguments would exceed ARG_MAX when combined with environment
// Returns 0 if OK, -1 if would exceed limit (sets errno to E2BIG)
int syslimits_check_exec_args(char **args);

#endif
