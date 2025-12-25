#ifndef CHAIN_H
#define CHAIN_H

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
} ChainedCommand;

// A list of chained commands
typedef struct {
    ChainedCommand *commands;
    int count;
    int capacity;
} CommandChain;

// Parse a line into chained commands
CommandChain *chain_parse(char *line);

// Free a command chain
void chain_free(CommandChain *chain);

// Execute a command chain
// Returns 1 to continue shell loop, 0 to exit
int chain_execute(CommandChain *chain);

#endif // CHAIN_H
