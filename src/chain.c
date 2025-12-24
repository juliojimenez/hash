#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "chain.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "safe_string.h"

#define INITIAL_CHAIN_CAPACITY 8

// Create a new command chain
static CommandChain *chain_create(void) {
    CommandChain *chain = malloc(sizeof(CommandChain));
    if (!chain) return NULL;

    chain->capacity = INITIAL_CHAIN_CAPACITY;
    chain->count = 0;
    chain->commands = malloc(chain->capacity * sizeof(ChainedCommand));

    if (!chain->commands) {
        free(chain);
        return NULL;
    }

    return chain;
}

// Add a command to the chain
static int chain_add(CommandChain *chain, const char *cmd_line, ChainOp next_op) {
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        ChainedCommand *new_cmds = realloc(chain->commands,
                                           chain->capacity * sizeof(ChainedCommand));
        if (!new_cmds) return -1;
        chain->commands = new_cmds;
    }

    chain->commands[chain->count].cmd_line = strdup(cmd_line);
    if (!chain->commands[chain->count].cmd_line) return -1;

    chain->commands[chain->count].next_op = next_op;
    chain->count++;

    return 0;
}

// Parse a line into chained commands
CommandChain *chain_parse(char *line) {
    if (!line) return NULL;

    CommandChain *chain = chain_create();
    if (!chain) return NULL;

    char *current = line;
    char *cmd_start = line;
    int in_single_quote = 0;
    int in_double_quote = 0;
    int escaped = 0;

    while (*current) {
        // Handle escape sequences
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

        // Track quote state
        if (*current == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*current == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }

        // Look for operators outside quotes
        if (!in_single_quote && !in_double_quote) {
            ChainOp op = CHAIN_NONE;
            int op_len = 0;

            // Check for &&
            if (*current == '&' && *(current + 1) == '&') {
                op = CHAIN_AND;
                op_len = 2;
            }
            // Check for ||
            else if (*current == '|' && *(current + 1) == '|') {
                op = CHAIN_OR;
                op_len = 2;
            }
            // Check for ;
            else if (*current == ';') {
                op = CHAIN_ALWAYS;
                op_len = 1;
            }

            // Found an operator
            if (op != CHAIN_NONE) {
                // Extract command
                *current = '\0';

                // Trim whitespace from command
                char *cmd = cmd_start;
                while (isspace(*cmd)) cmd++;

                char *end = current - 1;
                while (end > cmd && isspace(*end)) {
                    *end = '\0';
                    end--;
                }

                // Add command to chain
                if (safe_strlen(cmd, sizeof(cmd)) > 0) {
                    if (chain_add(chain, cmd, op) != 0) {
                        chain_free(chain);
                        return NULL;
                    }
                }

                // Move past operator
                current += op_len;
                cmd_start = current;
                continue;
            }
        }

        current++;
    }

    // Add final command
    char *cmd = cmd_start;
    while (isspace(*cmd)) cmd++;

    if (*cmd) {
        char *end = cmd + safe_strlen(cmd, sizeof(cmd)) - 1;
        while (end > cmd && isspace(*end)) {
            *end = '\0';
            end--;
        }

        if (safe_strlen(cmd, sizeof(cmd)) > 0) {
            if (chain_add(chain, cmd, CHAIN_NONE) != 0) {
                chain_free(chain);
                return NULL;
            }
        }
    }

    // If no commands were added, free and return NULL
    if (chain->count == 0) {
        chain_free(chain);
        return NULL;
    }

    return chain;
}

// Free a command chain
void chain_free(CommandChain *chain) {
    if (!chain) return;

    for (int i = 0; i < chain->count; i++) {
        free(chain->commands[i].cmd_line);
    }

    free(chain->commands);
    free(chain);
}

// Execute a command chain
int chain_execute(CommandChain *chain) {
    if (!chain || chain->count == 0) return 1;

    int last_exit_code = 0;
    int shell_continue = 1;

    for (int i = 0; i < chain->count; i++) {
        ChainedCommand *cmd = &chain->commands[i];

        // Check if we should execute this command based on previous exit code
        if (i > 0) {
            ChainOp prev_op = chain->commands[i - 1].next_op;

            if (prev_op == CHAIN_AND && last_exit_code != 0) {
                // && and previous failed, skip this command
                continue;
            }

            if (prev_op == CHAIN_OR && last_exit_code == 0) {
                // || and previous succeeded, skip this command
                continue;
            }
        }

        // Parse and execute command
        char *line_copy = strdup(cmd->cmd_line);
        if (!line_copy) continue;

        char **args = parse_line(line_copy);
        if (args) {
            shell_continue = execute(args);
            last_exit_code = execute_get_last_exit_code();

            free(args);
        }

        free(line_copy);

        // If command was "exit", stop processing chain
        if (shell_continue == 0) {
            return 0;
        }
    }

    return shell_continue;
}
