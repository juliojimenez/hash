#ifndef BUILTINS_H
#define BUILTINS_H

// Built-in command: change directory
int shell_cd(char **args);

// Built-in command: exit shell
int shell_exit(char **args);

// Check if command is a built-in and execute it
// Returns -1 if not a built-in, otherwise returns the result
int try_builtin(char **args);

#endif // BUILTINS_H
