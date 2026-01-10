#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include "trap.h"
#include "hash.h"
#include "script.h"

// Trap storage
static char *traps[MAX_TRAPS];

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
        script_execute_string(traps[signum]);
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

// Reset traps for subshell - POSIX says traps are not inherited
void trap_reset_for_subshell(void) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (traps[i]) {
            free(traps[i]);
            traps[i] = NULL;
        }
        // Reset signal handlers to default for non-EXIT signals
        if (i > 0 && i != SIGKILL && i != SIGSTOP) {
            signal(i, SIG_DFL);
        }
    }
}

void trap_list(void) {
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (traps[i]) {
            const char *name = trap_signal_name(i);
            if (name) {
                printf("trap -- '%s' %s\n", traps[i], name);
            } else {
                printf("trap -- '%s' %d\n", traps[i], i);
            }
        }
    }
}
