#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#if defined(__GLIBC__)
#include <stdio_ext.h>   // __fpurge
#else
int fpurge(FILE *);     // BSD libcs (FreeBSD, macOS): hidden by the strict feature macros above
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>
#include <signal.h>
#include "exec.h"
#include "completion.h"
#include "hash.h"
#include "input.h"
#include "builtins.h"
#include "expand.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "execute.h"
#include "history.h"
#include "jobs.h"
#include "test_builtin.h"
#include "script.h"
#include "parser.h"
#include "safe_string.h"
#include "update.h"
#include "shellvar.h"
#include "trap.h"
#include "utils.h"

extern int last_command_exit_code;

// Track if this is a login shell (set from main)
static bool is_login_shell = false;

void builtins_set_login_shell(bool is_login) {
    is_login_shell = is_login;
}

// Builtin command names and functions
Builtin builtins[BUILTIN_FUNC_MAX] = {
    [BUILTIN_FUNC_CD] = (Builtin){"cd", &shell_cd,
        "cd [-L|-P] [dir]",
        "Change the working directory to dir, or to $HOME without one. -L, the default,\n"
        "keeps the logical path, so `cd ..` under a symlinked directory goes to the\n"
        "parent of the link; -P follows symlinks and sets PWD to the physical path.\n"
        "`cd -` goes back to the previous directory and prints it. PWD and OLDPWD are\n"
        "updated.\n"
        ""},
    [BUILTIN_FUNC_EXIT] = (Builtin){"exit", &shell_exit,
        "exit [n]",
        "Leave the shell with status n, or with the status of the last command. The\n"
        "EXIT trap runs first; a login shell also runs ~/.hash_logout.\n"
        ""},
    [BUILTIN_FUNC_ALIAS] = (Builtin){"alias", &shell_alias,
        "alias [name[=value]...]",
        "Define aliases, or print them: `alias` lists every alias, `alias name` prints\n"
        "one. An alias is expanded when its name is the first word of a command.\n"
        ""},
    [BUILTIN_FUNC_UNALIAS] = (Builtin){"unalias", &shell_unalias,
        "unalias [-a] name...",
        "Remove the named aliases, or every alias with -a. A name that is not an\n"
        "alias is reported and the status is 1; the other names are still removed.\n"
        ""},
    [BUILTIN_FUNC_SOURCE] = (Builtin){"source", &shell_source,
        "source file",
        "Read and run file in the current shell, like `.`. Functions, variables and\n"
        "aliases it defines stay. A file name without a slash is looked up in PATH.\n"
        ""},
    [BUILTIN_FUNC_DOT] = (Builtin){".", &shell_source,
        ". file",
        "Read and run file in the current shell (POSIX spelling of source). `return`\n"
        "ends the file; `exit` ends the shell.\n"
        ""},
    [BUILTIN_FUNC_EXPORT] = (Builtin){"export", &shell_export,
        "export [-p] [name[=value]...]",
        "Mark variables for export to the environment of commands, assigning a value\n"
        "when one is given. -p, or no arguments, prints the exported variables.\n"
        ""},
    [BUILTIN_FUNC_SET] = (Builtin){"set", &shell_set,
        "set [-abCefhmnuvx] [-o option] [+...] [--] [arg...]\n"
        "set name=value",
        "Set shell options with -, clear them with +: -a allexport, -b notify,\n"
        "-C noclobber, -e errexit, -f noglob, -h, -m monitor, -n noexec, -u nounset,\n"
        "-v verbose, -x xtrace; -o takes a name, also emacs, vi, nolog and\n"
        "nonlexicalctrl. `set -o` lists the options, `set +o` prints them as\n"
        "commands, $- holds the letters in effect. Arguments after the options (or\n"
        "after --) become the positional parameters.\n"
        "hash's own settings use name=value: prompt=NAME picks a prompt preset\n"
        "(classic, plain, minimal, two-line, powerline; prompt=list shows them),\n"
        "PS1=... builds one, and colors=on|off, welcome=on|off, color.<element>=<spec>,\n"
        "syntax_highlight=on|off, autosuggest=on|off, danger_highlight=on|off. They\n"
        "work at the prompt, in scripts and in ~/.hashrc.\n"
        ""},
    [BUILTIN_FUNC_HISTORY] = (Builtin){"history", &shell_history,
        "history [-c | -w | -r]",
        "Print the command history with its numbers. -c clears it, -w writes it to\n"
        "the history file, -r reads the file back. HISTFILE, HISTSIZE, HISTFILESIZE\n"
        "and HISTCONTROL apply; `!!`, `!n`, `!-n` and `!prefix` expand entries.\n"
        ""},
    [BUILTIN_FUNC_JOBS] = (Builtin){"jobs", &shell_jobs,
        "jobs [-l | -p]",
        "List the background jobs of this shell. -l adds the process IDs, -p prints\n"
        "only them.\n"
        ""},
    [BUILTIN_FUNC_FG] = (Builtin){"fg", &shell_fg,
        "fg [%job]",
        "Bring a job to the foreground and wait for it. Without an argument, the\n"
        "current job.\n"
        ""},
    [BUILTIN_FUNC_BG] = (Builtin){"bg", &shell_bg,
        "bg [%job]",
        "Continue a stopped job in the background.\n"
        ""},
    [BUILTIN_FUNC_LOGOUT] = (Builtin){"logout", &shell_logout,
        "logout",
        "Leave a login shell, running ~/.hash_logout first. In a shell that is not a\n"
        "login shell it fails; use exit.\n"
        ""},
    [BUILTIN_FUNC_TEST] = (Builtin){"test", &shell_test,
        "test expr",
        "Evaluate a conditional expression: file tests (-e -f -d -r -w -x -s -L and\n"
        "more), string tests (-z -n = !=), numbers (-eq -ne -lt -le -gt -ge), and !\n"
        "-a -o with parentheses. The status is 0 when expr is true. `[ expr ]` is the\n"
        "same command with a closing bracket.\n"
        ""},
    [BUILTIN_FUNC_BRACKET] = (Builtin){"[", &shell_bracket,
        "[ expr ]",
        "The test command written with brackets; the closing ] is required. See\n"
        "`help test` for the operators.\n"
        ""},
    [BUILTIN_FUNC_DOUBLE_BRACKET] = (Builtin){"[[", &shell_double_bracket,
        "[[ expr ]]",
        "Extended test: the operators of test plus pattern matching with == and !=,\n"
        "regular expressions with =~, and && and || inside the brackets. Words are\n"
        "not split or glob-expanded.\n"
        ""},
    [BUILTIN_FUNC_UNSET] = (Builtin){"unset", &shell_unset,
        "unset [-f | -v] name...",
        "Remove variables (-v, the default) or functions (-f). Readonly variables\n"
        "cannot be unset.\n"
        ""},
    [BUILTIN_FUNC_TRUE] = (Builtin){"true", &shell_true,
        "true",
        "Do nothing, successfully.\n"
        ""},
    [BUILTIN_FUNC_FALSE] = (Builtin){"false", &shell_false,
        "false",
        "Do nothing, with status 1.\n"
        ""},
    [BUILTIN_FUNC_COLON] = (Builtin){":", &shell_colon,
        ": [arg...]",
        "Do nothing; the arguments are expanded and dropped. Status 0.\n"
        ""},
    [BUILTIN_FUNC_ECHO] = (Builtin){"echo", &shell_echo,
        "echo [-neE] [arg...]",
        "Print the arguments separated by spaces, then a newline. -n leaves out the\n"
        "newline, -e interprets backslash escapes (\\n \\t \\\\ ...), -E does not.\n"
        ""},
    [BUILTIN_FUNC_READ] = (Builtin){"read", &shell_read,
        "read [-r] name...",
        "Read one line from standard input and split it into the variables; the last\n"
        "one takes the rest. Backslash continues a line unless -r is given. The\n"
        "status is 1 at end of input.\n"
        ""},
    [BUILTIN_FUNC_GETOPTS] = (Builtin){"getopts", &shell_getopts,
        "getopts optstring name [arg...]",
        "Parse options from the positional parameters (or arg...) one call at a time,\n"
        "placing the letter in name, its argument in OPTARG and the next index in\n"
        "OPTIND. A colon after a letter in optstring means it takes an argument; a\n"
        "leading colon selects silent error reporting.\n"
        ""},
    [BUILTIN_FUNC_SHIFT] = (Builtin){"shift", &shell_shift,
        "shift [n]",
        "Drop the first n positional parameters (one by default). A count past $# is\n"
        "an error.\n"
        ""},
    [BUILTIN_FUNC_LOCAL] = (Builtin){"local", &shell_local,
        "local [name[=value]...]",
        "Declare variables local to the current function; the previous values come\n"
        "back when it returns. Outside a function it is an error.\n"
        ""},
    [BUILTIN_FUNC_COMPLETE] = (Builtin){"complete", &shell_complete,
        "complete [-abcdfuv] [-o default|dirnames] [-W words] [-F function] name...\n"
        "complete -p [name...]   complete -r [name...]",
        "Record how a command's arguments are tab-completed. -W gives the candidate\n"
        "words, -F a function whose output lines are candidates ($1 command, $2 word,\n"
        "$3 previous word), -a -b -c -d -f -u -v add aliases, builtins, commands,\n"
        "directories, files, users or variables, -o default falls back to files.\n"
        "-p prints the definitions in a form that can be typed back, -r removes them.\n"
        ""},
    [BUILTIN_FUNC_RETURN] = (Builtin){"return", &shell_return,
        "return [n]",
        "Leave the current function or sourced file with status n, or with the status\n"
        "of the last command.\n"
        ""},
    [BUILTIN_FUNC_BREAK] = (Builtin){"break", &shell_break,
        "break [n]",
        "Leave the innermost n enclosing loops (one by default).\n"
        ""},
    [BUILTIN_FUNC_CONTINUE] = (Builtin){"continue", &shell_continue_cmd,
        "continue [n]",
        "Resume the next iteration of the innermost n enclosing loops.\n"
        ""},
    [BUILTIN_FUNC_EVAL] = (Builtin){"eval", &shell_eval,
        "eval [arg...]",
        "Join the arguments with spaces and run the result as a command in the\n"
        "current shell.\n"
        ""},
    [BUILTIN_FUNC_UPDATE] = (Builtin){"update", &shell_update,
        "update",
        "Check for a newer release of hash and install it when one is available.\n"
        "HASH_DISABLE_UPDATE_CHECK=1 turns the startup check off.\n"
        ""},
    [BUILTIN_FUNC_COMMAND] = (Builtin){"command", &shell_command,
        "command name [arg...]\n"
        "command -v | -V name...",
        "Run name without looking up functions, so a function can call the command\n"
        "it shadows; a failing special builtin does not end the shell, but break,\n"
        "continue, return and exit keep their effect. -v prints how name would be\n"
        "resolved, -V describes it.\n"
        ""},
    [BUILTIN_FUNC_EXEC] = (Builtin){"exec", &shell_exec,
        "exec [command [arg...]]\n"
        "exec redirection...",
        "Replace the shell with command. With only redirections, apply them to the\n"
        "shell itself for the commands that follow (`exec 3< file`, `exec >log`).\n"
        ""},
    [BUILTIN_FUNC_TIMES] = (Builtin){"times", &shell_times,
        "times",
        "Print the user and system times used by the shell and by its children.\n"
        ""},
    [BUILTIN_FUNC_TYPE] = (Builtin){"type", &shell_type,
        "type name...",
        "Say how each name would be interpreted: an alias, a shell keyword, a builtin,\n"
        "a function, or a file found in PATH.\n"
        ""},
    [BUILTIN_FUNC_READONLY] = (Builtin){"readonly", &shell_readonly,
        "readonly [-p] [name[=value]...]",
        "Mark variables read-only, assigning a value when one is given; later\n"
        "assignments and unset fail. -p, or no arguments, prints them.\n"
        ""},
    [BUILTIN_FUNC_TRAP] = (Builtin){"trap", &shell_trap,
        "trap [action] [signal...]\n"
        "trap -l | -p",
        "Run action when a signal arrives, or at EXIT. An empty action ignores the\n"
        "signal, `-` restores the default. -l lists the signal names, -p prints the\n"
        "traps in effect. The EXIT trap sees the shell's status as $?.\n"
        ""},
    [BUILTIN_FUNC_WAIT] = (Builtin){"wait", &shell_wait,
        "wait [pid | %job ...]",
        "Wait for the given background jobs, or for all of them, and return the\n"
        "status of the last one waited for.\n"
        ""},
    [BUILTIN_FUNC_KILL] = (Builtin){"kill", &shell_kill,
        "kill [-s signal | -signal] pid | %job ...\n"
        "kill -l",
        "Send a signal (TERM by default) to processes or jobs, by number or name.\n"
        "-l lists the signal names.\n"
        ""},
    [BUILTIN_FUNC_UMASK] = (Builtin){"umask", &shell_umask,
        "umask [-S] [mode]",
        "Show or set the file creation mask. Without a mode, print it in octal,\n"
        "or with -S as u=rwx,g=rx,o=rx. A mode is octal (022) or symbolic\n"
        "(u+r, g-w,o=, a=rx) as for chmod.\n"
        ""},
    [BUILTIN_FUNC_HASH] = (Builtin){"hash", &shell_hash,
        "hash [-r] [name...]",
        "Remember where commands live. With names, look them up now; -r forgets\n"
        "everything remembered; alone, print the table.\n"
        ""},
    [BUILTIN_FUNC_HELP] = (Builtin){"help", &shell_help,
        "help [-s] [pattern...]",
        "Describe the builtins and the shell's keywords. Alone, list every topic with\n"
        "its synopsis; with names or glob patterns, print the synopsis and a short\n"
        "description of each; -s prints the synopsis only. The book has the long\n"
        "form: https://hash-shell.org/reference/\n"
        ""},
};

