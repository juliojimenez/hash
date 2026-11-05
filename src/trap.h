#ifndef TRAP_H
#define TRAP_H

#include <signal.h>

// Special pseudo-signal numbers for shell traps
#define TRAP_EXIT   0
#define TRAP_DEBUG  -1
#define TRAP_ERR    -2
#define TRAP_RETURN -3

// Maximum number of traps we support
#define MAX_TRAPS 64

// Initialize trap system
void trap_init(void);

// Clean up trap system
void trap_cleanup(void);

// Set a trap for a signal
// action: the command string to execute (NULL or "-" to reset to default)
// signal_name: signal name (e.g., "INT", "HUP", "EXIT") or number
// Returns 0 on success, -1 on error
int trap_set(const char *action, const char *signal_name);

// Get the trap action for a signal
// Returns NULL if no trap set, or the command string
const char *trap_get(int signum);

/**
 * Run the EXIT trap, if one is set, and return the status the shell (or
 * subshell) exits with. `status` is the status it was exiting with: the trap
 * action sees it as $? and it stays the exit status unless the action itself
 * calls exit with an operand (POSIX). A bare `exit` inside the trap keeps it.
 */
int trap_exit_status(int status);

/**
 * While the EXIT trap runs: the status the shell is exiting with. -1 otherwise.
 * The exit builtin uses it so that a bare `exit` inside the trap keeps it.
 */
int trap_running_exit_status(void);

// Reset traps for subshell (POSIX: traps are not inherited by subshells)
void trap_reset_for_subshell(void);

// List all traps
void trap_list(void);

// Parse signal name to number
// Returns -1 if invalid
int trap_parse_signal(const char *name);

// Get signal name from number
const char *trap_signal_name(int signum);

#endif // TRAP_H
