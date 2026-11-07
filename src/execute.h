#ifndef EXECUTE_H
#define EXECUTE_H

#include "script.h"

// External global to store last exit code
extern int last_command_exit_code;

/**
 * Execute a command
 *
 * The command may be built-in or external
 *
 * @param args The arguments (including command)
 *
 * @return
 */
int execute(char **args);

/**
 * The next execute() call runs no shell function: `command name` must reach
 * the builtin or the external command even when a function has that name
 */
void execute_bypass_functions(void);

/**
 * Get the last exit code
 *
 * @return Returns the last command exit code
 */
int execute_get_last_exit_code(void);

#endif
