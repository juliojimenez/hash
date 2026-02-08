#ifndef EXECUTE_H
#define EXECUTE_H

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
 * Get the last exit code
 *
 * @return Returns the last command exit code
 */
int execute_get_last_exit_code(void);

#endif
