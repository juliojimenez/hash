#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "chain.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "safe_string.h"
#include "pipeline.h"
#include "jobs.h"
#include "hash.h"
#include "script.h"
#include "trap.h"
#include "config.h"

#define INITIAL_CHAIN_CAPACITY 8

// Debug flag - matches execute.c
#define DEBUG_EXIT_CODE 0

// Create a new command chain
static CommandChain *chain_create(void) {
    CommandChain *chain = malloc(sizeof(CommandChain));
    if (!chain) return NULL;

    chain->capacity = INITIAL_CHAIN_CAPACITY;
    chain->count = 0;
    chain->background = false;
    chain->commands = malloc(chain->capacity * sizeof(ChainedCommand));

    if (!chain->commands) {
        free(chain);
        return NULL;
    }

    return chain;
}

// Add a command to the chain
static int chain_add(CommandChain *chain, const char *cmd_line, ChainOp next_op, bool background) {
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        ChainedCommand *new_cmds = realloc(chain->commands, chain->capacity * sizeof(ChainedCommand));
        if (!new_cmds) return -1;
        chain->commands = new_cmds;
    }

    chain->commands[chain->count].cmd_line = strdup(cmd_line);
    if (!chain->commands[chain->count].cmd_line) return -1;

    chain->commands[chain->count].next_op = next_op;
    chain->commands[chain->count].background = background;
    chain->count++;

    return 0;
}

