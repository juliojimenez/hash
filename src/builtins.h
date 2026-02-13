#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

typedef enum {
    BUILTIN_FUNC_CD,
    BUILTIN_FUNC_EXIT,
    BUILTIN_FUNC_ALIAS,
    BUILTIN_FUNC_UNALIAS,
    BUILTIN_FUNC_SOURCE,
    BUILTIN_FUNC_DOT,
    BUILTIN_FUNC_EXPORT,
    BUILTIN_FUNC_SET,
    BUILTIN_FUNC_HISTORY,
    BUILTIN_FUNC_JOBS,
    BUILTIN_FUNC_FG,
    BUILTIN_FUNC_BG,
    BUILTIN_FUNC_LOGOUT,
    BUILTIN_FUNC_TEST,
    BUILTIN_FUNC_BRACKET,
    BUILTIN_FUNC_DOUBLE_BRACKET,
    BUILTIN_FUNC_UNSET,
    BUILTIN_FUNC_TRUE,
    BUILTIN_FUNC_FALSE,
    BUILTIN_FUNC_COLON,
    BUILTIN_FUNC_ECHO,
    BUILTIN_FUNC_READ,
    BUILTIN_FUNC_RETURN,
    BUILTIN_FUNC_BREAK,
    BUILTIN_FUNC_CONTINUE,
    BUILTIN_FUNC_EVAL,
    BUILTIN_FUNC_UPDATE,
    BUILTIN_FUNC_COMMAND,
    BUILTIN_FUNC_EXEC,
    BUILTIN_FUNC_TIMES,
    BUILTIN_FUNC_TYPE,
    BUILTIN_FUNC_READONLY,
    BUILTIN_FUNC_TRAP,
    BUILTIN_FUNC_WAIT,
    BUILTIN_FUNC_KILL,
    BUILTIN_FUNC_HASH,

    BUILTIN_FUNC_MAX
} BuiltinFunc;

typedef struct {
    const char *name;
    int (*func)(char **);
} Builtin;

extern Builtin builtins[BUILTIN_FUNC_MAX];

/**
 * Built-in command: change directory
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_cd(char **args);

/**
 * Built-in command: exit shell
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_exit(char **args);

/**
 * Built-in command: manage aliases
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_alias(char **args);

/**
 * Built-in command: remove alias
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_unalias(char **args);

/**
 * Built-in command: source config file (also handles '.')
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_source(char **args);

/**
 * Built-in command: export environment variable
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_export(char **args);

/**
 * Built-in command: show command history
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_history(char **args);

/**
 * Built-in command: set shell options
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_set(char **args);

/**
 * Built-in command: list background jobs
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_jobs(char **args);

/**
 * Built-in command: bring job to foreground
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_fg(char **args);

/**
 * Built-in command: continue job in background
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_bg(char **args);

/**
 * Built-in command: logout from login shell
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_logout(char **args);

/**
 * Built-in command: test expression ([ ] and test)
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_test(char **args);

/**
 * Built-in command: [ (alias for test with ] check)
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_bracket(char **args);

/**
 * Built-in command: [[ ]] extended test (bash-style)
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_double_bracket(char **args);

/**
 * Built-in command: unset environment variable or function
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_unset(char **args);

/**
 * Built-in command: true - always succeeds
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_true(char **args);

/**
 * Built-in command: false - always fails
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_false(char **args);

/**
 * Built-in command: colon (:) - null command, always succeeds
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_colon(char **args);

/**
 * Built-in command: echo - print arguments
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_echo(char **args);

/**
 * Built-in command: read - read line from stdin
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_read(char **args);

/**
 * Built-in command: return - return from function or sourced script
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_return(char **args);

/**
 * Built-in command: break - exit from loop
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_break(char **args);

/**
 * Built-in command: continue - skip to next loop iteration
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_continue_cmd(char **args);

/**
 * Built-in command: eval - evaluate arguments as shell command
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_eval(char **args);

/**
 * Built-in command: update - check for and install updates
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_update(char **args);

/**
 * Built-in command: command - execute command or describe command type
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_command(char **args);

/**
 * Built-in command: exec - replace shell or manage file descriptors
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_exec(char **args);

/**
 * Built-in command: times - print shell process times
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_times(char **args);

/**
 * Built-in command: type - describe command type (alias for command -V)
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_type(char **args);

/**
 * Built-in command: readonly - mark variables as readonly
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_readonly(char **args);

/**
 * Built-in command: trap - set signal handlers
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_trap(char **args);

/**
 * Built-in command: wait - wait for background jobs
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_wait(char **args);

/**
 * Built-in command: kill - send signal to process or job
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_kill(char **args);

/**
 * Built-in command: hash - manage command path hash table
 *
 * @param args Arguments for the command
 *
 * @return
 */
int shell_hash(char **args);

/**
 * Add a command to the hash table (called when external commands are executed)
 *
 * @param name Name of the command
 * @param path Path to the command
 */
void cmd_hash_add(const char *name, const char *path);

/**
 * Find command in PATH and return full path (caller must free)
 *
 * @param cmd Command to search for
 *
 * @return Returns Path to command or NULL if couldn't find
 */
char *find_in_path(const char *cmd);

/**
 * Set login shell status (called from main)
 *
 * @param is_login Whether login shell
 */
void builtins_set_login_shell(bool is_login);

/**
 * Check if command is a built-in and execute it
 *
 * @param args Command and arguments array
 *
 * @return Returns -1 if not a built-in, otherwise returns the result
 */
int try_builtin(char **args);

/**
 * Check if a command name is a built-in (without executing it)
 *
 * @param cmd The command to check
 *
 * @return Returns true if it's a builtin, false otherwise
 */
bool is_builtin(const char *cmd);

#endif // BUILTINS_H
