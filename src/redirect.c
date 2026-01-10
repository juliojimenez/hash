#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include "redirect.h"
#include "safe_string.h"
#include "hash.h"

#define MAX_REDIRECTS 16

// Create new redirection info
static RedirInfo *create_redir_info(void) {
    RedirInfo *info = malloc(sizeof(RedirInfo));
    if (!info) return NULL;

    info->redirs = calloc(MAX_REDIRECTS, sizeof(Redirection));
    if (!info->redirs) {
        free(info);
        return NULL;
    }

    info->count = 0;
    info->args = NULL;
    return info;
}

// Add a redirection
static int add_redirection(RedirInfo *info, RedirType type, const char *filename) {
    if (!info || info->count >= MAX_REDIRECTS) return -1;

    info->redirs[info->count].type = type;

    if (filename) {
        info->redirs[info->count].filename = strdup(filename);
        if (!info->redirs[info->count].filename) {
            return -1;
        }
    } else {
        info->redirs[info->count].filename = NULL;
    }

    info->count++;
    return 0;
}

// Parse redirections from args
RedirInfo *redirect_parse(char **args) {
    if (!args || !args[0]) return NULL;

    RedirInfo *info = create_redir_info();
    if (!info) return NULL;

    // Count args
    int arg_count = 0;
    while (args[arg_count]) arg_count++;

    // Allocate new args array (worst case: same size)
    info->args = calloc(arg_count + 1, sizeof(char *));
    if (!info->args) {
        redirect_free(info);
        return NULL;
    }

    int new_arg_idx = 0;

    for (int i = 0; args[i] != NULL; i++) {
        char *arg = args[i];

        // Check for standalone redirection operators
        if (strcmp(arg, "<") == 0) {
            // Input redirection
            if (args[i + 1]) {
                add_redirection(info, REDIR_INPUT, args[i + 1]);
                i++;  // Skip filename
            }
        } else if (strcmp(arg, ">") == 0) {
            // Output redirection
            if (args[i + 1]) {
                add_redirection(info, REDIR_OUTPUT, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, ">>") == 0) {
            // Append redirection
            if (args[i + 1]) {
                add_redirection(info, REDIR_APPEND, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "2>") == 0) {
            // Error redirection
            if (args[i + 1]) {
                add_redirection(info, REDIR_ERROR, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "2>>") == 0) {
            // Error append
            if (args[i + 1]) {
                add_redirection(info, REDIR_ERROR_APPEND, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "&>") == 0) {
            // Both stdout and stderr
            if (args[i + 1]) {
                add_redirection(info, REDIR_BOTH, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "2>&1") == 0) {
            // Redirect stderr to stdout
            add_redirection(info, REDIR_ERROR_TO_OUT, NULL);
        } else if (strncmp(arg, "2>&", 3) == 0 && arg[3] == '1' && arg[4] == '\0') {
            // Handle 2>&1 as single token
            add_redirection(info, REDIR_ERROR_TO_OUT, NULL);
        } else if (strcmp(arg, ">&2") == 0 || strcmp(arg, "1>&2") == 0) {
            // Redirect stdout to stderr
            add_redirection(info, REDIR_OUT_TO_ERROR, NULL);
        }
        // Handle attached redirections: >file, <file, >>file, etc.
        else if (arg[0] == '<' && arg[1] != '\0' && arg[1] != '&') {
            // <file (input redirection with attached filename)
            add_redirection(info, REDIR_INPUT, arg + 1);
        } else if (arg[0] == '>' && arg[1] != '\0') {
            // >file or >>file or >&
            if (arg[1] == '>') {
                // >>file or >> file
                if (arg[2] != '\0') {
                    add_redirection(info, REDIR_APPEND, arg + 2);
                } else if (args[i + 1]) {
                    add_redirection(info, REDIR_APPEND, args[i + 1]);
                    i++;
                }
            } else if (arg[1] == '&') {
                // >&N
                if (arg[2] == '2' && arg[3] == '\0') {
                    add_redirection(info, REDIR_OUT_TO_ERROR, NULL);
                } else {
                    info->args[new_arg_idx++] = arg;
                }
            } else {
                // >file
                add_redirection(info, REDIR_OUTPUT, arg + 1);
            }
        }
        // Handle N>file, N<file, N>>file patterns
        else if (isdigit(arg[0])) {
            const char *p = arg;
            while (*p && isdigit(*p)) p++;

            if (*p == '>' || *p == '<') {
                int fd = atoi(arg);
                if (*p == '<') {
                    p++;
                    if (*p == '&') {
                        // N<&M - not fully supported
                        info->args[new_arg_idx++] = arg;
                    } else if (*p != '\0') {
                        // N<file
                        if (fd == 0) {
                            add_redirection(info, REDIR_INPUT, p);
                        } else {
                            info->args[new_arg_idx++] = arg;
                        }
                    } else if (args[i + 1]) {
                        if (fd == 0) {
                            add_redirection(info, REDIR_INPUT, args[i + 1]);
                            i++;
                        } else {
                            info->args[new_arg_idx++] = arg;
                        }
                    }
                } else {
                    // N> or N>>
                    p++;
                    int append = 0;
                    if (*p == '>') {
                        append = 1;
                        p++;
                    }
                    if (*p == '&') {
                        // N>&M
                        p++;
                        if (*p == '1' && *(p + 1) == '\0' && fd == 2) {
                            add_redirection(info, REDIR_ERROR_TO_OUT, NULL);
                        } else if (*p == '2' && *(p + 1) == '\0' && fd == 1) {
                            add_redirection(info, REDIR_OUT_TO_ERROR, NULL);
                        } else {
                            info->args[new_arg_idx++] = arg;
                        }
                    } else if (*p != '\0') {
                        // N>file or N>>file
                        if (fd == 1) {
                            add_redirection(info, append ? REDIR_APPEND : REDIR_OUTPUT, p);
                        } else if (fd == 2) {
                            add_redirection(info, append ? REDIR_ERROR_APPEND : REDIR_ERROR, p);
                        } else {
                            info->args[new_arg_idx++] = arg;
                        }
                    } else if (args[i + 1]) {
                        if (fd == 1) {
                            add_redirection(info, append ? REDIR_APPEND : REDIR_OUTPUT, args[i + 1]);
                            i++;
                        } else if (fd == 2) {
                            add_redirection(info, append ? REDIR_ERROR_APPEND : REDIR_ERROR, args[i + 1]);
                            i++;
                        } else {
                            info->args[new_arg_idx++] = arg;
                        }
                    }
                }
            } else {
                // Regular argument starting with digit
                info->args[new_arg_idx++] = arg;
            }
        } else {
            // Regular argument - keep it
            info->args[new_arg_idx++] = arg;
        }
    }

    info->args[new_arg_idx] = NULL;

    return info;
}

// Apply redirections
int redirect_apply(const RedirInfo *info) {
    if (!info) return 0;

    for (int i = 0; i < info->count; i++) {
        const Redirection *redir = &info->redirs[i];

        switch (redir->type) {
            case REDIR_INPUT: {
                // < file
                int fd = open(redir->filename, O_RDONLY);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                break;
            }

            case REDIR_OUTPUT: {
                // > file
                int fd = open(redir->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
            }

            case REDIR_APPEND: {
                // >> file
                int fd = open(redir->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
            }

            case REDIR_ERROR: {
                // 2> file
                int fd = open(redir->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
            }

            case REDIR_ERROR_APPEND: {
                // 2>> file
                int fd = open(redir->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
            }

            case REDIR_BOTH: {
                // &> file (both stdout and stderr)
                int fd = open(redir->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror(HASH_NAME);
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
            }

            case REDIR_ERROR_TO_OUT: {
                // 2>&1 (stderr to stdout)
                dup2(STDOUT_FILENO, STDERR_FILENO);
                break;
            }

            case REDIR_OUT_TO_ERROR: {
                // >&2 or 1>&2 (stdout to stderr)
                dup2(STDERR_FILENO, STDOUT_FILENO);
                break;
            }

            case REDIR_NONE:
            default:
                break;
        }
    }

    return 0;
}

// Free redirection info
void redirect_free(RedirInfo *info) {
    if (!info) return;

    if (info->redirs) {
        for (int i = 0; i < info->count; i++) {
            free(info->redirs[i].filename);
        }
        free(info->redirs);
    }

    // Don't free info->args - those are pointers into original args array
    free(info->args);
    free(info);
}
