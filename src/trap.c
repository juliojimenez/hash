#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include "trap.h"
#include "hash.h"
#include "script.h"
#include "config.h"

// Trap storage
static char *traps[MAX_TRAPS];

// Inherited traps for display in subshells (POSIX: trap with no operands shows
// the trap commands as they were when the subshell was entered)
static char *inherited_traps[MAX_TRAPS];
static int in_subshell = 0;

// Signal name to number mapping
static const struct {
    const char *name;
    int num;
} signal_names[] = {
    {"EXIT", 0},
    {"HUP", SIGHUP},
    {"INT", SIGINT},
    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},
    {"TRAP", SIGTRAP},
    {"ABRT", SIGABRT},
    {"FPE", SIGFPE},
    {"KILL", SIGKILL},
    {"BUS", SIGBUS},
    {"SEGV", SIGSEGV},
    {"SYS", SIGSYS},
    {"PIPE", SIGPIPE},
    {"ALRM", SIGALRM},
    {"TERM", SIGTERM},
    {"URG", SIGURG},
    {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP},
    {"CONT", SIGCONT},
    {"CHLD", SIGCHLD},
    {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},
    {"IO", SIGIO},
    {"XCPU", SIGXCPU},
    {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM},
    {"PROF", SIGPROF},
    {"WINCH", SIGWINCH},
    {"USR1", SIGUSR1},
    {"USR2", SIGUSR2},
    {NULL, 0}
};

void trap_init(void) {
    memset(traps, 0, sizeof(traps));
}

void trap_cleanup(void) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (traps[i]) {
            free(traps[i]);
            traps[i] = NULL;
        }
    }
}

int trap_parse_signal(const char *name) {
    if (!name) return -1;

    // Skip SIG prefix if present
    if (strncasecmp(name, "SIG", 3) == 0) {
        name += 3;
    }

    // Check for numeric signal
    if (isdigit(name[0])) {
        int num = atoi(name);
        if (num >= 0 && num < MAX_TRAPS) {
            return num;
        }
        return -1;
    }

    // Look up by name
    for (int i = 0; signal_names[i].name; i++) {
        if (strcasecmp(name, signal_names[i].name) == 0) {
            return signal_names[i].num;
        }
    }

    return -1;
}

const char *trap_signal_name(int signum) {
    for (int i = 0; signal_names[i].name; i++) {
        if (signal_names[i].num == signum) {
            return signal_names[i].name;
        }
    }
    return NULL;
}

// Signal handler that executes trap command
static void trap_signal_handler(int signum) {
    if (signum >= 0 && signum < MAX_TRAPS && traps[signum]) {
        // Execute the trap command
        int result = script_execute_string(traps[signum]);

        // If errexit triggered in trap handler (exit code != 0 with -e),
        // the return value will be non-zero. Check if shell should exit.
        extern int last_command_exit_code;
        if (shell_option_errexit() && last_command_exit_code != 0) {
            // Errexit triggered in trap - exit the shell
            exit(last_command_exit_code);
        }
        (void)result;  // Suppress unused warning
    }
}

int trap_set(const char *action, const char *signal_name) {
    int signum = trap_parse_signal(signal_name);
    if (signum < 0 || signum >= MAX_TRAPS) {
        fprintf(stderr, "%s: trap: %s: invalid signal specification\n",
                HASH_NAME, signal_name);
        return -1;
    }

    // Free existing trap
    if (traps[signum]) {
        free(traps[signum]);
        traps[signum] = NULL;
    }

    // Check for reset (NULL, empty, or "-")
    if (!action || action[0] == '\0' || (action[0] == '-' && action[1] == '\0')) {
        // Reset to default
        if (signum > 0) {
            signal(signum, SIG_DFL);
        }
        return 0;
    }

    // Set the trap
    traps[signum] = strdup(action);

    // Install signal handler for non-EXIT signals
    if (signum > 0 && signum != SIGKILL && signum != SIGSTOP) {
        signal(signum, trap_signal_handler);
    }

    return 0;
}

const char *trap_get(int signum) {
    if (signum < 0 || signum >= MAX_TRAPS) {
        return NULL;
    }
    return traps[signum];
}

void trap_execute_exit(void) {
    if (traps[0]) {
        script_execute_string(traps[0]);
    }
}

// Reset traps for subshell - POSIX says traps are not inherited for execution,
// but trap with no operands should show what traps were when subshell was entered
void trap_reset_for_subshell(void) {
    in_subshell = 1;
    for (int i = 0; i < MAX_TRAPS; i++) {
        // Save current trap for display purposes (if not already in nested subshell)
        if (traps[i] && !inherited_traps[i]) {
            inherited_traps[i] = strdup(traps[i]);
        }
        // Clear the active trap
        if (traps[i]) {
            free(traps[i]);
            traps[i] = NULL;
        }
        // Reset signal handlers to default for non-EXIT signals
        // POSIX: Signals set to SIG_IGN must remain ignored (e.g., for background jobs)
        if (i > 0 && i != SIGKILL && i != SIGSTOP) {
            // Check current handler - only reset if not SIG_IGN
            struct sigaction sa;
            if (sigaction(i, NULL, &sa) == 0 && sa.sa_handler != SIG_IGN) {
                signal(i, SIG_DFL);
            }
        }
    }
}

void trap_list(void) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        // Show active traps, or inherited traps if in subshell with no active trap
        const char *action = traps[i];
        if (!action && in_subshell) {
            action = inherited_traps[i];
        }
        if (action) {
            const char *name = trap_signal_name(i);
            if (name) {
                printf("trap -- '%s' %s\n", action, name);
            } else {
                printf("trap -- '%s' %d\n", action, i);
            }
        }
    }
}