// Parse job ID from argument (handles %n, %%, %+, %-, n)
static int parse_job_id(const char *arg) {
    if (!arg) return 0;  // No arg means current job

    while (isspace(*arg)) arg++;

    if (*arg == '%') {
        arg++;
        if (char_in_string(*arg, "\0%+")) {
            return 0;
        } else if (*arg == '-') {
            return 0;
        } else if (isdigit(*arg)) {
            return atoi(arg);
        }
    } else if (isdigit(*arg)) {
        return atoi(arg);
    }

    return -1;
}

// ============================================================================
// Core Builtins
// ============================================================================

// $PWD when it still names the current directory, else the physical path.
// The logical path is what cd -L builds on; a stale or missing PWD (the shell
// was started with a wrong one, or the directory moved) falls back to getcwd().
static const char *cd_current_dir(char *buf, size_t bufsz) {
    const char *pwd = shellvar_get("PWD");
    struct stat here, named;
    if (pwd && pwd[0] == '/' && stat(pwd, &named) == 0 && stat(".", &here) == 0 &&
        named.st_dev == here.st_dev && named.st_ino == here.st_ino) {
        return pwd;
    }
    return getcwd(buf, bufsz);
}

// The path cd -L changes to, built the way POSIX describes: an absolute operand
// stands, a relative one goes under the logical current directory, "." drops
// out, and each ".." removes the component before it after checking that the
// prefix is a directory (so `cd ..` from a symlinked directory goes to the
// parent of the link, not of its target). Returns 0, or -1 with errno set.
static int cd_logical_path(const char *dir, char *out, size_t outsz) {
    char base[PATH_MAX];
    const char *from = "";
    if (dir[0] != '/') {
        from = cd_current_dir(base, sizeof(base));
        if (!from) {
            return -1;
        }
    }

    size_t olen = 1;
    out[0] = '/';
    out[1] = '\0';

    // Walk the components of "<from>/<dir>" without building that string
    const char *parts[2] = {from, dir};
    for (int k = 0; k < 2; k++) {
        const char *p = parts[k];
        while (*p) {
            while (*p == '/') p++;
            if (!*p) break;
            const char *comp = p;
            while (*p && *p != '/') p++;
            size_t n = (size_t)(p - comp);

            if (n == 1 && comp[0] == '.') {
                continue;
            }
            if (n == 2 && comp[0] == '.' && comp[1] == '.') {
                struct stat st;
                if (stat(out, &st) != 0) {
                    return -1;
                }
                if (!S_ISDIR(st.st_mode)) {
                    errno = ENOTDIR;
                    return -1;
                }
                if (olen > 1) {
                    char *slash = strrchr(out, '/');
                    if (slash == out) {
                        out[1] = '\0';
                        olen = 1;
                    } else {
                        *slash = '\0';
                        olen = (size_t)(slash - out);
                    }
                }
                continue;
            }
            if (olen + (olen > 1 ? 1 : 0) + n + 1 > outsz) {
                errno = ENAMETOOLONG;
                return -1;
            }
            if (olen > 1) {
                out[olen++] = '/';
            }
            memcpy(out + olen, comp, n);
            olen += n;
            out[olen] = '\0';
        }
    }
    return 0;
}

int shell_cd(char **args) {
    bool physical = false;   // -L is the default, the last of -L/-P wins

    int i = 1;
    while (args[i] && args[i][0] == '-' && args[i][1] != '\0') {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        for (const char *o = args[i] + 1; *o; o++) {
            if (*o == 'L') {
                physical = false;
            } else if (*o == 'P') {
                physical = true;
            } else {
                color_error("%s: cd: -%c: invalid option", HASH_NAME, *o);
                fprintf(stderr, "cd: usage: cd [-L|-P] [dir]\n");
                last_command_exit_code = 2;
                return 1;
            }
        }
        i++;
    }

    const char *dir = args[i];   // further operands are ignored, as dash does
    bool print = false;

    if (dir == NULL) {
        dir = shellvar_get("HOME");
        if (!dir) {
            struct passwd pw;
            struct passwd *result = NULL;
            char buf[1024];

            if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result != NULL) {
                dir = pw.pw_dir;
            } else {
                color_error("%s: cd: could not determine home directory", HASH_NAME);
                last_command_exit_code = 1;
                return 1;
            }
        }
    } else if (strcmp(dir, "-") == 0) {
        dir = shellvar_get("OLDPWD");
        if (!dir) {
            color_error("%s: cd: OLDPWD not set", HASH_NAME);
            last_command_exit_code = 1;
            return 1;
        }
        print = true;   // POSIX: cd - prints the new working directory
    }

    // The directory we leave, as $PWD names it, becomes OLDPWD
    char oldbuf[PATH_MAX];
    const char *oldpwd = cd_current_dir(oldbuf, sizeof(oldbuf));

    char target[PATH_MAX];
    if (physical) {
        if (chdir(dir) != 0 || getcwd(target, sizeof(target)) == NULL) {
            color_error("%s: cd: %s: %s", HASH_NAME, dir, strerror(errno));
            last_command_exit_code = 1;
            return 1;
        }
    } else {
        if (cd_logical_path(dir, target, sizeof(target)) != 0 || chdir(target) != 0) {
            color_error("%s: cd: %s: %s", HASH_NAME, dir, strerror(errno));
            last_command_exit_code = 1;
            return 1;
        }
    }

    // Through the variable table, not setenv() alone: the environment is
    // imported into the table at startup and $PWD reads the table first, so a
    // plain setenv() left $PWD at the directory the shell started in.
    if (oldpwd) {
        shellvar_set("OLDPWD", oldpwd);
        shellvar_set_export("OLDPWD");
    }
    shellvar_set("PWD", target);
    shellvar_set_export("PWD");
    if (print) {
        printf("%s\n", target);
    }
    last_command_exit_code = 0;
    return 1;
}

int shell_exit(char **args) {
    // POSIX: exit without an operand exits with the last command's status;
    // inside the EXIT trap, with the status the shell was already exiting with
    int exit_code = trap_running_exit_status();
    if (exit_code < 0) {
        exit_code = last_command_exit_code;
    }

    // Check for exit code argument
    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "%s: exit: %s: numeric argument required\n", HASH_NAME, args[1]);
            exit_code = 2;
        } else {
            exit_code = (int)(val & 0xFF);  // Exit codes are 0-255
        }
    }

    // Check for running jobs (only relevant in interactive mode)
    int job_count = jobs_count();
    if (job_count > 0 && isatty(STDIN_FILENO)) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'exit' again to force exit, or 'jobs' to see them.\n");

        static int exit_attempted = 0;
        if (exit_attempted) {
            exit_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = exit_code;
            script_state.exit_requested = true;
            return 0;
        }
        exit_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    // Only print goodbye message in interactive mode
    if (isatty(STDIN_FILENO)) {
        fprintf(stdout, "Bye :)\n");
    }
    last_command_exit_code = exit_code;
    script_state.exit_requested = true;  // Mark that exit was explicitly called
    return 0;
}

// Drop what stdout still holds after a failed flush. glibc discards the data
// itself; the BSD libcs (FreeBSD, macOS) keep it for a retry, so the failed
// listing would come out with the next command's output once the redirection
// is gone.
static void discard_stdout_buffer(void) {
#if defined(__GLIBC__)
    __fpurge(stdout);
#else
    fpurge(stdout);
#endif
}

// The status of a builtin that printed a listing: 0, or 1 with a message when
// the output could not be written (stdout on /dev/full, a closed descriptor).
// echo and printf already answer that way; a listing that said nothing about
// a failed write made `export >/dev/full` look like it had succeeded.
static int listing_status(const char *name) {
    if (fflush(stdout) == EOF || ferror(stdout)) {
        fprintf(stderr, "%s: %s: write error: %s\n", HASH_NAME, name, strerror(errno));
        discard_stdout_buffer();
        clearerr(stdout);
        last_command_exit_code = 1;
    } else {
        last_command_exit_code = 0;
    }
    return 1;
}

int shell_alias(char **args) {
    if (args[1] == NULL) {
        config_list_aliases();
        return listing_status("alias");
    }

    char *equals = strchr(args[1], '=');
    if (equals) {
        *equals = '\0';
        const char *name = args[1];
        // The shell has already removed the quotes around the value; any
        // quote characters left in it belong to the alias text
        const char *value = equals + 1;

        if (config_add_alias(name, value) == 0) {
            last_command_exit_code = 0;
        } else {
            color_error("Failed to add alias");
            last_command_exit_code = 1;
        }
    } else {
        const char *value = config_get_alias(args[1]);
        if (value) {
            printf("alias %s='%s'\n", args[1], value);
            return listing_status("alias");
        } else {
            fprintf(stderr, "%s: alias: %s: not found\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
        }
    }

    return 1;
}

int shell_unalias(char **args) {
    // unalias [-a] name...   (POSIX): -a removes every alias; otherwise each
    // name is removed, a name that is not an alias is reported and the status
    // is 1, and the others are still removed.
    bool all = false;
    int i = 1;
    while (args[i] && args[i][0] == '-' && args[i][1] != '\0') {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        for (const char *o = args[i] + 1; *o; o++) {
            if (*o == 'a') {
                all = true;
            } else {
                fprintf(stderr, "%s: unalias: -%c: invalid option\n", HASH_NAME, *o);
                fprintf(stderr, "unalias: usage: unalias [-a] name [name ...]\n");
                last_command_exit_code = 2;
                return 1;
            }
        }
        i++;
    }

    if (all) {
        config_clear_aliases();
        last_command_exit_code = 0;
        return 1;
    }

    if (args[i] == NULL) {
        fprintf(stderr, "unalias: usage: unalias [-a] name [name ...]\n");
        last_command_exit_code = 2;
        return 1;
    }

    int status = 0;
    for (; args[i]; i++) {
        if (config_remove_alias(args[i]) != 0) {
            fprintf(stderr, "%s: unalias: %s: not found\n", HASH_NAME, args[i]);
            status = 1;
        }
    }
    last_command_exit_code = status;
    return 1;
}

int shell_source(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "%s: %s: filename argument required\n", HASH_NAME, args[0]);
        last_command_exit_code = 2;
        return 1;
    }

    const char *filepath = args[1];
    char resolved_path[PATH_MAX];

    // POSIX: If filename doesn't contain '/', search PATH for it
    // Note: POSIX says to use PATH only, NOT to check CWD first
    if (strchr(filepath, '/') == NULL) {
        // Use shellvar_get to see shell variables (which may not be in env)
        const char *path_env = shellvar_get("PATH");
        bool found = false;

        if (path_env && *path_env) {
            char *path_copy = strdup(path_env);
            if (path_copy) {
                char *saveptr;
                char *dir = strtok_r(path_copy, ":", &saveptr);

                while (dir) {
                    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", dir, filepath);
                    // Check if file exists and is readable
                    if (access(resolved_path, R_OK) == 0) {
                        filepath = resolved_path;
                        found = true;
                        break;
                    }
                    dir = strtok_r(NULL, ":", &saveptr);
                }
                free(path_copy);
            }
        }

        if (!found) {
            // File not found in PATH
            fprintf(stderr, "%s: %s: not found\n", args[0], args[1]);
            last_command_exit_code = 1;
            // Special builtin: non-interactive shell should exit on error
            return is_interactive ? 1 : 0;
        }
    } else {
        // Filepath contains '/' - check if it exists
        if (access(filepath, R_OK) != 0) {
            fprintf(stderr, "%s: %s: not found\n", args[0], args[1]);
            last_command_exit_code = 1;
            // Special builtin: non-interactive shell should exit on error
            return is_interactive ? 1 : 0;
        }
    }

    // Execute the script file
    // If we're already in silent mode (e.g., sourcing system files),
    // continue in silent mode for nested sources
    // Note: break/continue have lexical scope - they don't propagate from sourced files
    // The caller's exit flag may already be set (an EXIT trap runs with it on),
    // so only an exit requested by this file counts as one
    bool old_exit_requested = script_state.exit_requested;
    script_state.exit_requested = false;
    int result = script_execute_file_ex(filepath, 0, NULL, script_state.silent_errors);
    last_command_exit_code = result;

    // POSIX: an exit in the sourced file exits the shell
    if (script_state.exit_requested) {
        return 0;
    }
    script_state.exit_requested = old_exit_requested;
    if (exec_abort_pending()) {
        return 0;  // the nesting limit was hit inside: keep unwinding
    }
    return 1;
}

