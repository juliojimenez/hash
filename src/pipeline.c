#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include "hash.h"
#include "pipeline.h"
#include "parser.h"
#include "execute.h"
#include "safe_string.h"
#include "redirect.h"

#define INITIAL_PIPE_CAPACITY 8

// Create a new pipeline
static Pipeline *pipeline_create(void) {
    Pipeline *pipeline = malloc(sizeof(Pipeline));
    if (!pipeline) return NULL;

    pipeline->commands = calloc(INITIAL_PIPE_CAPACITY, sizeof(PipeCommand));
    if (!pipeline->commands) {
        free(pipeline);
        return NULL;
    }

    pipeline->count = 0;
    return pipeline;
}

// Add a command to pipeline
static int pipeline_add(Pipeline *pipeline, const char *cmd_line) {
    if (!pipeline || !cmd_line) return -1;

    pipeline->commands[pipeline->count].cmd_line = strdup(cmd_line);
    if (!pipeline->commands[pipeline->count].cmd_line) {
        return -1;
    }

    pipeline->count++;
    return 0;
}

// Parse command line into pipeline
Pipeline *pipeline_parse(char *line) {
    if (!line) return NULL;

    Pipeline *pipeline = pipeline_create();
    if (!pipeline) return NULL;

    char *current = line;
    char *cmd_start = line;
    int in_single_quote = 0;
    int in_double_quote = 0;
    int escaped = 0;

    while (*current) {
        // Handle escape
        if (escaped) {
            escaped = 0;
            current++;
            continue;
        }

        if (*current == '\\') {
            escaped = 1;
            current++;
            continue;
        }

        // Track quotes
        if (*current == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*current == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }

        // Look for pipe outside quotes
        if (!in_single_quote && !in_double_quote && *current == '|') {
            // Check if it's || (OR operator) - skip both characters
            if (*(current + 1) == '|') {
                current += 2;  // Skip both | characters
                continue;
            }

            // Single | - it's a pipe
            *current = '\0';

            // Trim and add command
            safe_trim(cmd_start);
            if (*cmd_start != '\0') {
                if (pipeline_add(pipeline, cmd_start) != 0) {
                    pipeline_free(pipeline);
                    return NULL;
                }
            }

            // Move to next command
            current++;
            cmd_start = current;
            continue;
        }

        current++;
    }

    // Add final command
    safe_trim(cmd_start);
    if (*cmd_start != '\0') {
        if (pipeline_add(pipeline, cmd_start) != 0) {
            pipeline_free(pipeline);
            return NULL;
        }
    }

    // If no commands or only one command, not a pipeline
    if (pipeline->count <= 1) {
        pipeline_free(pipeline);
        return NULL;
    }

    return pipeline;
}

// Execute a pipeline
int pipeline_execute(const Pipeline *pipeline) {
    if (!pipeline || pipeline->count == 0) return -1;
    if (pipeline->count == 1) return -1;  // Single command, shouldn't be here

    int num_pipes = pipeline->count - 1;
    int (*pipes)[2] = malloc(num_pipes * sizeof(int[2]));
    pid_t *pids = malloc(pipeline->count * sizeof(pid_t));

    if (!pipes || !pids) {
        free(pipes);
        free(pids);
        return -1;
    }

    // Create all pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            free(pipes);
            free(pids);
            return -1;
        }
    }

    // Fork and execute each command
    for (int i = 0; i < pipeline->count; i++) {
        pids[i] = fork();

        if (pids[i] == -1) {
            perror("fork");
            // Clean up pipes
            for (int j = 0; j < num_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(pipes);
            free(pids);
            return -1;
        }

        if (pids[i] == 0) {
            // Child process

            // Set up input (stdin)
            if (i > 0) {
                // Not first command - read from previous pipe
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            // Set up output (stdout)
            if (i < pipeline->count - 1) {
                // Not last command - write to next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors
            for (int j = 0; j < num_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Parse and execute command
            char *line_copy = strdup(pipeline->commands[i].cmd_line);
            if (!line_copy) exit(EXIT_FAILURE);

            char **args = parse_line(line_copy);
            if (!args) {
                free(line_copy);
                exit(EXIT_FAILURE);
            }

            // Parse redirections
            RedirInfo *redir = redirect_parse(args);
            char **exec_args = redir ? redir->args : args;

            // Apply redirections
            if (redir && redirect_apply(redir) != 0) {
                redirect_free(redir);
                free(args);
                free(line_copy);
                exit(EXIT_FAILURE);
            }

            // Execute (this will handle expansions, built-ins, etc.)
            // execute(args);

            // Execute (this will handle expansions, but not built-ins in pipes)
            if (execvp(exec_args[0], exec_args) == -1) {
                perror(HASH_NAME);
            }

            // Should not reach here for built-ins that continue
            redirect_free(redir);
            free(args);
            free(line_copy);
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process - close all pipes
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children
    int last_exit_code = 0;
    for (int i = 0; i < pipeline->count; i++) {
        int status;
        waitpid(pids[i], &status, 0);

        // Track exit code of last command
        if (i == pipeline->count - 1) {
            if (WIFEXITED(status)) {
                last_exit_code = WEXITSTATUS(status);
            } else {
                last_exit_code = 1;
            }
        }
    }

    free(pipes);
    free(pids);

    return last_exit_code;
}

// Free pipeline
void pipeline_free(Pipeline *pipeline) {
    if (!pipeline) return;

    for (int i = 0; i < pipeline->count; i++) {
        free(pipeline->commands[i].cmd_line);
    }

    free(pipeline->commands);
    free(pipeline);
}
