/*

hash - Command line interface (shell) for the Linux operating system.
https://github.com/juliojimenez/hash
Apache 2.0

Julio Jimenez, julio@julioj.com

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "hash.h"
#include "parser.h"
#include "execute.h"
#include "colors.h"
#include "config.h"
#include "prompt.h"
#include "chain.h"
#include "lineedit.h"
#include "history.h"
#include "completion.h"
#include "builtins.h"
#include "jobs.h"
#include "script.h"
#include "update.h"
#include "shellvar.h"
#include "trap.h"
#include "color_config.h"
#include "syntax.h"

// Shell process group ID
static pid_t shell_pgid;

// Track if this is a login shell (needed for logout handling)
static bool is_login_shell_global = false;

// is_interactive is defined in config.c

// Signal handler for cleanup
static void signal_handler(int sig) {
    (void)sig;
    lineedit_cleanup();
    printf("\n");
    exit(EXIT_SUCCESS);
}

// Initialize job control for the shell
static void init_job_control(void) {
    // Check if we're running interactively
    if (!isatty(STDIN_FILENO)) {
        return;
    }

    // Loop until we're in the foreground
    while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp())) {
        kill(-shell_pgid, SIGTTIN);
    }

    // Ignore interactive and job-control signals in the shell
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // Put ourselves in our own process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        // EPERM means we're already a session leader (e.g., login shell via SSH)
        // This is fine - we're already in control
        if (errno != EPERM) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }
        // We're a session leader, so our pgid is already our pid
        shell_pgid = getpgrp();
    }

    // Grab control of the terminal
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

static void loop(void) {
    char *line;
    int status;
    int last_exit_code = 0;

    // Debug flag - matches execute.c
    #define DEBUG_EXIT_CODE 0

    do {
        // Check for completed background jobs before displaying prompt
        jobs_check_completed();

#if DEBUG_EXIT_CODE
        fprintf(stderr, "DEBUG: loop() before prompt_generate, execute_get_last_exit_code()=%d, last_exit_code=%d\n",
                execute_get_last_exit_code(), last_exit_code);
#endif

        const char *prompt_str = prompt_generate(last_exit_code);

        line = read_line(prompt_str);

        history_reset_position();

        // Expand history references
        char *expanded = history_expand(line);
        if (expanded) {
            printf("%s\n", expanded);
            free(line);
            line = expanded;
        }

        // Save the original line for history before chain_parse modifies it
        char *line_for_history = strdup(line);

        CommandChain *chain = chain_parse(line);

        if (chain) {
            // Add the original (unmodified) line to history
            if (line_for_history) {
                history_add(line_for_history);
            }

            // Execute the chain
            status = chain_execute(chain);

            // Get exit code for next prompt
            last_exit_code = execute_get_last_exit_code();

            // Free the chain
            chain_free(chain);
        } else {
            // Empty line or parse error
            status = 1;
            last_exit_code = 0;
        }

        // Free allocated memory
        free(line_for_history);
        free(line);
    } while(status);
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options] [script-file] [arguments...]\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -c string     Execute commands from string\n");
    printf("  -i            Force interactive mode\n");
    printf("  -l, --login   Run as a login shell\n");
    printf("  -s            Read commands from standard input\n");
    printf("  -v, --version Print version information\n");
    printf("  -h, --help    Show this help message\n");
    printf("\n");
    printf("If a script-file is provided, hash executes the script.\n");
    printf("Without arguments, hash runs interactively.\n");
}

static void print_version(void) {
    printf("%s version %s\n", HASH_NAME, HASH_VERSION);
    printf("A POSIX-compatible shell for Linux\n");
    printf("https://github.com/juliojimenez/hash\n");
}

int main(int argc, char *argv[]) {
    // Argument parsing state
    const char *command_string = NULL;      // -c argument
    const char *script_file = NULL;         // Script file to execute
    int script_argc = 0;              // Number of script arguments
    char **script_argv = NULL;        // Script arguments ($0, $1, $2, ...)
    bool force_interactive = false;   // -i flag
    bool read_stdin = false;          // -s flag

    // Determine if we're a login shell
    // A login shell is indicated by:
    // 1. argv[0] starting with '-' (e.g., "-hash" set by login/sshd)
    // 2. --login or -l flag passed as argument
    if (argc > 0 && argv[0][0] == '-') {
        is_login_shell_global = true;
    }

    // Parse command line options
    int i = 1;
    while (i < argc) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            // Option argument
            if (strcmp(argv[i], "-c") == 0) {
                // -c: Execute command string
                if (i + 1 >= argc) {
                    fprintf(stderr, "%s: -c: option requires an argument\n", HASH_NAME);
                    return 2;
                }
                command_string = argv[i + 1];
                i += 2;

                // Remaining arguments become positional parameters
                if (i < argc) {
                    script_argc = argc - i + 1;  // +1 for $0
                    script_argv = &argv[i - 1];  // $0 is the command string context
                    // Actually, for -c, $0 should be the shell name or specified
                    // Let's use remaining args starting from next one
                    script_argc = argc - i;
                    script_argv = &argv[i];
                }
                break;  // Stop option parsing

            } else if (strcmp(argv[i], "-i") == 0) {
                force_interactive = true;
                i++;

            } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--login") == 0) {
                is_login_shell_global = true;
                i++;

            } else if (strcmp(argv[i], "-s") == 0) {
                read_stdin = true;
                i++;

            } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                print_version();
                return 0;

            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;

            } else if (strcmp(argv[i], "--") == 0) {
                // End of options
                i++;
                break;

            } else {
                fprintf(stderr, "%s: %s: invalid option\n", HASH_NAME, argv[i]);
                print_usage(argv[0]);
                return 2;
            }
        } else {
            // Non-option argument - script file
            break;
        }
    }

    // If there are remaining arguments and no -c or -s, treat first as script file
    if (i < argc && command_string == NULL && !read_stdin) {
        script_file = argv[i];
        script_argc = argc - i;
        script_argv = &argv[i];
    }

    // Determine if we're running interactively
    is_interactive = force_interactive ||
                     (command_string == NULL &&
                      script_file == NULL &&
                      !read_stdin &&
                      isatty(STDIN_FILENO));

#ifdef DEBUG
    fprintf(stderr, "DEBUG: is_interactive=%d, is_login_shell_global=%d\n",
            is_interactive, is_login_shell_global);
    fprintf(stderr, "DEBUG: force_interactive=%d, command_string=%s, script_file=%s\n",
            force_interactive, command_string ? command_string : "(null)",
            script_file ? script_file : "(null)");
    fprintf(stderr, "DEBUG: read_stdin=%d, isatty(STDIN)=%d\n",
            read_stdin, isatty(STDIN_FILENO));
#endif

    // Initialize scripting subsystem (always needed)
    script_init();

    // Initialize shell variables (readonly, export tracking)
    shellvar_init();

    // Import environment variables (so they can be modified and synced back)
    shellvar_sync_from_env();

    // Initialize trap system
    trap_init();

    // Initialize colors
    colors_init();

    // Initialize color configuration with defaults
    color_config_init();

    // Initialize syntax highlighting
    syntax_init();

    // Initialize config with defaults
    config_init();

    // ========================================================================
    // Non-interactive mode: Execute command string (-c)
    // ========================================================================
    if (command_string != NULL) {
        // Set up positional parameters if provided
        if (script_argc > 0 && script_argv != NULL) {
            script_state.positional_params = malloc((size_t)script_argc * sizeof(char*));
            if (script_state.positional_params) {
                for (int j = 0; j < script_argc; j++) {
                    if (script_argv[j] != NULL) {
                        script_state.positional_params[j] = strdup(script_argv[j]);
                    } else {
                        script_state.positional_params[j] = strdup("");
                    }
                }
                script_state.positional_count = script_argc;
            }
        }

        // Execute the command string
        int result = script_execute_string(command_string);

        script_cleanup();
        return result;
    }

    // ========================================================================
    // Non-interactive mode: Execute script file
    // ========================================================================
    if (script_file != NULL) {
        // If -i flag was used, initialize history for the script
        if (force_interactive) {
            history_init();
        }

        int result = script_execute_file(script_file, script_argc, script_argv);

        // Execute EXIT trap before cleanup
        trap_execute_exit();
        script_cleanup();
        return result;
    }

    // ========================================================================
    // Non-interactive mode: Read from stdin (-s or piped input)
    // ========================================================================
    if (read_stdin || !isatty(STDIN_FILENO)) {
        // Set up positional parameters if provided (for -s)
        if (script_argc > 0 && script_argv != NULL) {
            script_state.positional_params = malloc((size_t)script_argc * sizeof(char*));
            if (script_state.positional_params) {
                for (int j = 0; j < script_argc; j++) {
                    if (script_argv[j] != NULL) {
                        script_state.positional_params[j] = strdup(script_argv[j]);
                    } else {
                        script_state.positional_params[j] = strdup("");
                    }
                }
                script_state.positional_count = script_argc;
            }
        }

        // Read and execute from stdin
        char line[MAX_LINE];
        script_state.in_script = true;

        int result = 1;
        while (1) {
            // Clear buffer before each read to prevent corruption
            // when lines don't end with newline
            memset(line, 0, sizeof(line));
            if (!fgets(line, sizeof(line), stdin)) break;
            // Remove trailing newline
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }

            result = script_process_line(line);
            // Exit if the exit command was called (returns 0)
            if (result == 0) {
                break;
            }
        }

        script_state.in_script = false;

        // Check for unclosed control structures
        if (script_state.context_depth > 0) {
            fprintf(stderr, "%s: unexpected end of file\n", HASH_NAME);
            trap_execute_exit();
            script_cleanup();
            return 1;
        }

        // Execute EXIT trap before cleanup
        trap_execute_exit();
        script_cleanup();
        return execute_get_last_exit_code();
    }

    // ========================================================================
    // Interactive mode
    // ========================================================================

    // Initialize job control (only for interactive shells)
    init_job_control();

    // Setup signal handler for clean terminal restoration on SIGTERM
    signal(SIGTERM, signal_handler);

    // Initialize line editor
    lineedit_init();

    // Initialize prompt system
    prompt_init();

    // Initialize history (loads from ~/.hash_history)
    history_init();

    // Initialize tab completion
    completion_init();

    // Initialize job control subsystem
    jobs_init();

    // Set login shell status for builtins (needed for logout command)
    builtins_set_login_shell(is_login_shell_global);

    // Load startup files based on shell type
    config_load_startup_files(is_login_shell_global);

    // Load color configuration from environment (after startup files)
    color_config_load_env();

    if (shell_config.show_welcome) {
        color_print(COLOR_BOLD COLOR_CYAN, "%s", HASH_NAME);
        printf(" v%s", HASH_VERSION);
        if (is_login_shell_global) {
            printf(" (login)");
        }
        printf("\n");
        printf("Type ");
        color_print(COLOR_YELLOW, "'exit'");
        printf(" to quit\n\n");
    }

    // Check for updates (if enabled and interval has passed)
    update_startup_check();

    // Run Command Loop
    loop();

    // Run logout scripts for login shells
    if (is_login_shell_global) {
        config_load_logout_files();
    }

    // Execute EXIT trap
    trap_execute_exit();

    // Cleanup
    lineedit_cleanup();
    script_cleanup();

    return EXIT_SUCCESS;
}