int shell_export(char **args) {
    if (args[1] == NULL) {
        shellvar_list_exported();
        return listing_status("export");
    }

    // Handle export -p option (same as export with no args)
    if (strcmp(args[1], "-p") == 0) {
        shellvar_list_exported();
        return listing_status("export");
    }

    for (int i = 1; args[i] != NULL; i++) {
        char *equals = strchr(args[i], '=');
        if (!equals) {
            // Just marking for export - mark variable for export
            if (shellvar_is_readonly(args[i])) {
                // It's OK to export a readonly variable
            }
            shellvar_set_export(args[i]);
            last_command_exit_code = 0;
            continue;
        }

        *equals = '\0';
        const char *name = args[i];
        const char *value = equals + 1;

        // Check if readonly
        if (shellvar_is_readonly(name)) {
            fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, name);
            last_command_exit_code = 1;
            *equals = '=';  // Restore for next iteration
            return 0;  // Exit shell in non-interactive mode per POSIX
        }

        // Set and export the variable
        if (shellvar_set(name, value) == 0) {
            shellvar_set_export(name);
            setenv(name, value, 1);
            last_command_exit_code = 0;
        } else {
            last_command_exit_code = 1;
            *equals = '=';
            return 0;  // Exit shell on error
        }
        *equals = '=';  // Restore
    }

    return 1;
}

int shell_history(char **args) {
    if (args[1] != NULL) {
        if (strcmp(args[1], "-c") == 0) {
            history_clear();
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-w") == 0) {
            history_save();
            last_command_exit_code = 0;
            return 1;
        } else if (strcmp(args[1], "-r") == 0) {
            history_clear();
            history_load();
            last_command_exit_code = 0;
            return 1;
        }
    }

    int count = history_count();
    for (int i = 0; i < count; i++) {
        const char *cmd = history_get(i);
        if (cmd) {
            printf("%5d  %s\n", i, cmd);
        }
    }

    last_command_exit_code = 0;
    return 1;
}

static void handle_positional_arguments(char **args) {
    int argc = 0;
    // Count arguments
    for (argc = 0; args[argc] != NULL; ++argc);

    // Set positional parameters ($1, $2, ...)
    script_set_positional_params(argc, argc > 0 ? args : NULL);
    last_command_exit_code = 0;
}

// Handle POSIX shell options: -u, +u, -m, +m, -o option, +o option, etc.
// Returns true if arg was handled and last_command_exit_code was set.

// Returns true if arg was handled and last_command_exit_code was set.
static bool handle_hash_shell_options(const char *arg) {
    // name=value options shared with ~/.hashrc (colors, welcome, PS1, prompt,
    // color.<element>, syntax_highlight, autosuggest, danger_highlight).
    // config_set_option(): 0 applied, 1 ours but rejected, -1 not ours.
    int rc = config_set_option(arg);
    if (rc < 0) {
        return false;
    }
    last_command_exit_code = rc;
    return true;
}

int shell_set(char **args) {
    // Handle set with no arguments - list all shell variables
    if (args[1] == NULL) {
        shellvar_list_all();
        return listing_status("set");
    }

    // Handle set option=value syntax for hash-specific options
    int status = 0;
    bool printed_listing = false;   // set -o / set +o: the write must have succeeded
    for (int i = 1; args[i] != NULL; i++) {
        const char *arg = args[i];

        // Handle -- to set positional parameters
        // set -- arg1 arg2 ... sets $1, $2, etc.
        // set -- (with no args) clears positional parameters
        if (strcmp(arg, "--") == 0) {
            // All remaining arguments are used as positional arguments
            handle_positional_arguments(&args[i + 1]);
            return 1;
        }

        // POSIX: If argument doesn't start with - or +, and isn't an option=value,
        // treat it and all following arguments as positional parameters
        if (!char_in_string(arg[0], "-+") && !strchr(arg, '=')) {
            // arguments from this point are used as positional arguments
            handle_positional_arguments(&args[i]);
            return 1;
        }

        // POSIX options: -x, +x, clusters such as -ex, -o name, +o name, and
        // the listings `set -o` (table) and `set +o` (commands)
        if ((arg[0] == '-' || arg[0] == '+') && arg[1] != '\0') {
            bool on = (arg[0] == '-');
            if (strcmp(arg + 1, "o") == 0) {
                if (!args[i + 1]) {
                    shell_options_print(!on);
                    printed_listing = true;
                    continue;
                }
                const char *name = args[++i];
                if (!shell_option_apply_name(name, on)) {
                    fprintf(stderr, "%s: set: %s: invalid option name\n", HASH_NAME, name);
                    last_command_exit_code = 2;
                    return 1;
                }
                continue;
            }
            for (const char *p = arg + 1; *p; p++) {
                if (!shell_option_apply_letter(*p, on)) {
                    fprintf(stderr, "%s: set: %c%c: invalid option\n", HASH_NAME, arg[0], *p);
                    last_command_exit_code = 2;
                    return 1;
                }
            }
            continue;
        }

        if (handle_hash_shell_options(arg)) {
            if (last_command_exit_code != 0) {
                status = last_command_exit_code;  // a rejected value (set prompt=nosuch) fails the command
            }
            continue;
        }

        // Unknown option - silently ignore for compatibility
        // (bash 'set' has many options we don't support)
    }

    if (status == 0 && printed_listing) {
        return listing_status("set");
    }
    last_command_exit_code = status;
    return 1;
}

// ============================================================================
// Job Control Builtins
// ============================================================================

int shell_jobs(char **args) {
    JobsFormat format = JOBS_FORMAT_DEFAULT;

    // Parse options
    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "-l") == 0) {
            format = JOBS_FORMAT_LONG;
        } else if (strcmp(args[i], "-p") == 0) {
            format = JOBS_FORMAT_PID_ONLY;
        }
    }

    jobs_list(format);
    return listing_status("jobs");
}

int shell_fg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        fprintf(stderr, "%s: fg: %s: no such job\n", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_foreground(job_id);
    last_command_exit_code = (result == -1) ? 1 : result;
    return 1;
}

int shell_bg(char **args) {
    int job_id = parse_job_id(args[1]);

    if (job_id == -1) {
        fprintf(stderr, "%s: bg: %s: no such job\n", HASH_NAME, args[1]);
        last_command_exit_code = 1;
        return 1;
    }

    int result = jobs_background(job_id);
    last_command_exit_code = (result == -1) ? 1 : 0;
    return 1;
}

