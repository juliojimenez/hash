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

// Built-in command: source config file
int shell_source(char **args);

// Built-in command: export environment variable
int shell_export(char **args);

// Built-in command: show command history
int shell_history(char **args);

// Built-in command: list background jobs
int shell_jobs(char **args);

// Built-in command: bring job to foreground
int shell_fg(char **args);

// Built-in command: continue job in background
int shell_bg(char **args);

// Built-in command: logout from login shell
int shell_logout(char **args);

// Set login shell status (called from main)
void builtins_set_login_shell(bool is_login);

// Check if command is a built-in and execute it
// Returns -1 if not a built-in, otherwise returns the result
int try_builtin(char **args);

#endif // BUILTINS_H
