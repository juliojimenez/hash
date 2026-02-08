#ifndef HASH_H
#define HASH_H
#include <stdbool.h>

// MAX_LINE: Terminal input buffer limit (canonical mode typically caps at 4KB)
// This is NOT the limit for external command arguments - that uses ARG_MAX
#define MAX_LINE 4096

// MAX_ARGS: Internal buffer size for argument tracking during expansion
// The actual limit for external commands is the system's ARG_MAX (queried via sysconf)
#define MAX_ARGS 256

#define HASH_NAME "hash"
#define HASH_VERSION "36"

// Global flag indicating interactive mode (for history tracking, etc.)
extern bool is_interactive;

// Global flag to indicate we're in a command substitution child and
// should exec directly instead of fork+exec for external commands.
// This ensures $PPID returns the correct parent PID.
extern bool exec_directly_in_child;

#endif
