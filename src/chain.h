#ifndef CHAIN_H
#define CHAIN_H

#include <stdbool.h>

// Chain operator types
typedef enum {
    CHAIN_NONE,      // No chaining (single command)
    CHAIN_ALWAYS,    // ; (always execute next)
    CHAIN_AND,       // && (execute next only if previous succeeded)
    CHAIN_OR         // || (execute next only if previous failed)
} ChainOp;

// A single command in a chain
typedef struct {
    char *cmd_line;
    ChainOp next_op;
    bool background;  // Run in background (&)
} ChainedCommand;

// A list of chained commands
typedef struct {
    ChainedCommand *commands;
    int count;
    int capacity;
    bool background;  // Entire chain runs in background
} CommandChain;

/**
 * Parse a line into chained commands
 *
 * @param line The line to parse
 *
 * @return Returns pointer to CommandChain or NULL on failure.
 */
CommandChain *chain_parse(char *line);

/**
 * Free a command chain
 *
 * @param chain The command chain to free
 */
void chain_free(CommandChain *chain);

/**
 * Execute a command chain
 *
 * @param chain The command chain to execute
 *
 * @return Returns 1 to continue shell loop, 0 to exit
 */
int chain_execute(const CommandChain *chain);

#endif // CHAIN_H
