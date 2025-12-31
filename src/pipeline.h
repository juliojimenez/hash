#ifndef PIPELINE_H
#define PIPELINE_H

// A single command in a pipeline
typedef struct {
    char *cmd_line;
} PipeCommand;

// A pipeline of commands connected by pipes
typedef struct {
    PipeCommand *commands;
    int count;
} Pipeline;

/**
 * Parse a command line into a pipeline
 * Splits on | operator (respects quotes)
 *
 * @param line Command line to parse
 * @return Pipeline structure, or NULL on error
 */
Pipeline *pipeline_parse(char *line);

/**
 * Execute a pipeline
 * Connects stdout of each command to stdin of next
 *
 * @param pipeline Pipeline to execute
 * @return Exit code of last command in pipeline
 */
int pipeline_execute(const Pipeline *pipeline);

/**
 * Free a pipeline structure
 *
 * @param pipeline Pipeline to free
 */
void pipeline_free(Pipeline *pipeline);

#endif // PIPELINE_H
