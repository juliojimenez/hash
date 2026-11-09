#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include "trap.h"
#include "hash.h"
#include "script.h"
#include "config.h"
#include "execute.h"

// Trap storage
static char *traps[MAX_TRAPS];

// Inherited traps for display in subshells (POSIX: trap with no operands shows
// the trap commands as they were when the subshell was entered)
static char *inherited_traps[MAX_TRAPS];
static int in_subshell = 0;
// Status the shell is exiting with while the EXIT trap runs, -1 otherwise
static int exit_trap_status = -1;

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

// Signals that arrived ignored. POSIX: a non-interactive shell cannot trap
// or reset them (they stay ignored, quietly), and `trap - SIG` puts a signal
// back to what it was on entry, which for these is ignored. GitHub Actions
// runs every step with SIGPIPE ignored, so a script there must not undo that.
static bool ignored_at_entry[MAX_TRAPS];

void trap_init(void) {
    memset(traps, 0, sizeof(traps));
    for (int i = 1; i < MAX_TRAPS; i++) {
        struct sigaction sa;
        ignored_at_entry[i] = sigaction(i, NULL, &sa) == 0 && sa.sa_handler == SIG_IGN;
    }
}

bool trap_ignored_at_entry(int signum) {
    return signum > 0 && signum < MAX_TRAPS && ignored_at_entry[signum];
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

// A trapped signal is only recorded here; the action runs at the next safe
// point (trap_run_pending(): before a command, after a foreground child or
// `wait` has been reaped, after an interrupted read, before a prompt line).
// Parsing and running it from the handler meant malloc, stdio and fork in
// signal context, which ThreadSanitizer flags and which can corrupt the heap
// when the signal lands inside an allocation (#398).
static volatile sig_atomic_t pending_signals[MAX_TRAPS];
static volatile sig_atomic_t trap_pending_any = 0;
static bool running_trap_action = false;

static void trap_signal_handler(int signum) {
    if (signum > 0 && signum < MAX_TRAPS) {
        pending_signals[signum] = 1;
        trap_pending_any = 1;
    }
}

bool trap_run_pending(void) {
    if (!trap_pending_any || running_trap_action) {
        return false;
    }
    trap_pending_any = 0;
    running_trap_action = true;   // an action's own commands reach these safe points too
    bool exit_asked = false;
    for (int i = 1; i < MAX_TRAPS; i++) {
        if (!pending_signals[i]) continue;
        pending_signals[i] = 0;
        if (!traps[i] || traps[i][0] == '\0') continue;   // reset or ignored meanwhile
        // POSIX: $? after the action is what it was before the action ran
        int saved_status = last_command_exit_code;
        script_execute_string(traps[i]);
        // `exit` in the action, or set -e tripping on one of its commands, ends
        // the shell with the action's status, through the normal exit path so
        // the EXIT trap still runs
        if (script_state.exit_requested) {
            exit_asked = true;
            break;
        }
        last_command_exit_code = saved_status;
    }
    running_trap_action = false;
    return exit_asked;
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

    // Check for reset (NULL or "-")
    if (!action || (action[0] == '-' && action[1] == '\0')) {
        // Back to the disposition the shell started with
        if (signum > 0) {
            signal(signum, ignored_at_entry[signum] ? SIG_IGN : SIG_DFL);
        }
        return 0;
    }

    // Check for ignore (empty string)
    if (action[0] == '\0') {
        // POSIX: trap "" SIGNAL means ignore the signal
        traps[signum] = strdup(action);
        if (signum > 0 && signum != SIGKILL && signum != SIGSTOP) {
            signal(signum, SIG_IGN);
        }
        return 0;
    }

    // A signal ignored on entry stays ignored in a non-interactive shell; the
    // action is accepted and dropped, as bash and dash do
    if (signum > 0 && ignored_at_entry[signum] && !is_interactive) {
        return 0;
    }

    // Set the trap with a handler
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

int trap_running_exit_status(void) {
    return exit_trap_status;
}

int trap_exit_status(int status) {
    // A signal that arrived during the last command has had no safe point
    // yet: its action runs before the shell leaves, and before the EXIT trap
    // (set -e; trap "false; echo BUG" USR1; kill -s USR1 $$ exits 1)
    if (trap_run_pending()) {
        status = last_command_exit_code;   // the action asked to exit: with its status
    }
    if (!traps[0]) {
        return status;
    }

    // The trap action runs with $? = status; an exit it requests itself is
    // told apart from the exit that brought us here
    bool was_requested = script_state.exit_requested;
    script_state.exit_requested = false;
    last_command_exit_code = status;
    exit_trap_status = status;

    script_execute_string(traps[0]);

    exit_trap_status = -1;
    int final_status = script_state.exit_requested ? last_command_exit_code : status;
    script_state.exit_requested = was_requested || script_state.exit_requested;
    return final_status;
}

// Reset traps for subshell - POSIX says traps are not inherited for execution,
// but trap with no operands should show what traps were when subshell was entered
void trap_reset_for_subshell(void) {
    in_subshell = 1;
    // Signals that arrived before the fork belong to the parent
    for (int i = 0; i < MAX_TRAPS; i++) {
        pending_signals[i] = 0;
    }
    trap_pending_any = 0;
    running_trap_action = false;
    // A subshell started inside an EXIT trap action is a new environment: a
    // bare exit in it uses the subshell's own last status, not the trap's
    exit_trap_status = -1;
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
