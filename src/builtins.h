#ifndef BUILTINS_H
#define BUILTINS_H

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

// Check if command is a built-in and execute it
// Returns -1 if not a built-in, otherwise returns the result
int try_builtin(char **args);

#endif // BUILTINS_H
