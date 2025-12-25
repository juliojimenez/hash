#ifndef EXECUTE_H
#define EXECUTE_H
extern int last_command_exit_code;
int execute(char **args);
int execute_get_last_exit_code(void);
#endif