int shell_logout(char **args) {
    (void)args;

    if (!is_login_shell) {
        fprintf(stderr, "%s: logout: not login shell: use `exit'\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    int job_count = jobs_count();
    if (job_count > 0 && isatty(STDIN_FILENO)) {
        color_warning("There are %d running job(s).", job_count);
        printf("Use 'logout' again to force logout, or 'jobs' to see them.\n");

        static int logout_attempted = 0;
        if (logout_attempted) {
            logout_attempted = 0;
            fprintf(stdout, "Bye :)\n");
            last_command_exit_code = 0;
            script_state.exit_requested = true;
            return 0;
        }
        logout_attempted = 1;
        last_command_exit_code = 1;
        return 1;
    }

    // Only print goodbye message in interactive mode
    if (isatty(STDIN_FILENO)) {
        fprintf(stdout, "Bye :)\n");
    }
    last_command_exit_code = 0;
    // Same shutdown flag as exit, so a logout in a startup file ends the
    // shell before the prompt and a logout in a sourced file ends the shell
    script_state.exit_requested = true;
    return 0;
}

// ============================================================================
// Test and Conditional Builtins
// ============================================================================

int shell_test(char **args) {
    last_command_exit_code = builtin_test(args);
    return 1;
}

int shell_bracket(char **args) {
    last_command_exit_code = builtin_bracket(args);
    return 1;
}

int shell_double_bracket(char **args) {
    last_command_exit_code = builtin_double_bracket(args);
    return 1;
}

// ============================================================================
// Variable Management Builtins
// ============================================================================

int shell_unset(char **args) {
    if (args[1] == NULL) {
        last_command_exit_code = 0;
        return 1;
    }

    bool unset_var = true;
    bool unset_func = false;
    int start = 1;

    // Check for -v (variable) or -f (function) flags
    if (args[1][0] == '-') {
        if (strcmp(args[1], "-v") == 0) {
            unset_var = true;
            unset_func = false;
            start = 2;
        } else if (strcmp(args[1], "-f") == 0) {
            unset_var = false;
            unset_func = true;
            start = 2;
        }
    }

    int error = 0;
    for (int i = start; args[i] != NULL; i++) {
        if (unset_var) {
            // Check for readonly variable
            if (shellvar_unset(args[i]) != 0) {
                // shellvar_unset prints error for readonly
                error = 1;
                // In non-interactive mode, exit shell per POSIX
                // In interactive mode, continue processing
                if (!is_interactive) {
                    last_command_exit_code = 1;
                    return 0;  // Exit shell
                }
            }
        }
        if (unset_func) {
            // TODO: Implement function unset when functions are fully implemented
        }
    }

    last_command_exit_code = error ? 1 : 0;
    return 1;
}

// ============================================================================
// Simple Utility Builtins
// ============================================================================

int shell_true(char **args) {
    (void)args;
    last_command_exit_code = 0;
    return 1;
}

int shell_false(char **args) {
    (void)args;
    last_command_exit_code = 1;
    return 1;
}

int shell_colon(char **args) {
    (void)args;
    last_command_exit_code = 0;
    return 1;
}

int shell_echo(char **args) {
    bool newline = true;
    bool interpret_escapes = false;
    int start = 1;
    int write_error = 0;

    // Handle options
    while (args[start] != NULL && args[start][0] == '-') {
        if (strcmp(args[start], "-n") == 0) {
            newline = false;
            start++;
        } else if (strcmp(args[start], "-e") == 0) {
            interpret_escapes = true;
            start++;
        } else if (strcmp(args[start], "-E") == 0) {
            interpret_escapes = false;
            start++;
        } else {
            break;  // Unknown option, treat as argument
        }
    }

    // Print arguments
    for (int i = start; args[i] != NULL; i++) {
        if (i > start) {
            if (putchar(' ') == EOF) write_error = 1;
        }

        if (interpret_escapes) {
            // Interpret escape sequences
            const char *s = args[i];
            while (*s) {
                if (*s == '\\' && *(s + 1)) {
                    s++;
                    switch (*s) {
                        case 'n': if (putchar('\n') == EOF) write_error = 1; break;
                        case 't': if (putchar('\t') == EOF) write_error = 1; break;
                        case 'r': if (putchar('\r') == EOF) write_error = 1; break;
                        case '\\': if (putchar('\\') == EOF) write_error = 1; break;
                        case 'a': if (putchar('\a') == EOF) write_error = 1; break;
                        case 'b': if (putchar('\b') == EOF) write_error = 1; break;
                        case 'f': if (putchar('\f') == EOF) write_error = 1; break;
                        case 'v': if (putchar('\v') == EOF) write_error = 1; break;
                        case 'c': goto done;  // Stop output
                        default:
                            if (putchar('\\') == EOF) write_error = 1;
                            if (putchar(*s) == EOF) write_error = 1;
                            break;
                    }
                } else {
                    if (putchar(*s) == EOF) write_error = 1;
                }
                s++;
            }
        } else {
            if (printf("%s", args[i]) < 0) write_error = 1;
        }
    }

done:
    if (newline) {
        if (putchar('\n') == EOF) write_error = 1;
    }

    // Check for write errors on flush
    if (fflush(stdout) == EOF) write_error = 1;
    if (write_error) {
        discard_stdout_buffer();
        clearerr(stdout);
    }

    last_command_exit_code = write_error ? 1 : 0;
    return 1;
}

// read assigns a shell variable, as POSIX says: through the variable table,
// which is what $name reads first (a plain setenv() left `for v in ...; read v`
// showing the loop's last value) and which puts the value in the environment
// only when the variable is exported. Returns false for a readonly variable
// (shellvar_set has said so); read then fails.
static bool read_assign(const char *name, const char *value) {
    return shellvar_set(name, value) == 0;
}

int shell_read(char **args) {
    // Basic read implementation
    // Usage: read [-r] [var ...]

    bool raw = false;
    int start = 1;

    if (args[start] && strcmp(args[start], "-r") == 0) {
        raw = true;
        start++;
    }

    // One line from descriptor 0, leaving what follows for the next reader
    char line[4096];
    if (input_read_line(STDIN_FILENO, line, sizeof(line)) <= 0) {
        last_command_exit_code = 1;
        return 1;
    }

    // Remove trailing newline. Without one the input ended inside the line:
    // the words are still assigned but the status says end of file (POSIX)
    size_t len = strlen(line);
    bool at_eof = true;
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        at_eof = false;
    }

    // If no variables specified, use REPLY
    if (args[start] == NULL) {
        last_command_exit_code = read_assign("REPLY", line) ? (at_eof ? 1 : 0) : 1;
        return 1;
    }
    int status = at_eof ? 1 : 0;

    // Split line into words and assign to variables
    char *saveptr;
    // TODO: Use the IFS
    const char *word = strtok_r(line, " \t", &saveptr);
    int i = start;

    while (args[i] != NULL) {
        if (args[i + 1] == NULL) {
            // Last variable gets the rest of the line
            if (word) {
                char rest[4096];
                safe_strcpy(rest, word, sizeof(rest));

                const char *remaining = strtok_r(NULL, "", &saveptr);
                if (remaining) {
                    safe_strcat(rest, " ", sizeof(rest));
                    safe_strcat(rest, remaining, sizeof(rest));
                }
                if (!read_assign(args[i], rest)) status = 1;
            } else {
                if (!read_assign(args[i], "")) status = 1;
            }
        } else {
            if (!read_assign(args[i], word ? word : "")) status = 1;
            if (word) {
                // TODO: Use the IFS
                word = strtok_r(NULL, " \t", &saveptr);
            }
        }
        i++;
    }

    (void)raw;  // TODO: Handle -r flag for backslash escapes

    last_command_exit_code = status;
    return 1;
}

// ============================================================================
// umask
// ============================================================================

// Symbolic mode clauses as for chmod: [ugoa]*([+-=][rwx]*)+ separated by
// commas. Returns false when the text is not a mode.
static bool umask_apply_symbolic(const char *text, mode_t *mask) {
    mode_t m = *mask;
    const char *p = text;
    for (;;) {
        mode_t who = 0;
        for (; *p; p++) {
            if (*p == 'u') who |= 0700;
            else if (*p == 'g') who |= 0070;
            else if (*p == 'o') who |= 0007;
            else if (*p == 'a') who |= 0777;
            else break;
        }
        if (who == 0) {
            who = 0777;
        }
        if (!char_in_string(*p, "+-=") || *p == '\0') {
            return false;
        }
        while (*p && char_in_string(*p, "+-=")) {
            char op = *p++;
            mode_t perm = 0;
            for (; *p; p++) {
                if (*p == 'r') perm |= 0444;
                else if (*p == 'w') perm |= 0222;
                else if (*p == 'x') perm |= 0111;
                else break;
            }
            perm &= who;
            if (op == '+') {
                m &= ~perm;             // grant: clear the mask bits
            } else if (op == '-') {
                m |= perm;              // deny: set the mask bits
            } else {
                m = (m | who) & ~perm;  // exactly these for the named classes
            }
        }
        if (*p == '\0') {
            break;
        }
        if (*p != ',') {
            return false;
        }
        p++;
    }
    *mask = m;
    return true;
}

// u=rwx,g=rx,o=rx: the permissions the mask leaves on
static void umask_symbolic_text(mode_t mask, char *out, size_t size) {
    const char classes[] = {'u', 'g', 'o'};
    size_t o = 0;
    for (int c = 0; c < 3 && o + 8 < size; c++) {
        int shift = 6 - 3 * c;
        if (c > 0) out[o++] = ',';
        out[o++] = classes[c];
        out[o++] = '=';
        if (!((mask >> shift) & 4)) out[o++] = 'r';
        if (!((mask >> shift) & 2)) out[o++] = 'w';
        if (!((mask >> shift) & 1)) out[o++] = 'x';
    }
    out[o] = '\0';
}

int shell_umask(char **args) {
    bool symbolic = false;
    int i = 1;
    while (args[i] && args[i][0] == '-' && args[i][1]) {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        if (strcmp(args[i], "-S") == 0) {
            symbolic = true;
            i++;
            continue;
        }
        fprintf(stderr, "%s: umask: %s: invalid option\n", HASH_NAME, args[i]);
        fprintf(stderr, "umask: usage: umask [-S] [mode]\n");
        last_command_exit_code = 2;
        return 1;
    }

    mode_t current = umask(0);
    umask(current);

    if (args[i] == NULL) {
        if (symbolic) {
            char text[32];
            umask_symbolic_text(current, text, sizeof(text));
            printf("%s\n", text);
        } else {
            printf("%04o\n", (unsigned)current);
        }
        return listing_status("umask");
    }

    const char *mode = args[i];
    mode_t next = current;
    if (isdigit((unsigned char)mode[0])) {
        char *end;
        long n = strtol(mode, &end, 8);
        if (*end != '\0' || n < 0 || n > 0777) {
            fprintf(stderr, "%s: umask: %s: octal number out of range\n", HASH_NAME, mode);
            last_command_exit_code = 1;
            return 1;
        }
        next = (mode_t)n;
    } else if (!umask_apply_symbolic(mode, &next)) {
        fprintf(stderr, "%s: umask: %s: invalid symbolic mode operator\n", HASH_NAME, mode);
        last_command_exit_code = 1;
        return 1;
    }
    umask(next);
    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// Control Flow Builtins
// ============================================================================

int shell_return(char **args) {
    // POSIX: If no argument is provided, use the exit status of the last command
    int return_code = last_command_exit_code;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "%s: return: %s: numeric argument required\n", HASH_NAME, args[1]);
            return_code = 2;
        } else {
            return_code = (int)(val & 0xFF);
        }
    }

    // If not in a function or sourced script, this is an error
    if (!script_state.in_script) {
        fprintf(stderr, "%s: return: can only `return' from a function or sourced script\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    last_command_exit_code = return_code;
    // Set return pending flag for condition evaluation
    script_set_return_pending(true);
    // Signal to caller to return
    return -2;  // Special return code for "return from function"
}

int shell_break(char **args) {
    int levels = 1;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || val < 1) {
            fprintf(stderr, "%s: break: %s: numeric argument required\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
            return 1;
        }
        levels = (int)val;
    }

    // POSIX: Check if we're in a loop (dynamic scoping across function boundaries)
    int available_loops = script_count_loops_at_current_depth();
    if (available_loops == 0) {
        fprintf(stderr, "%s: break: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    // If requesting more levels than available, just break all available
    if (levels > available_loops) {
        levels = available_loops;
    }

    // Set pending break levels for dynamic scoping propagation
    script_set_break_pending(levels);

    last_command_exit_code = 0;
    return -3;  // Special return code for "break from loop"
}

int shell_continue_cmd(char **args) {
    int levels = 1;

    if (args[1] != NULL) {
        char *endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr != '\0' || val < 1) {
            fprintf(stderr, "%s: continue: %s: numeric argument required\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
            return 1;
        }
        levels = (int)val;
    }

    // POSIX: Check if we're in a loop (dynamic scoping across function boundaries)
    int available_loops = script_count_loops_at_current_depth();
    if (available_loops == 0) {
        fprintf(stderr, "%s: continue: only meaningful in a `for', `while', or `until' loop\n", HASH_NAME);
        last_command_exit_code = 0;
        return 1;
    }

    // If requesting more levels than available, just continue the outermost available
    if (levels > available_loops) {
        levels = available_loops;
    }

    // Set pending continue levels for dynamic scoping propagation
    script_set_continue_pending(levels);

    last_command_exit_code = 0;
    return -4;  // Special return code for "continue loop"
}

int shell_eval(char **args) {
    // POSIX: no arguments, or only null arguments, is a successful no-op; text
    // that is only blanks parses to an empty program and counts the same
    bool all_blank = true;
    for (int i = 1; args[i] != NULL && all_blank; i++) {
        for (const char *c = args[i]; *c; c++) {
            if (*c != ' ' && *c != '\t' && *c != '\n') {
                all_blank = false;
                break;
            }
        }
    }
    if (all_blank) {
        last_command_exit_code = 0;
        return 1;
    }

    // Concatenate all arguments with spaces
    size_t total_len = 0;
    for (int i = 1; args[i] != NULL; i++) {
        total_len += strlen(args[i]) + 1;  // +1 for space or null
    }

    char *cmd = malloc(total_len);
    if (!cmd) {
        last_command_exit_code = 1;
        return 1;
    }

    cmd[0] = '\0';
    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) {
            strcat(cmd, " ");
        }
        strcat(cmd, args[i]);
    }

    if (!exec_nesting_enter("eval")) {
        free(cmd);
        return 0;  // abort pending: unwinds as EXEC_ABORT
    }
    // Messages from the text name eval as their source
    const char *old_source = exec_source();
    exec_set_source("eval");
    bool incomplete = false;
    ExecStatus st = exec_string(cmd, &incomplete);
    exec_nesting_leave();
    size_t cmd_lines = 1;
    for (const char *p = cmd; *p; p++) {
        if (*p == '\n' && p[1] != '\0') {
            cmd_lines++;
        }
    }
    free(cmd);
    if (incomplete) {
        exec_report_syntax_error("syntax error: unexpected end of file", cmd_lines);
    }
    exec_set_source(old_source);
    if (incomplete) {
        last_command_exit_code = 2;
        // An interactive shell reports the error and keeps going;
        // POSIX: a syntax error ends a non-interactive shell
        if (is_interactive) {
            return 1;
        }
        script_state.exit_requested = true;
        return 0;
    }
    switch (st) {
        case EXEC_EXIT:
        case EXEC_ABORT:
            return 0;
        case EXEC_RETURN:
            return -2;
        case EXEC_BREAK:
            return -3;
        case EXEC_CONTINUE:
            return -4;
        default:
            return 1;
    }
}

// ============================================================================
// Command Information Builtins
// ============================================================================