// Check if command ends with & (background operator)
// Returns true if & found and removes it from the string
static bool check_background(char *cmd) {
    if (!cmd) return false;

    size_t len = strlen(cmd);
    if (len == 0) return false;

    // Find the last non-whitespace character
    char *end = cmd + len - 1;
    while (end > cmd && isspace(*end)) {
        end--;
    }

    // Check if it's &
    if (*end == '&') {
        // Make sure it's not && (AND operator)
        if (end > cmd && *(end - 1) == '&') {
            return false;  // It's &&, not background
        }

        // Make sure it's not escaped with backslash (\&)
        if (end > cmd && *(end - 1) == '\\') {
            return false;  // It's escaped, not a background operator
        }

        // Remove the &
        *end = '\0';

        // Trim any trailing whitespace
        end--;
        while (end > cmd && isspace(*end)) {
            *end = '\0';
            end--;
        }

        return true;
    }

    return false;
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
    int paren_depth = 0;  // Track $() and $(()), () depth
    int brace_depth = 0;  // Track {} brace groups
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

        // Track (), $() and $(()) depth when not in single quotes
        // Need to track both subshells () and command substitutions $()
        if (!in_single_quote && !in_double_quote) {
            if (*current == '(') {
                paren_depth++;
            } else if (*current == ')' && paren_depth > 0) {
                paren_depth--;
            }
            // Track brace groups { }
            if (*current == '{') {
                brace_depth++;
            } else if (*current == '}' && brace_depth > 0) {
                brace_depth--;
            }
        } else if (!in_single_quote && in_double_quote) {
            // Inside double quotes, only track $()
            if (*current == '$' && *(current + 1) == '(') {
                paren_depth++;
                current += 2;  // Skip past $(
                continue;
            } else if (*current == ')' && paren_depth > 0) {
                paren_depth--;
            }
        }

        // Look for operators outside quotes, command substitution, and brace groups
        if (!in_single_quote && !in_double_quote && paren_depth == 0 && brace_depth == 0) {
            // Check for comment - # starts a comment that extends to end of line
            // Must be preceded by whitespace or be at start of command
            if (*current == '#' && (current == cmd_start || isspace(*(current - 1)))) {
                // Terminate the line at the comment
                *current = '\0';
                break;  // Exit the parsing loop
            }

            ChainOp op = CHAIN_NONE;
            int op_len = 0;

            // Check for &&
            if (*current == '&' && *(current + 1) == '&') {
                op = CHAIN_AND;
                op_len = 2;
            }
            // Check for single & (background + continue) - must not be followed by &
            // and must not be part of >&N or N>&M redirection (where & is preceded by > and/or followed by digit)
            else if (*current == '&' && *(current + 1) != '&' &&
                     !(current > cmd_start && *(current - 1) == '>') &&
                     !isdigit(*(current + 1))) {
                // Single & acts as separator - command before it runs in background
                // Null-terminate at & position
                *current = '\0';

                // Trim whitespace from command
                safe_trim(cmd_start);

                // Add command to chain if not empty, marked as background
                if (*cmd_start != '\0') {
                    if (chain_add(chain, cmd_start, CHAIN_ALWAYS, true) != 0) {
                        chain_free(chain);
                        return NULL;
                    }
                }

                // Move past &
                current++;
                cmd_start = current;
                continue;
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
                // Null-terminate at operator position
                *current = '\0';

                // Trim whitespace from command
                safe_trim(cmd_start);

                // Check for background operator on this segment
                bool bg = check_background(cmd_start);

                // Add command to chain if not empty
                if (*cmd_start != '\0') {
                    if (chain_add(chain, cmd_start, op, bg) != 0) {
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
    safe_trim(cmd_start);

    // Check for trailing & on final command
    bool final_bg = check_background(cmd_start);

    if (*cmd_start != '\0') {
        if (chain_add(chain, cmd_start, CHAIN_NONE, final_bg) != 0) {
            chain_free(chain);
            return NULL;
        }
    }

    // If the last command is background, mark the whole chain
    if (chain->count > 0 && chain->commands[chain->count - 1].background) {
        chain->background = true;
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

// Execute a single command in background
static int execute_background(const char *cmd_line) {
    // Flush stdout/stderr before forking to prevent child from inheriting
    // buffered content that it would then flush on exit
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();

    if (pid == -1) {
        perror(HASH_NAME);
        return -1;
    }

    if (pid == 0) {
        // Child process

        // Create new process group
        setpgid(0, 0);

        // POSIX: Asynchronous (background) commands should ignore SIGINT and SIGQUIT
        // This prevents background jobs from being killed by keyboard interrupts
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);

        // Redirect stdin from /dev/null to prevent interference with parent's stdin reading
        // This is critical when the shell is reading from a pipe
        // Simply closing isn't enough - we need to replace it to avoid fd reuse issues
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull != STDIN_FILENO) {
                close(devnull);
            }
        }

        // Reset traps for this subprocess
        trap_reset_for_subshell();
        // POSIX: break/continue only affect loops in this subshell
        script_reset_for_subshell();

        extern int last_command_exit_code;

        // Optimization: If command is a subshell "( ... )", extract content and
        // execute directly to avoid extra fork. The background fork IS the subshell.
        // This also ensures $PPID returns correct value for commands inside.
        const char *p = cmd_line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '(') {
            // Find matching closing paren
            const char *start = p + 1;
            int depth = 1;
            const char *end = start;
            while (*end && depth > 0) {
                if (*end == '(') depth++;
                else if (*end == ')') depth--;
                if (depth > 0) end++;
            }
            if (depth == 0) {
                // Extract subshell content
                size_t len = end - start;
                char *subshell_cmd = malloc(len + 1);
                if (subshell_cmd) {
                    memcpy(subshell_cmd, start, len);
                    subshell_cmd[len] = '\0';
                    // Execute directly without extra fork
                    script_execute_string(subshell_cmd);
                    free(subshell_cmd);
                    fflush(stdout);
                    fflush(stderr);
                    trap_execute_exit();
                    _exit(last_command_exit_code);
                }
            }
        }

        // Use script_process_line to handle all command types
        // (brace groups, control structures, etc.)
        script_process_line(cmd_line);
        fflush(stdout);
        fflush(stderr);
        _exit(last_command_exit_code);
    }

    // Parent process
    // Set the last background PID for $! expansion
    jobs_set_last_bg_pid(pid);

    // Add job to job table (needed for jobs command and $!)
    int job_id = jobs_add(pid, cmd_line);

    // Only print job notification in interactive mode with job control
    if (job_id > 0 && isatty(STDIN_FILENO) && shell_option_monitor()) {
        printf("[%d] %d\n", job_id, pid);
    }

    return 0;
}

// Execute a command chain
int chain_execute(const CommandChain *chain) {
    if (!chain || chain->count == 0) return 1;

    int last_exit_code = 0;
    int shell_continue = 1;

    for (int i = 0; i < chain->count; i++) {
        const ChainedCommand *cmd = &chain->commands[i];

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

        // Check if this command should run in background
        if (cmd->background) {
            execute_background(cmd->cmd_line);
            last_exit_code = 0;  // Background commands always "succeed" immediately
            continue;
        }

        // Parse and execute command
        char *line_copy = strdup(cmd->cmd_line);
        if (!line_copy) continue;

        // Skip leading whitespace
        const char *trimmed = line_copy;
        while (*trimmed && isspace(*trimmed)) trimmed++;

        // Check for pipeline negation: ! command
        // POSIX: ! inverts the exit status of the pipeline
        bool negate = false;
        if (*trimmed == '!' && (isspace(*(trimmed + 1)) || *(trimmed + 1) == '\0')) {
            negate = true;
            trimmed++;
            while (*trimmed && isspace(*trimmed)) trimmed++;
        }

        // Check for subshell syntax: (commands) [redirections]
        if (*trimmed == '(') {
            // Find matching closing paren by tracking depth
            const char *p = trimmed + 1;
            int depth = 1;
            int in_single_quote = 0;
            int in_double_quote = 0;

            while (*p && depth > 0) {
                if (*p == '\'' && !in_double_quote) {
                    in_single_quote = !in_single_quote;
                } else if (*p == '"' && !in_single_quote) {
                    in_double_quote = !in_double_quote;
                } else if (!in_single_quote && !in_double_quote) {
                    if (*p == '(') depth++;
                    else if (*p == ')') depth--;
                }
                if (depth > 0) p++;
            }

            if (depth == 0 && *p == ')') {
                // p points to matching ')'
                const char *end_paren = p;

                // Extract subshell content
                size_t len = (size_t)(end_paren - (trimmed + 1));
                char *subshell_cmd = malloc(len + 1);
                if (subshell_cmd) {
                    memcpy(subshell_cmd, trimmed + 1, len);
                    subshell_cmd[len] = '\0';

                    // Check for redirections after the closing paren
                    const char *after_paren = end_paren + 1;
                    while (*after_paren && isspace(*after_paren)) after_paren++;
                    char *redir_str = NULL;
                    if (*after_paren) {
                        redir_str = strdup(after_paren);
                    }

                    // Flush before fork
                    fflush(stdout);
                    fflush(stderr);

                    pid_t pid = fork();
                    if (pid == 0) {
                        // Child process - apply external redirections first
                        if (redir_str) {
                            char *redir_copy = strdup(redir_str);
                            if (redir_copy) {
                                char *r = redir_copy;
                                while (*r) {
                                    while (*r && isspace(*r)) r++;
                                    if (!*r) break;

                                    int fd = -1;
                                    if (isdigit(*r)) {
                                        fd = 0;
                                        while (isdigit(*r)) {
                                            fd = fd * 10 + (*r - '0');
                                            r++;
                                        }
                                    }

                                    if (*r == '<') {
                                        r++;
                                        if (fd < 0) fd = 0;
                                        if (*r == '&') {
                                            r++;
                                            if (*r == '-') { close(fd); r++; }
                                            else { int src = atoi(r); while (isdigit(*r)) { r++; } dup2(src, fd); }
                                        } else {
                                            while (*r && isspace(*r)) r++;
                                            const char *fn = r;
                                            while (*r && !isspace(*r)) r++;
                                            char sv = *r; *r = '\0';
                                            int nfd = open(fn, O_RDONLY);
                                            if (nfd >= 0) { if (nfd != fd) { dup2(nfd, fd); close(nfd); } }
                                            *r = sv;
                                        }
                                    } else if (*r == '>') {
                                        r++;
                                        if (fd < 0) fd = 1;
                                        int app = 0;
                                        if (*r == '>') { app = 1; r++; }
                                        if (*r == '&') {
                                            r++;
                                            if (*r == '-') { close(fd); r++; }
                                            else { int src = atoi(r); while (isdigit(*r)) { r++; } dup2(src, fd); }
                                        } else {
                                            while (*r && isspace(*r)) r++;
                                            const char *fn = r;
                                            while (*r && !isspace(*r)) r++;
                                            char sv = *r; *r = '\0';
                                            int fl = O_WRONLY | O_CREAT | (app ? O_APPEND : O_TRUNC);
                                            int nfd = open(fn, fl, 0644);
                                            if (nfd >= 0) { if (nfd != fd) { dup2(nfd, fd); close(nfd); } }
                                            *r = sv;
                                        }
                                    } else { r++; }
                                }
                                free(redir_copy);
                            }
                            free(redir_str);
                        }
                        trap_reset_for_subshell();
                        script_reset_for_subshell();
                        int exit_code = script_execute_string(subshell_cmd);
                        free(subshell_cmd);
                        trap_execute_exit();
                        fflush(stdout);
                        fflush(stderr);
                        _exit(exit_code);
                    } else if (pid > 0) {
                        // Parent process
                        free(subshell_cmd);
                        free(redir_str);
                        int status;
                        waitpid(pid, &status, 0);
                        extern int last_command_exit_code;
                        if (WIFEXITED(status)) {
                            last_command_exit_code = WEXITSTATUS(status);
                        } else {
                            last_command_exit_code = 1;
                        }
                        // Apply negation if needed
                        if (negate) {
                            last_command_exit_code = (last_command_exit_code == 0) ? 1 : 0;
                        }
                        last_exit_code = last_command_exit_code;
                    } else {
                        free(subshell_cmd);
                        free(redir_str);
                        last_exit_code = 1;
                    }
                }
                free(line_copy);
                continue;
            }
        }

        // If negation flag is set but no subshell, we need to execute the rest
        // and negate the exit code
        // Need to make a copy since pipeline_parse/parse_line may modify the string
        char *exec_cmd = negate ? strdup(trimmed) : line_copy;
        if (negate && !exec_cmd) {
            free(line_copy);
            continue;
        }

        // Check if this command contains pipes
        Pipeline *pipe = pipeline_parse(exec_cmd);

        if (pipe) {
            // Execute as pipeline
            int pipe_exit = pipeline_execute(pipe);

            // Update global exit code
            extern int last_command_exit_code;
            last_command_exit_code = pipe_exit;
            // Apply negation if needed, but NOT if return was called
            if (negate && !script_get_return_pending()) {
                last_command_exit_code = (last_command_exit_code == 0) ? 1 : 0;
            }
            last_exit_code = last_command_exit_code;

            pipeline_free(pipe);
        } else {
            // No pipes - execute normally
            ParseResult parsed = parse_line(exec_cmd);
            if (parsed.tokens) {
                shell_continue = execute(parsed.tokens);
                last_exit_code = execute_get_last_exit_code();
                // Apply negation if needed, but NOT if return was called
                // (return's exit code should not be negated)
                if (negate && !script_get_return_pending()) {
                    extern int last_command_exit_code;
                    last_command_exit_code = (last_command_exit_code == 0) ? 1 : 0;
                    last_exit_code = last_command_exit_code;
                }
#if DEBUG_EXIT_CODE
                fprintf(stderr, "DEBUG: chain_execute() after execute, last_exit_code=%d\n", last_exit_code);
#endif
                parse_result_free(&parsed);
            }
        }

        if (negate) free(exec_cmd);
        free(line_copy);

        // If command was "exit", stop processing chain
        if (shell_continue == 0) {
            return 0;
        }

        // If break, continue, or return was called, stop processing chain
        if (script_get_break_pending() > 0 || script_get_continue_pending() > 0 ||
            script_get_return_pending()) {
            return shell_continue;
        }

        // Check errexit option: exit if command failed and we're not in a special context
        // POSIX says errexit should NOT trigger if:
        // - We're in an if/while/until condition (script_get_in_condition())
        // - The command is part of a && or || chain being tested (cmd->next_op)
        // - The command was negated with !
        if (shell_option_errexit() && last_exit_code != 0 && !negate) {
            // Don't trigger errexit if command is left side of && or ||
            if (cmd->next_op != CHAIN_AND && cmd->next_op != CHAIN_OR) {
                // Don't trigger errexit if in condition context
                if (!script_get_in_condition()) {
                    // Errexit triggers - exit the shell
                    return 0;
                }
            }
        }
    }

    return shell_continue;
}
