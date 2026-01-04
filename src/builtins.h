#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

// Built-in command: change directory
int shell_cd(char **args);

// Built-in command: exit shell
int shell_exit(char **args);

// Built-in command: manage aliases
int shell_alias(char **args);

// Built-in command: remove alias
int shell_unalias(char **args);

// Built-in command: source config file (also handles '.')
int shell_source(char **args);

// Built-in command: export environment variable
int shell_export(char **args);

// Built-in command: show command history
int shell_history(char **args);

// Built-in command: set shell options
int shell_set(char **args);

// Built-in command: list background jobs
int shell_jobs(char **args);

// Built-in command: bring job to foreground
int shell_fg(char **args);

// Built-in command: continue job in background
int shell_bg(char **args);

// Built-in command: logout from login shell
int shell_logout(char **args);

// Built-in command: test expression ([ ] and test)
int shell_test(char **args);

// Built-in command: [ (alias for test with ] check)
int shell_bracket(char **args);

// Built-in command: [[ ]] extended test (bash-style)
int shell_double_bracket(char **args);

// Built-in command: unset environment variable or function
int shell_unset(char **args);

// Built-in command: true - always succeeds
int shell_true(char **args);

// Built-in command: false - always fails
int shell_false(char **args);

// Built-in command: colon (:) - null command, always succeeds
int shell_colon(char **args);

// Built-in command: echo - print arguments
int shell_echo(char **args);

// Built-in command: read - read line from stdin
int shell_read(char **args);

// Built-in command: return - return from function or sourced script
int shell_return(char **args);

// Built-in command: break - exit from loop
int shell_break(char **args);

// Built-in command: continue - skip to next loop iteration
int shell_continue_cmd(char **args);

// Built-in command: eval - evaluate arguments as shell command
int shell_eval(char **args);

// Built-in command: update - check for and install updates
int shell_update(char **args);

// Set login shell status (called from main)
void builtins_set_login_shell(bool is_login);

// Check if command is a built-in and execute it
// Returns -1 if not a built-in, otherwise returns the result
int try_builtin(char **args);

#endif // BUILTINS_H
