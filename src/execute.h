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
 * Hook for running shell function bodies. When set, execute() calls it in
 * place of script_execute_function() so another engine can run the body.
 * Same contract: returns 1 to keep going, 0 when the shell should stop.
 */
typedef int (*FunctionRunner)(const ShellFunction *func, int argc, char **argv);
void execute_set_function_runner(FunctionRunner runner);

/**
 * Get the last exit code
 *
 * @return Returns the last command exit code
 */
int execute_get_last_exit_code(void);

#endif