// POSIX reserved words (keywords)
static const char *posix_keywords[] = {
    "!", "{", "}", "case", "do", "done", "elif", "else", "esac",
    "fi", "for", "if", "in", "then", "until", "while", NULL
};

// Check if word is a POSIX reserved word
static bool is_posix_keyword(const char *word) {
    for (int i = 0; posix_keywords[i] != NULL; i++) {
        if (strcmp(word, posix_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Check if name is a builtin (helper for command -v/-V)
static bool is_builtin_name(const char *name) {
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return true;
        }
    }
    return false;
}

// Find command in PATH and return full path (caller must free)
char *find_in_path(const char *cmd) {
    // If cmd contains /, it's already a path
    if (strchr(cmd, '/') != NULL) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "/usr/bin:/bin";
    }

    char *path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char *saveptr;
    char *dir = strtok_r(path_copy, ":", &saveptr);

    while (dir) {
        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd);

        if (access(fullpath, X_OK) == 0) {
            free(path_copy);
            return strdup(fullpath);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

static bool find_command(const char *cmd, bool verbose) {
    bool found = false;

    // Check for alias first
    const char *alias_val = config_get_alias(cmd);
    if (alias_val) {
        if (verbose) {
            printf("%s is aliased to '%s'\n", cmd, alias_val);
        } else {
            printf("alias %s='%s'\n", cmd, alias_val);
        }
        found = true;
    }

    // Check for keyword
    if (is_posix_keyword(cmd)) {
        if (verbose) {
            printf("%s is a shell keyword\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for builtin
    if (is_builtin_name(cmd)) {
        if (verbose) {
            printf("%s is a shell builtin\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for function
    if (script_get_function(cmd)) {
        if (verbose) {
            printf("%s is a function\n", cmd);
        } else {
            printf("%s\n", cmd);
        }
        found = true;
    }

    // Check for external command
    if (!found) {
        char *path = find_in_path(cmd);
        if (path) {
            if (verbose) {
                printf("%s is %s\n", cmd, path);
            } else {
                printf("%s\n", path);
            }
            free(path);
            found = true;
        }
    }

    if (!found && verbose) {
        fprintf(stderr, "%s: %s: not found\n", HASH_NAME, cmd);
    }

    last_command_exit_code = found ? 0 : 1;

    return found;
}

int shell_command(char **args) {
    bool opt_v = false;  // -v: print command path
    bool opt_V = false;  // -V: verbose description
    bool opt_p = false;  // -p: use default PATH
    int arg_start = 1;

    // Parse options
    while (args[arg_start] && args[arg_start][0] == '-') {
        const char *opt = args[arg_start];
        if (strcmp(opt, "-v") == 0) {
            opt_v = true;
            arg_start++;
        } else if (strcmp(opt, "-V") == 0) {
            opt_V = true;
            arg_start++;
        } else if (strcmp(opt, "-p") == 0) {
            opt_p = true;
            arg_start++;
        } else if (strcmp(opt, "--") == 0) {
            arg_start++;
            break;
        } else {
            // Unknown option
            break;
        }
    }

    (void)opt_p;  // TODO: implement default PATH

    // No command specified
    if (!args[arg_start]) {
        last_command_exit_code = 0;
        return 1;
    }

    // The arguments still carry their quote markers (execute() leaves them for
    // this builtin): a name is looked up without them, a builtin gets a copy
    // without them, and an external command goes back through execute(), where
    // launch() strips them after parsing redirections, so expr X '<' X keeps
    // its '<' as an argument
    char *cmd = strdup(args[arg_start]);
    if (!cmd) {
        last_command_exit_code = 1;
        return 1;
    }
    strip_quote_markers(cmd);

    // Handle -v option (print command location/type)
    if (opt_v) {
        bool ret = find_command(cmd, false);
        free(cmd);
        if (ret) {
            return listing_status("command");   // found: the line must have been written
        }
        return 1;
    }

    // Handle -V option (verbose description)
    if (opt_V) {
        bool ret = find_command(cmd, true);
        free(cmd);
        if (ret) {
            return listing_status("command");
        }
        return 1;
    }
    free(cmd);

    // Execute command, bypassing functions. First check builtins.
    // POSIX: `command` strips a special builtin of its "an error exits the
    // shell" property, not of what it does: break, continue, return and exit
    // keep their control flow (their -3/-4/-2 and exit codes go back to the
    // executor exactly as a direct call's would), while a builtin that
    // returns 0 to stop the shell on an error is turned into "keep going".
    int count = 0;
    while (args[arg_start + count]) {
        count++;
    }
    char **plain = calloc((size_t)count + 1, sizeof(char *));
    if (!plain) {
        last_command_exit_code = 1;
        return 1;
    }
    for (int i = 0; i < count; i++) {
        plain[i] = strdup(args[arg_start + i]);
        if (!plain[i]) {
            for (int j = 0; j < i; j++) free(plain[j]);
            free(plain);
            last_command_exit_code = 1;
            return 1;
        }
        strip_quote_markers(plain[i]);
    }
    int result = try_builtin(plain);
    for (int i = 0; i < count; i++) free(plain[i]);
    free(plain);
    if (result != -1) {
        if (result == -2 || result == -3 || result == -4) {
            return result;
        }
        if (result == 0 && script_state.exit_requested) {
            return 0;
        }
        // The exit code is already set in last_command_exit_code by the builtin
        return 1;
    }

    // An external command: execute() without the function lookup (a function
    // named like the command is exactly what `command` is for skipping)
    execute_bypass_functions();
    execute(args + arg_start);
    return 1;
}

static bool check_has_command(char **args) {
    // Parse for redirections vs command
    for (int i = 0; args[i]; i++) {
        const char *arg = args[i];
        // Check if this looks like a redirection
        // Patterns: N<file, N>file, N>>file, N<&M, N>&M, <file, >file, etc.
        bool is_redir = false;

        // Check for digit prefix followed by redirection operator
        const char *p = arg;
        while (*p && isdigit(*p)) p++;

        if (char_in_string(*p, "<>")) {
            is_redir = true;
        } else if (p == arg) {
            // No digit prefix, check for bare redirection
            if (char_in_string(*p, "<>")) {
                is_redir = true;
            }
        }

        if (!is_redir) {
            // has command
            return true;
        }
    }

    return false;
}

static int handle_redirections(char **args, bool has_command) {
    for (int i = 0; args[i]; i++) {
        const char *arg = args[i];

        // Parse fd number (default to 0 for input, 1 for output)
        int fd = -1;
        const char *p = arg;

        while (*p && isdigit(*p)) p++;

        if (p > arg) {
            fd = atoi(arg);
        }

        // Handle different redirection types
        if (*p == '<') {
            if (fd < 0) fd = 0;  // Default input fd
            p++;

            if (*p == '&') {
                // N<&M or N<&-
                p++;
                if (*p == '-') {
                    // Close fd
                    close(fd);
                } else {
                    // Duplicate fd
                    int src_fd = atoi(p);
                    if (dup2(src_fd, fd) < 0) {
                        perror(HASH_NAME);
                        last_command_exit_code = 1;
                        return 1;
                    }
                }
            } else {
                // N<file - open file for input
                const char *filename = p;
                if (!*filename && args[i + 1]) {
                    filename = args[++i];
                }
                int new_fd = open(filename, O_RDONLY);
                if (new_fd < 0) {
                    fprintf(stderr, "%s: %s: %s\n", HASH_NAME, filename, strerror(errno));
                    last_command_exit_code = 1;
                    return 1;
                }
                if (new_fd != fd) {
                    dup2(new_fd, fd);
                    close(new_fd);
                }
            }
        } else if (*p == '>') {
            if (fd < 0) fd = 1;  // Default output fd
            p++;

            bool append = false;
            if (*p == '>') {
                append = true;
                p++;
            }

            if (*p == '&') {
                // N>&M or N>&-
                p++;
                if (*p == '-') {
                    close(fd);
                } else {
                    int src_fd = atoi(p);
                    if (dup2(src_fd, fd) < 0) {
                        perror(HASH_NAME);
                        last_command_exit_code = 1;
                        return 1;
                    }
                }
            } else {
                // N>file or N>>file
                const char *filename = p;
                if (!*filename && args[i + 1]) {
                    filename = args[++i];
                }
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int new_fd = open(filename, flags, 0644);
                if (new_fd < 0) {
                    fprintf(stderr, "%s: %s: %s\n", HASH_NAME, filename, strerror(errno));
                    last_command_exit_code = 1;
                    return 1;
                }
                if (new_fd != fd) {
                    dup2(new_fd, fd);
                    close(new_fd);
                }
            }
        } else if (has_command) {
            // This is the command to exec
            break;
        }
    }

    return 0;
}

int shell_exec(char **args) {
    // exec with no arguments: just return success (noop)
    if (!args[1]) {
        last_command_exit_code = 0;
        return 1;
    }

    // Check for redirections only (exec N<file, exec N>file, etc.)
    // These persist for the shell process
    bool has_command = check_has_command(&args[1]);

    // Handle redirections
    int ret = handle_redirections(&args[1], has_command);
    if (ret != 0) {
        return ret;
    }

    // If there's a command, replace the shell
    if (has_command) {
        // Find the command start
        for (int i = 1; args[i]; i++) {
            const char *arg = args[i];
            const char *p = arg;
            while (*p && isdigit(*p)) p++;
            if (*p != '<' && *p != '>') {
                // This is the command
                // Flush all output buffers before replacing the process
                fflush(stdout);
                fflush(stderr);
                execvp(args[i], args + i);
                // If we get here, exec failed: 127 when the command was not
                // found, 126 when it exists but cannot run (POSIX). A
                // non-interactive shell exits; an interactive one reports it
                int err = errno;
                fprintf(stderr, "%s: exec: %s: %s\n", HASH_NAME, args[i],
                        (err == ENOENT && strchr(args[i], '/') == NULL) ? "not found" : strerror(err));
                last_command_exit_code = (err == ENOENT) ? 127 : 126;
                if (is_interactive) {
                    return 1;
                }
                script_state.exit_requested = true;
                return 0;  // Exit shell on exec failure
            }
        }
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_times(char **args) {
    (void)args;

    struct tms times_buf;
    clock_t real_time = times(&times_buf);

    if (real_time == (clock_t)-1) {
        perror(HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) ticks_per_sec = 100;  // Fallback

    // Shell times (user and system)
    long shell_user_sec = times_buf.tms_utime / ticks_per_sec;
    long shell_user_ms = (times_buf.tms_utime % ticks_per_sec) * 1000 / ticks_per_sec;
    long shell_sys_sec = times_buf.tms_stime / ticks_per_sec;
    long shell_sys_ms = (times_buf.tms_stime % ticks_per_sec) * 1000 / ticks_per_sec;

    // Children times (user and system)
    long child_user_sec = times_buf.tms_cutime / ticks_per_sec;
    long child_user_ms = (times_buf.tms_cutime % ticks_per_sec) * 1000 / ticks_per_sec;
    long child_sys_sec = times_buf.tms_cstime / ticks_per_sec;
    long child_sys_ms = (times_buf.tms_cstime % ticks_per_sec) * 1000 / ticks_per_sec;

    // Format output into buffer and use write(2) directly to detect EPIPE immediately
    // (printf/fflush may not detect broken pipe due to buffering)
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%ldm%ld.%03lds %ldm%ld.%03lds\n",
           shell_user_sec / 60, shell_user_sec % 60, shell_user_ms,
           shell_sys_sec / 60, shell_sys_sec % 60, shell_sys_ms);

    if (len < 0 || write(STDOUT_FILENO, buf, (size_t)len) < 0) {
        fprintf(stderr, "%s: times: I/O error\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }

    len = snprintf(buf, sizeof(buf), "%ldm%ld.%03lds %ldm%ld.%03lds\n",
           child_user_sec / 60, child_user_sec % 60, child_user_ms,
           child_sys_sec / 60, child_sys_sec % 60, child_sys_ms);

    if (len < 0 || write(STDOUT_FILENO, buf, (size_t)len) < 0) {
        fprintf(stderr, "%s: times: I/O error\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_type(char **args) {
    // type is equivalent to command -V
    if (!args[1]) {
        last_command_exit_code = 0;
        return 1;
    }

    bool all_found = true;

    for (int i = 1; args[i]; i++) {
        const char *cmd = args[i];
        if (!find_command(cmd, true)) {
            all_found = false;
        }
    }

    if (all_found) {
        return listing_status("type");   // every name resolved; the lines must have been written
    }
    last_command_exit_code = 1;
    return 1;
}

// ============================================================================
// getopts
// ============================================================================

// Position inside an option cluster such as -ab: the character to look at
// next in the argument OPTIND points to. It only means something while OPTIND
// is still the value we left it at; a script that assigns OPTIND (OPTIND=1 to
// parse again) starts a fresh argument.
static int getopts_cluster_pos = 0;
static long getopts_cluster_optind = -1;

static void getopts_set_optind(long optind) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", optind);
    shellvar_set("OPTIND", buf);
    getopts_cluster_optind = optind;
}

static void getopts_report(const char *what, char opt) {
    const char *who = script_get_positional_param(0);
    if (!who || !*who) {
        who = HASH_NAME;
    }
    fprintf(stderr, "%s: %s -- %c\n", who, what, opt);
}

int shell_getopts(char **args) {
    if (!args[1] || !args[2]) {
        fprintf(stderr, "%s: getopts: usage: getopts optstring name [arg ...]\n", HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }
    const char *optstring = args[1];
    const char *name = args[2];
    bool silent = (optstring[0] == ':');
    if (silent) {
        optstring++;
    }

    // What to parse, indexed from 1 like the positional parameters
    char **argv;
    int argc;
    if (args[3]) {
        argv = &args[2];
        argc = 0;
        while (argv[argc + 1]) {
            argc++;
        }
    } else {
        argv = script_state.positional_params;
        argc = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
    }

    const char *optind_str = shellvar_get("OPTIND");
    long optind = optind_str ? strtol(optind_str, NULL, 10) : 1;
    if (optind < 1) {
        optind = 1;
    }
    if (optind != getopts_cluster_optind) {
        getopts_cluster_pos = 0;  // the script moved OPTIND: start that argument afresh
    }

    // End of the options: no more arguments, an operand, or "--"
    if (optind > argc || !argv) {
        shellvar_set(name, "?");
        getopts_set_optind(optind);
        last_command_exit_code = 1;
        return 1;
    }
    const char *arg = argv[optind];
    if (getopts_cluster_pos == 0) {
        if (arg[0] != '-' || arg[1] == '\0') {
            shellvar_set(name, "?");
            getopts_set_optind(optind);
            last_command_exit_code = 1;
            return 1;
        }
        if (strcmp(arg, "--") == 0) {
            shellvar_set(name, "?");
            getopts_set_optind(optind + 1);
            last_command_exit_code = 1;
            return 1;
        }
        getopts_cluster_pos = 1;
    }

    char opt = arg[getopts_cluster_pos];
    const char *spec = (opt == ':') ? NULL : strchr(optstring, opt);
    getopts_cluster_pos++;
    bool more_in_cluster = (arg[getopts_cluster_pos] != '\0');
    char optbuf[2] = {opt, '\0'};

    if (!spec) {
        // Not an option we know
        shellvar_set(name, "?");
        if (silent) {
            shellvar_set("OPTARG", optbuf);
        } else {
            shellvar_unset("OPTARG");
            getopts_report("illegal option", opt);
        }
        if (!more_in_cluster) {
            optind++;
            getopts_cluster_pos = 0;
        }
        getopts_set_optind(optind);
        last_command_exit_code = 0;
        return 1;
    }

    if (spec[1] == ':') {
        // The option takes an argument: the rest of this word, or the next one
        if (more_in_cluster) {
            shellvar_set("OPTARG", arg + getopts_cluster_pos);
            optind++;
        } else if (optind + 1 <= argc) {
            shellvar_set("OPTARG", argv[optind + 1]);
            optind += 2;
        } else {
            optind++;
            getopts_cluster_pos = 0;
            if (silent) {
                shellvar_set(name, ":");
                shellvar_set("OPTARG", optbuf);
            } else {
                shellvar_set(name, "?");
                shellvar_unset("OPTARG");
                getopts_report("option requires an argument", opt);
            }
            getopts_set_optind(optind);
            last_command_exit_code = 0;
            return 1;
        }
        getopts_cluster_pos = 0;
        shellvar_set(name, optbuf);
        getopts_set_optind(optind);
        last_command_exit_code = 0;
        return 1;
    }

    // A flag: no argument, OPTARG unset
    shellvar_set(name, optbuf);
    shellvar_unset("OPTARG");
    if (!more_in_cluster) {
        optind++;
        getopts_cluster_pos = 0;
    }
    getopts_set_optind(optind);
    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// shift
// ============================================================================

int shell_shift(char **args) {
    long n = 1;
    if (args[1]) {
        char *end;
        n = strtol(args[1], &end, 10);
        if (*args[1] == '\0' || *end != '\0' || n < 0) {
            fprintf(stderr, "%s: shift: %s: numeric argument required\n", HASH_NAME, args[1]);
            last_command_exit_code = 1;
            return 1;
        }
    }
    int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
    if (n > count) {
        fprintf(stderr, "%s: shift: shift count out of range\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }
    if (n > 0) {
        // $0 stays; $1..$n go and the rest move down
        for (long i = 1; i <= n; i++) {
            free(script_state.positional_params[i]);
        }
        for (long i = 1; i + n < script_state.positional_count; i++) {
            script_state.positional_params[i] = script_state.positional_params[i + n];
        }
        script_state.positional_count -= (int)n;
    }
    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// local
// ============================================================================

static bool valid_variable_name(const char *name, size_t len) {
    if (len == 0 || !(isalpha((unsigned char)name[0]) || name[0] == '_')) {
        return false;
    }
    for (size_t i = 1; i < len; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) {
            return false;
        }
    }
    return true;
}

int shell_local(char **args) {
    if (script_state.function_call_depth == 0) {
        fprintf(stderr, "%s: local: can only be used in a function\n", HASH_NAME);
        last_command_exit_code = 1;
        return 1;
    }
    int status = 0;
    for (int i = 1; args[i]; i++) {
        const char *arg = args[i];
        const char *eq = strchr(arg, '=');
        size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
        char name[256];
        if (!valid_variable_name(arg, len) || len >= sizeof(name)) {
            fprintf(stderr, "%s: local: `%s': not a valid identifier\n", HASH_NAME, arg);
            status = 1;
            continue;
        }
        memcpy(name, arg, len);
        name[len] = '\0';
        if (shellvar_local(name) != 0) {
            fprintf(stderr, "%s: local: %s: readonly variable\n", HASH_NAME, name);
            status = 1;
            continue;
        }
        if (eq) {
            shellvar_set(name, eq + 1);
        } else {
            shellvar_unset(name);  // declared, no value yet
        }
    }
    last_command_exit_code = status;
    return 1;
}

int shell_readonly(char **args) {
    // readonly with no args: list all readonly variables
    if (args[1] == NULL) {
        shellvar_list_readonly();
        return listing_status("readonly");
    }

    // Handle options
    int start = 1;
    while (args[start] && args[start][0] == '-') {
        if (strcmp(args[start], "-p") == 0) {
            if (args[start + 1] == NULL) {
                shellvar_list_readonly();
                return listing_status("readonly");
            }
            start++;
        } else if (strcmp(args[start], "--") == 0) {
            // End of options
            start++;
            break;
        } else {
            // Unknown option - just skip it for now
            start++;
        }
    }

    // Process each argument
    for (int i = start; args[i] != NULL; i++) {
        const char *arg = args[i];
        char *equals = strchr(arg, '=');

        if (equals) {
            // readonly name=value: set and mark as readonly
            *equals = '\0';
            const char *name = arg;
            const char *value = equals + 1;

            // Check if already readonly with different value
            if (shellvar_is_readonly(name)) {
                const char *old_val = shellvar_get(name);
                if (old_val && strcmp(old_val, value) != 0) {
                    fprintf(stderr, "readonly: %s: is read only\n", name);
                    last_command_exit_code = 1;
                    *equals = '=';
                    return 0;  // Exit shell per POSIX (unless via 'command')
                }
            }

            // Set the value first
            if (shellvar_set(name, value) != 0) {
                last_command_exit_code = 1;
                *equals = '=';
                return 0;
            }

            // Then mark as readonly
            shellvar_set_readonly(name);

            // Also set in environment
            setenv(name, value, 1);

            *equals = '=';
        } else {
            // readonly name: just mark as readonly
            shellvar_set_readonly(arg);
        }
    }

    last_command_exit_code = 0;
    return 1;
}

int shell_trap(char **args) {
    // trap with no args: list all traps
    if (args[1] == NULL) {
        trap_list();
        last_command_exit_code = 0;
        return 1;
    }

    // trap -p: print traps (same as no args for now)
    if (strcmp(args[1], "-p") == 0) {
        if (args[2] == NULL) {
            trap_list();
        } else {
            // Print specific traps
            for (int i = 2; args[i]; i++) {
                int signum = trap_parse_signal(args[i]);
                if (signum >= 0) {
                    const char *action = trap_get(signum);
                    if (action) {
                        const char *name = trap_signal_name(signum);
                        if (name) {
                            printf("trap -- '%s' %s\n", action, name);
                        } else {
                            printf("trap -- '%s' %d\n", action, signum);
                        }
                    }
                }
            }
        }
        last_command_exit_code = 0;
        return 1;
    }

    // trap -l: list signal names
    if (strcmp(args[1], "-l") == 0) {
        // Print common signal numbers and names
        printf(" 1) SIGHUP\t 2) SIGINT\t 3) SIGQUIT\t 4) SIGILL\n");
        printf(" 5) SIGTRAP\t 6) SIGABRT\t 7) SIGBUS\t 8) SIGFPE\n");
        printf(" 9) SIGKILL\t10) SIGUSR1\t11) SIGSEGV\t12) SIGUSR2\n");
        printf("13) SIGPIPE\t14) SIGALRM\t15) SIGTERM\t16) SIGSTKFLT\n");
        printf("17) SIGCHLD\t18) SIGCONT\t19) SIGSTOP\t20) SIGTSTP\n");
        printf("21) SIGTTIN\t22) SIGTTOU\n");
        last_command_exit_code = 0;
        return 1;
    }

    // Check if first arg is a signal name (reset to default)
    // trap signal [signal...]: reset signals to default
    // trap '' signal [signal...]: ignore signals
    // trap action signal [signal...]: set trap

    const char *action = args[1];
    int start = 2;

    // If action is "-", it means reset to default
    if (action[0] == '-' && action[1] == '\0') {
        action = NULL;
    }

    // Set traps for all specified signals
    for (int i = start; args[i]; i++) {
        if (trap_set(action, args[i]) != 0) {
            last_command_exit_code = 1;
            return 1;
        }
    }

    last_command_exit_code = 0;
    return 1;
}

static void wait_for_all_jobs(sigset_t old_mask) {
    int status;
    pid_t pid;

    // Wait for all child processes
    // Keep trying until no more children (ECHILD) or interrupted
    while (1) {
        pid = waitpid(-1, &status, 0);
        if (pid > 0) {
            // Successfully waited for a child
            jobs_update_status(pid, status);
            const Job *job = jobs_get_by_pid(pid);
            if (job && (job->state == JOB_DONE || job->state == JOB_TERMINATED)) {
                jobs_remove(job->job_id);
            }
        } else if (pid == -1) {
            if (errno == ECHILD) {
                // No more children to wait for
                break;
            } else if (errno == EINTR) {
                // Interrupted by signal, try again
                continue;
            } else {
                // Other error, stop waiting
                break;
            }
        } else {
            // pid == 0 shouldn't happen without WNOHANG, but handle it
            break;
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    last_command_exit_code = 0;
}

static void wait_for_job_pid(pid_t pid, int job_id_to_remove) {
    int status;
    pid_t result = waitpid(pid, &status, 0);
    if (result > 0) {
        if (WIFEXITED(status)) {
            last_command_exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_command_exit_code = 128 + WTERMSIG(status);
        } else {
            last_command_exit_code = 1;
        }
        jobs_update_status(pid, status);
        // Remove the completed job from the table
        if (job_id_to_remove > 0) {
            jobs_remove(job_id_to_remove);
        }
    } else {
        // Process already exited (reaped by SIGCHLD handler) or doesn't exist
        // Check if we have a stored exit status from the job table
        const Job *job = jobs_get_by_pid(pid);
        if (job && (job->state == JOB_DONE || job->state == JOB_TERMINATED)) {
            // Use the stored exit status from when SIGCHLD reaped it
            last_command_exit_code = job->exit_status;
            jobs_remove(job->job_id);
        } else if (job_id_to_remove > 0) {
            // Job exists but no stored status - shouldn't happen
            last_command_exit_code = 127;
            jobs_remove(job_id_to_remove);
        } else {
            // Not a known job - just report 127
            last_command_exit_code = 127;
        }
    }
}

int shell_wait(char **args) {
    // Block SIGCHLD during wait to prevent race condition where
    // the SIGCHLD handler reaps children before waitpid can see them
    sigset_t block_mask, old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    // wait with no args: wait for all background jobs
    if (args[1] == NULL) {
        wait_for_all_jobs(old_mask);
        return 1;
    }

    // wait with arguments: wait for specific PIDs or job IDs
    for (int i = 1; args[i]; i++) {
        const char *arg = args[i];
        pid_t pid = 0;
        int job_id_to_remove = 0;

        // Check for job ID (%n)
        if (arg[0] == '%') {
            const Job *job = NULL;
            if (char_in_string(arg[1], "\0%+")) {
                // %%, %+, or just % - current job
                job = jobs_get_current();
            } else {
                int job_id = atoi(arg + 1);
                job = jobs_get(job_id);
            }
            if (job) {
                pid = job->pid;
                job_id_to_remove = job->job_id;
            } else {
                fprintf(stderr, "%s: wait: %s: no such job\n", HASH_NAME, arg);
                last_command_exit_code = 127;
                continue;
            }
        } else {
            // PID
            pid = atoi(arg);
            // Find job ID if this PID is in the table
            const Job *job = jobs_get_by_pid(pid);
            if (job) {
                job_id_to_remove = job->job_id;
            }
        }

        if (pid > 0) {
            wait_for_job_pid(pid, job_id_to_remove);
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return 1;
}

// Signal name to number mapping
static int signal_name_to_number(const char *name) {
    // Skip optional "SIG" prefix
    if (strncasecmp(name, "SIG", 3) == 0) {
        name += 3;
    }

    // Common signals
    if (strcasecmp(name, "HUP") == 0) return SIGHUP;
    if (strcasecmp(name, "INT") == 0) return SIGINT;
    if (strcasecmp(name, "QUIT") == 0) return SIGQUIT;
    if (strcasecmp(name, "ILL") == 0) return SIGILL;
#ifdef SIGTRAP
    if (strcasecmp(name, "TRAP") == 0) return SIGTRAP;
#endif
    if (strcasecmp(name, "ABRT") == 0) return SIGABRT;
    if (strcasecmp(name, "FPE") == 0) return SIGFPE;
    if (strcasecmp(name, "KILL") == 0) return SIGKILL;
    if (strcasecmp(name, "BUS") == 0) return SIGBUS;
    if (strcasecmp(name, "SEGV") == 0) return SIGSEGV;
    if (strcasecmp(name, "SYS") == 0) return SIGSYS;
    if (strcasecmp(name, "PIPE") == 0) return SIGPIPE;
    if (strcasecmp(name, "ALRM") == 0) return SIGALRM;
    if (strcasecmp(name, "TERM") == 0) return SIGTERM;
    if (strcasecmp(name, "URG") == 0) return SIGURG;
    if (strcasecmp(name, "STOP") == 0) return SIGSTOP;
    if (strcasecmp(name, "TSTP") == 0) return SIGTSTP;
    if (strcasecmp(name, "CONT") == 0) return SIGCONT;
    if (strcasecmp(name, "CHLD") == 0) return SIGCHLD;
    if (strcasecmp(name, "TTIN") == 0) return SIGTTIN;
    if (strcasecmp(name, "TTOU") == 0) return SIGTTOU;
#ifdef SIGIO
    if (strcasecmp(name, "IO") == 0) return SIGIO;
#endif
#ifdef SIGXCPU
    if (strcasecmp(name, "XCPU") == 0) return SIGXCPU;
#endif
#ifdef SIGXFSZ
    if (strcasecmp(name, "XFSZ") == 0) return SIGXFSZ;
#endif
#ifdef SIGVTALRM
    if (strcasecmp(name, "VTALRM") == 0) return SIGVTALRM;
#endif
#ifdef SIGPROF
    if (strcasecmp(name, "PROF") == 0) return SIGPROF;
#endif
#ifdef SIGWINCH
    if (strcasecmp(name, "WINCH") == 0) return SIGWINCH;
#endif
    if (strcasecmp(name, "USR1") == 0) return SIGUSR1;
    if (strcasecmp(name, "USR2") == 0) return SIGUSR2;

    return -1;  // Unknown signal
}

static void print_signals(void) {
    printf("HUP INT QUIT ILL ");
#ifdef SIGTRAP
    printf("TRAP ");
#endif
    printf("ABRT FPE KILL BUS SEGV SYS PIPE ALRM TERM\n");

    printf("URG STOP TSTP CONT CHLD TTIN TTOU ");
#ifdef SIGIO
    printf("IO ");
#endif
#ifdef SIGXCPU
    printf("XCPU ");
#endif
#ifdef SIGXFSZ
    printf("XFSZ ");
#endif
#ifdef SIGVTALRM
    printf("VTALRM ");
#endif
#ifdef SIGPROF
    printf("PROF ");
#endif
#ifdef SIGWINCH
    printf("WINCH ");
#endif
    printf("USR1 USR2\n");
}

static int process_each_target(char **args, int start_idx, int sig) {
    int result = 0;
    for (int i = start_idx; args[i]; i++) {
        pid_t pid = 0;
        const char *target = args[i];

        // Check for job specification
        if (target[0] == '%') {
            // Job specs only work when job control is enabled (set -m)
            if (!shell_option_monitor()) {
                fprintf(stderr, "%s: kill: %s: no job control\n", HASH_NAME, target);
                result = 1;
                continue;
            }

            const Job *job = NULL;
            if (char_in_string(target[1], "\0%+")) {
                // %%, %+, or just % - current job
                job = jobs_get_current();
            } else if (target[1] == '-') {
                // %- - previous job (we don't track this, use current)
                job = jobs_get_current();
            } else if (isdigit(target[1])) {
                // %n - job number
                int job_id = atoi(target + 1);
                job = jobs_get(job_id);
            }

            if (job) {
                pid = job->pid;
            } else {
                fprintf(stderr, "%s: kill: %s: no such job\n", HASH_NAME, target);
                result = 1;
                continue;
            }
        } else {
            // PID
            char *endptr;
            long num = strtol(target, &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "%s: kill: %s: arguments must be process or job IDs\n", HASH_NAME, target);
                result = 1;
                continue;
            }
            pid = (pid_t)num;
        }

        // Send signal
        if (kill(pid, sig) == -1) {
            fprintf(stderr, "%s: kill: (%d) - %s\n", HASH_NAME, pid, strerror(errno));
            result = 1;
        }
    }

    return result;
}

int shell_kill(char **args) {
    int sig = SIGTERM;  // Default signal
    int start_idx = 1;

    // kill -l [arg...]: the whole list, or for each arg the name of a signal
    // number (or of an exit status 128+n) and the number of a signal name;
    // an unknown one fails. modernish builds its signal table from
    // `kill -l N` for N in 1..127 and needs the misses to fail.
    if (args[1] && strcmp(args[1], "-l") == 0) {
        if (args[2] == NULL) {
            print_signals();
            return listing_status("kill");
        }
        int status = 0;
        for (int i = 2; args[i] != NULL; i++) {
            char *end;
            long n = strtol(args[i], &end, 10);
            if (*args[i] && *end == '\0') {
                if (n > 128) {
                    n -= 128;
                }
                const char *name = (n > 0 && n < 128) ? trap_signal_name((int)n) : NULL;
                if (name) {
                    printf("%s\n", strncmp(name, "SIG", 3) == 0 ? name + 3 : name);
                } else {
                    fprintf(stderr, "%s: kill: %s: invalid signal specification\n", HASH_NAME, args[i]);
                    status = 1;
                }
            } else {
                int num = signal_name_to_number(args[i]);
                if (num > 0) {
                    printf("%d\n", num);
                } else {
                    fprintf(stderr, "%s: kill: %s: invalid signal specification\n", HASH_NAME, args[i]);
                    status = 1;
                }
            }
        }
        if (status == 0) {
            return listing_status("kill");
        }
        last_command_exit_code = status;
        return 1;
    }

    // Parse signal specification
    if (args[1] && args[1][0] == '-') {
        const char *sigspec = args[1] + 1;

        if (strcmp(sigspec, "s") == 0) {
            // -s SIG format
            if (!args[2]) {
                fprintf(stderr, "%s: kill: -s requires an argument\n", HASH_NAME);
                last_command_exit_code = 1;
                return 1;
            }
            sigspec = args[2];
            start_idx = 3;
        } else {
            start_idx = 2;
        }

        // Check if it's a number
        char *endptr;
        long num = strtol(sigspec, &endptr, 10);
        if (*endptr == '\0') {
            sig = (int)num;
        } else {
            // It's a signal name
            sig = signal_name_to_number(sigspec);
            if (sig == -1) {
                fprintf(stderr, "%s: kill: %s: invalid signal specification\n", HASH_NAME, sigspec);
                last_command_exit_code = 1;
                return 1;
            }
        }
    }

    // No targets specified
    if (!args[start_idx]) {
        fprintf(stderr, "usage: kill [-s sigspec | -sigspec] pid | jobspec ...\n");
        last_command_exit_code = 1;
        return 1;
    }

    // Process each target
    last_command_exit_code = process_each_target(args, start_idx, sig);
    return 1;
}

// ============================================================================
// Command Hash Table (for hash builtin)
// ============================================================================

#define CMD_HASH_SIZE 64

typedef struct CmdHashEntry {
    char *name;
    char *path;
    int hits;
    struct CmdHashEntry *next;
} CmdHashEntry;

static CmdHashEntry *cmd_hash_table[CMD_HASH_SIZE];

// Simple hash function for command names
static unsigned int cmd_hash_func(const char *s) {
    unsigned int hash = 0;
    while (*s) {
        hash = hash * 31 + (unsigned char)*s++;
    }
    return hash % CMD_HASH_SIZE;
}

// Add a command to the hash table
void cmd_hash_add(const char *name, const char *path) {
    unsigned int h = cmd_hash_func(name);

    // Check if already exists
    CmdHashEntry *e = cmd_hash_table[h];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            // Update path if different
            if (strcmp(e->path, path) != 0) {
                free(e->path);
                e->path = strdup(path);
            }
            e->hits++;
            return;
        }
        e = e->next;
    }

    // Add new entry
    CmdHashEntry *new_entry = malloc(sizeof(CmdHashEntry));
    if (new_entry) {
        new_entry->name = strdup(name);
        new_entry->path = strdup(path);
        new_entry->hits = 1;
        new_entry->next = cmd_hash_table[h];
        cmd_hash_table[h] = new_entry;
    }
}

// Clear all entries from hash table
static void cmd_hash_clear(void) {
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        CmdHashEntry *e = cmd_hash_table[i];
        while (e) {
            CmdHashEntry *next = e->next;
            free(e->name);
            free(e->path);
            free(e);
            e = next;
        }
        cmd_hash_table[i] = NULL;
    }
}

// List all hashed commands
static void cmd_hash_list(void) {
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        const CmdHashEntry *e = cmd_hash_table[i];
        if (e) {
            printf("hits\tcommand\n");
        }
        if (cmd_hash_table[i]) break;
    }

    // Print header and entries
    bool has_entries = false;
    for (int i = 0; i < CMD_HASH_SIZE; i++) {
        if (cmd_hash_table[i]) {
            has_entries = true;
            break;
        }
    }

    if (has_entries) {
        for (int i = 0; i < CMD_HASH_SIZE; i++) {
            CmdHashEntry *e = cmd_hash_table[i];
            while (e) {
                printf("%4d\t%s\n", e->hits, e->path);
                e = e->next;
            }
        }
    }
}

int shell_hash(char **args) {
    // hash with no args: list all hashed commands
    if (args[1] == NULL) {
        cmd_hash_list();
        last_command_exit_code = 0;
        return 1;
    }

    // hash -r: clear hash table
    if (strcmp(args[1], "-r") == 0) {
        cmd_hash_clear();
        last_command_exit_code = 0;
        return 1;
    }

    // hash name [name...]: look up and hash commands
    for (int i = 1; args[i]; i++) {
        char *path = find_in_path(args[i]);
        if (path) {
            cmd_hash_add(args[i], path);
            free(path);
        } else {
            fprintf(stderr, "%s: hash: %s: not found\n", HASH_NAME, args[i]);
            last_command_exit_code = 1;
            return 1;
        }
    }

    last_command_exit_code = 0;
    return 1;
}

// ============================================================================
// Builtin Dispatch
// ============================================================================

int try_builtin(char **args) {
    if (args[0] == NULL) {
        return -1;
    }

    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            return (*builtins[i].func)(args);
        }
    }

    return -1;
}

bool is_builtin(const char *cmd) {
    if (cmd == NULL) {
        return false;
    }

    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// complete: programmable completion
// ============================================================================

static void complete_usage(void) {
    fprintf(stderr, "%s: complete: usage: complete [-abcdfuv] [-o default|dirnames] [-W wordlist] [-F function] name...\n"
                    "                           complete -p [name...]   complete -r [name...]\n", HASH_NAME);
}

int shell_complete(char **args) {
    bool print = false;
    bool remove = false;
    bool fallback_default = false;
    bool fallback_dirs = false;
    const char *wordlist = NULL;
    const char *function = NULL;
    unsigned actions = 0;
    bool any_spec_option = false;
    int i = 1;

    for (; args[i] && args[i][0] == '-' && args[i][1] != '\0'; i++) {
        const char *opt = args[i];
        if (strcmp(opt, "--") == 0) {
            i++;
            break;
        }
        if (strcmp(opt, "-W") == 0 || strcmp(opt, "-F") == 0 || strcmp(opt, "-o") == 0) {
            const char *value = args[i + 1];
            if (!value) {
                fprintf(stderr, "%s: complete: %s: option requires an argument\n", HASH_NAME, opt);
                last_command_exit_code = 2;
                return 1;
            }
            i++;
            any_spec_option = true;
            if (opt[1] == 'W') {
                wordlist = value;
            } else if (opt[1] == 'F') {
                function = value;
            } else if (strcmp(value, "default") == 0) {
                fallback_default = true;
            } else if (strcmp(value, "dirnames") == 0) {
                fallback_dirs = true;
            } else {
                fprintf(stderr, "%s: complete: -o %s: invalid option name\n", HASH_NAME, value);
                last_command_exit_code = 2;
                return 1;
            }
            continue;
        }
        for (const char *p = opt + 1; *p; p++) {
            switch (*p) {
                case 'p': print = true; break;
                case 'r': remove = true; break;
                case 'a': actions |= COMP_ACTION_ALIASES; any_spec_option = true; break;
                case 'b': actions |= COMP_ACTION_BUILTINS; any_spec_option = true; break;
                case 'c': actions |= COMP_ACTION_COMMANDS; any_spec_option = true; break;
                case 'd': actions |= COMP_ACTION_DIRS; any_spec_option = true; break;
                case 'f': actions |= COMP_ACTION_FILES; any_spec_option = true; break;
                case 'u': actions |= COMP_ACTION_USERS; any_spec_option = true; break;
                case 'v': actions |= COMP_ACTION_VARIABLES; any_spec_option = true; break;
                default:
                    fprintf(stderr, "%s: complete: -%c: invalid option\n", HASH_NAME, *p);
                    complete_usage();
                    last_command_exit_code = 2;
                    return 1;
            }
        }
    }

    int status = 0;
    if (remove) {
        if (!args[i]) {
            completion_spec_remove_all();
        }
        for (; args[i]; i++) {
            if (!completion_spec_remove(args[i])) {
                fprintf(stderr, "%s: complete: %s: no completion specification\n", HASH_NAME, args[i]);
                status = 1;
            }
        }
    } else if (print || !any_spec_option) {
        if (!args[i]) {
            for (const CompSpec *s = completion_spec_first(); s; s = s->next) {
                completion_spec_print(stdout, s);
            }
        }
        for (; args[i]; i++) {
            const CompSpec *s = completion_spec_find(args[i]);
            if (s) {
                completion_spec_print(stdout, s);
            } else {
                fprintf(stderr, "%s: complete: %s: no completion specification\n", HASH_NAME, args[i]);
                status = 1;
            }
        }
    } else if (!args[i]) {
        complete_usage();
        status = 2;
    } else {
        for (; args[i]; i++) {
            if (completion_spec_define(args[i], wordlist, function, actions, fallback_default, fallback_dirs) != 0) {
                fprintf(stderr, "%s: complete: %s: could not record the specification\n", HASH_NAME, args[i]);
                status = 1;
            }
        }
    }
    last_command_exit_code = status;
    return 1;
}

// ============================================================================
// help: builtins and keywords, with their synopsis and a short description
// ============================================================================

// The shell's keywords are not builtins; help knows them too
typedef struct {
    const char *name;
    const char *synopsis;
    const char *description;
} HelpTopic;

static const HelpTopic keyword_topics[] = {
    {"if", "if list; then list; [elif list; then list;]... [else list;] fi",
     "Run the first list whose condition succeeds (status 0), or the else list.\n"},
    {"for", "for name [in word...]; do list; done",
     "Run list once for each word, with name set to it; without `in`, over the\n"
     "positional parameters.\n"},
    {"while", "while list; do list; done",
     "Run the body as long as the condition list succeeds.\n"},
    {"until", "until list; do list; done",
     "Run the body as long as the condition list fails.\n"},
    {"case", "case word in [pattern [| pattern]...) list ;;]... esac",
     "Run the list of the first pattern that matches word. Patterns use * ? and\n"
     "[...]; quote them to match literally.\n"},
    {"function", "name() compound-command [redirection...]",
     "Define a function; `name` then runs the command with the arguments as\n"
     "positional parameters. `return` leaves it, `local` scopes variables to it.\n"},
    {"{", "{ list; }",
     "Group commands to run in the current shell, as one command for\n"
     "redirections and && / ||.\n"},
    {"(", "( list )",
     "Run commands in a subshell: a copy of the shell whose changes to\n"
     "variables and the working directory do not come back.\n"},
    {"!", "! pipeline",
     "Negate the status of a pipeline.\n"},
};
#define KEYWORD_TOPIC_COUNT (sizeof(keyword_topics) / sizeof(keyword_topics[0]))

// Pages of the book for topics that have one of their own
static const struct { const char *name; const char *page; } help_pages[] = {
    {"test", "scripting/test-command.html"}, {"[", "scripting/test-command.html"},
    {"[[", "scripting/test-command.html"}, {"history", "shell-features/history.html"},
    {"complete", "shell-features/tab-completion.html"}, {"alias", "getting-started/hashrc.html"},
    {"unalias", "getting-started/hashrc.html"}, {"set", "reference/index.html#shell-options"},
    {"if", "scripting/control-structures.html"}, {"for", "scripting/control-structures.html"},
    {"while", "scripting/control-structures.html"}, {"until", "scripting/control-structures.html"},
    {"case", "scripting/control-structures.html"}, {"function", "scripting/index.html#functions"},
    {"local", "scripting/index.html#functions"}, {"return", "scripting/index.html#functions"},
    {"jobs", "operators/background.html"}, {"fg", "operators/background.html"},
    {"bg", "operators/background.html"}, {"wait", "operators/background.html"},
    {"kill", "operators/background.html"}, {"exec", "operators/redirection.html"},
    {"(", "operators/command-chaining.html"}, {"!", "operators/command-chaining.html"},
    {"{", "operators/command-chaining.html"},
};

static const char *help_page_for(const char *name) {
    for (size_t i = 0; i < sizeof(help_pages) / sizeof(help_pages[0]); i++) {
        if (strcmp(help_pages[i].name, name) == 0) {
            return help_pages[i].page;
        }
    }
    return "reference/index.html#built-in-commands";
}

static void help_print_topic(const char *name, const char *synopsis, const char *description, bool synopsis_only) {
    printf("%s\n", synopsis);
    if (synopsis_only) {
        return;
    }
    printf("\n");
    const char *p = description;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        printf("    %.*s\n", (int)len, p);
        p += len + (nl ? 1 : 0);
    }
    printf("\n    More: https://hash-shell.org/%s\n", help_page_for(name));
}

static int help_compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

// Every topic name, sorted, for the bare `help` listing
static void help_list_all(void) {
    const char *names[BUILTIN_FUNC_MAX + KEYWORD_TOPIC_COUNT];
    size_t count = 0;
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        names[count++] = builtins[i].name;
    }
    for (size_t i = 0; i < KEYWORD_TOPIC_COUNT; i++) {
        names[count++] = keyword_topics[i].name;
    }
    qsort(names, count, sizeof(names[0]), help_compare_names);
    printf("%s builtins and keywords. `help NAME` describes one, `help -s NAME` prints\n"
           "its synopsis; names may be patterns. The book: https://hash-shell.org/\n\n", HASH_NAME);
    for (size_t i = 0; i < count; i++) {
        const char *synopsis = NULL;
        for (int b = 0; b < BUILTIN_FUNC_MAX && !synopsis; b++) {
            if (strcmp(builtins[b].name, names[i]) == 0) {
                synopsis = builtins[b].synopsis;
            }
        }
        for (size_t k = 0; k < KEYWORD_TOPIC_COUNT && !synopsis; k++) {
            if (strcmp(keyword_topics[k].name, names[i]) == 0) {
                synopsis = keyword_topics[k].synopsis;
            }
        }
        // A synopsis may have several lines; indent the continuation ones
        const char *p = synopsis ? synopsis : names[i];
        const char *nl;
        while ((nl = strchr(p, '\n')) != NULL) {
            printf("%.*s\n", (int)(nl - p), p);
            p = nl + 1;
        }
        printf("%s\n", p);
    }
}

int shell_help(char **args) {
    bool synopsis_only = false;
    int i = 1;
    while (args[i] && args[i][0] == '-' && args[i][1] != '\0') {
        if (strcmp(args[i], "--") == 0) {
            i++;
            break;
        }
        if (strcmp(args[i], "-s") == 0) {
            synopsis_only = true;
            i++;
            continue;
        }
        fprintf(stderr, "%s: help: %s: invalid option\n%s: help: usage: help [-s] [pattern...]\n",
                HASH_NAME, args[i], HASH_NAME);
        last_command_exit_code = 2;
        return 1;
    }
    if (!args[i]) {
        help_list_all();
        return listing_status("help");
    }
    int status = 0;
    bool first = true;
    for (; args[i]; i++) {
        bool found = false;
        for (int b = 0; b < BUILTIN_FUNC_MAX; b++) {
            if (fnmatch(args[i], builtins[b].name, 0) == 0) {
                if (!first && !synopsis_only) printf("\n");
                help_print_topic(builtins[b].name, builtins[b].synopsis, builtins[b].description, synopsis_only);
                found = true;
                first = false;
            }
        }
        for (size_t k = 0; k < KEYWORD_TOPIC_COUNT; k++) {
            if (fnmatch(args[i], keyword_topics[k].name, 0) == 0) {
                if (!first && !synopsis_only) printf("\n");
                help_print_topic(keyword_topics[k].name, keyword_topics[k].synopsis, keyword_topics[k].description, synopsis_only);
                found = true;
                first = false;
            }
        }
        if (!found) {
            fprintf(stderr, "%s: help: no help topics match '%s'. Try 'help' for the list.\n", HASH_NAME, args[i]);
            status = 1;
        }
    }
    if (status == 0) {
        return listing_status("help");   // the text was written, or it was not
    }
    last_command_exit_code = status;
    return 1;
}
