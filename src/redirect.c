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
#include "cmdsub.h"
#include "varexpand.h"
#include "execute.h"

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
    info->redirs[info->count].heredoc_delim = NULL;
    info->redirs[info->count].heredoc_content = NULL;
    info->redirs[info->count].dest_fd = -1;
    info->redirs[info->count].src_fd = -1;

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

// Add an FD duplication redirection (N>&M)
static int add_fd_dup(RedirInfo *info, int dest_fd, int src_fd) {
    if (!info || info->count >= MAX_REDIRECTS) return -1;

    info->redirs[info->count].type = REDIR_FD_DUP;
    info->redirs[info->count].filename = NULL;
    info->redirs[info->count].heredoc_delim = NULL;
    info->redirs[info->count].heredoc_content = NULL;
    info->redirs[info->count].dest_fd = dest_fd;
    info->redirs[info->count].src_fd = src_fd;

    info->count++;
    return 0;
}

// Add a heredoc redirection
static int add_heredoc(RedirInfo *info, RedirType type, const char *delimiter) {
    if (!info || info->count >= MAX_REDIRECTS) return -1;

    info->redirs[info->count].type = type;
    info->redirs[info->count].filename = NULL;
    info->redirs[info->count].heredoc_content = NULL;

    if (delimiter) {
        info->redirs[info->count].heredoc_delim = strdup(delimiter);
        if (!info->redirs[info->count].heredoc_delim) {
            return -1;
        }
    } else {
        info->redirs[info->count].heredoc_delim = NULL;
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
        } else if (strcmp(arg, "<<-") == 0) {
            // Heredoc with tab stripping
            if (args[i + 1]) {
                add_heredoc(info, REDIR_HEREDOC_NOTAB, args[i + 1]);
                i++;
            }
        } else if (strcmp(arg, "<<") == 0) {
            // Heredoc
            if (args[i + 1]) {
                add_heredoc(info, REDIR_HEREDOC, args[i + 1]);
                i++;
            }
        } else if (strncmp(arg, "<<-", 3) == 0 && arg[3] != '\0') {
            // <<-DELIM attached
            add_heredoc(info, REDIR_HEREDOC_NOTAB, arg + 3);
        } else if (strncmp(arg, "<<", 2) == 0 && arg[2] != '\0' && arg[2] != '-') {
            // <<DELIM attached
            add_heredoc(info, REDIR_HEREDOC, arg + 2);
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
        } else if (strncmp(arg, "<&", 2) == 0 && isdigit(arg[2])) {
            // <&N - dup fd N to stdin
            add_redirection(info, REDIR_INPUT_DUP, arg + 2);
        } else if (strncmp(arg, ">&", 2) == 0 && isdigit(arg[2]) && arg[2] != '2') {
            // >&N where N != 2 - dup fd N to stdout
            add_redirection(info, REDIR_OUTPUT_DUP, arg + 2);
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
                        } else if (isdigit(*p)) {
                            // General FD duplication: N>&M
                            int src_fd = atoi(p);
                            add_fd_dup(info, fd, src_fd);
                        } else {
                            // Unknown format, keep as argument
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

            case REDIR_HEREDOC:
            case REDIR_HEREDOC_NOTAB: {
                // << DELIMITER or <<- DELIMITER
                if (redir->heredoc_content) {
                    int pipefd[2];
                    if (pipe(pipefd) == -1) {
                        perror(HASH_NAME);
                        return -1;
                    }

                    // If delimiter was not quoted, expand variables and command substitutions
                    const char *content = redir->heredoc_content;
                    char *expanded = NULL;
                    if (!redir->heredoc_quoted) {
                        // Clear error flag before expansion
                        varexpand_clear_error();

                        // Apply command substitution expansion
                        char *cmdsub_result = cmdsub_expand(content);
                        if (cmdsub_result) {
                            content = cmdsub_result;
                            expanded = cmdsub_result;
                        }
                        // Apply variable expansion
                        char *var_result = varexpand_expand(content, execute_get_last_exit_code());
                        if (var_result) {
                            free(expanded);
                            content = var_result;
                            expanded = var_result;
                        }

                        // Check for expansion error (e.g., ${x?word} with unset x)
                        if (varexpand_had_error()) {
                            free(expanded);
                            close(pipefd[0]);
                            close(pipefd[1]);
                            return -1;
                        }
                    }

                    // Write heredoc content to pipe
                    size_t len = strlen(content);
                    ssize_t written = write(pipefd[1], content, len);
                    (void)written;  // Ignore write result for simplicity

                    free(expanded);  // Free expanded content if any

                    close(pipefd[1]);  // Close write end

                    // Replace stdin with read end of pipe
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
                }
                break;
            }

            case REDIR_INPUT_DUP: {
                // <&N - dup fd N to stdin
                int src_fd = atoi(redir->filename);
                if (dup2(src_fd, STDIN_FILENO) < 0) {
                    fprintf(stderr, "%s: %s: Bad file descriptor\n", HASH_NAME, redir->filename);
                    return -1;
                }
                break;
            }

            case REDIR_OUTPUT_DUP: {
                // >&N - dup fd N to stdout
                int src_fd = atoi(redir->filename);
                if (dup2(src_fd, STDOUT_FILENO) < 0) {
                    fprintf(stderr, "%s: %s: Bad file descriptor\n", HASH_NAME, redir->filename);
                    return -1;
                }
                break;
            }

            case REDIR_FD_DUP: {
                // N>&M - dup fd M (src) to fd N (dest)
                if (dup2(redir->src_fd, redir->dest_fd) < 0) {
                    fprintf(stderr, "%s: %d: Bad file descriptor\n", HASH_NAME, redir->src_fd);
                    return -1;
                }
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
            free(info->redirs[i].heredoc_delim);
            free(info->redirs[i].heredoc_content);
        }
        free(info->redirs);
    }

    // Don't free info->args - those are pointers into original args array
    free(info->args);
    free(info);
}

// Check if a line contains a heredoc operator
int redirect_has_heredoc(const char *line) {
    if (!line) return 0;

    const char *p = line;
    int in_single_quote = 0;
    int in_double_quote = 0;

    while (*p) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote) {
            if (*p == '<' && *(p + 1) == '<') {
                return 1;
            }
        }
        p++;
    }
    return 0;
}

// Extract heredoc delimiter from a line
char *redirect_get_heredoc_delim(const char *line, int *strip_tabs, int *quoted) {
    if (!line) return NULL;

    *strip_tabs = 0;
    *quoted = 0;
    const char *p = line;
    int in_single_quote = 0;
    int in_double_quote = 0;

    while (*p) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote) {
            if (*p == '<' && *(p + 1) == '<') {
                p += 2;  // Skip <<

                // Check for <<-
                if (*p == '-') {
                    *strip_tabs = 1;
                    p++;
                }

                // Skip whitespace
                while (*p && isspace(*p)) p++;

                // Extract delimiter, handling quotes
                // POSIX: <<'EOF' means delimiter is EOF (no expansion)
                //        <<"EOF" means delimiter is EOF (no expansion)
                //        <<EOF means delimiter is EOF (full expansion)
                const char *start = p;
                char quote_char = 0;

                // Check if delimiter is quoted
                if (*p == '\'' || *p == '"') {
                    quote_char = *p;
                    *quoted = 1;  // Mark as quoted - no expansion
                    p++;  // Skip opening quote
                    start = p;
                    // Find closing quote
                    while (*p && *p != quote_char && *p != '\n') p++;
                    if (*p == quote_char) {
                        // Found closing quote - extract content between quotes
                        size_t len = p - start;
                        char *delim = malloc(len + 1);
                        if (delim) {
                            memcpy(delim, start, len);
                            delim[len] = '\0';
                            return delim;
                        }
                    }
                } else {
                    // Unquoted delimiter - expansion happens
                    while (*p && !isspace(*p) && *p != '\n') p++;

                    if (start != p) {
                        size_t len = p - start;
                        char *delim = malloc(len + 1);
                        if (delim) {
                            memcpy(delim, start, len);
                            delim[len] = '\0';
                            return delim;
                        }
                    }
                }
                return NULL;
            }
        }
        p++;
    }
    return NULL;
}

// Set heredoc content for a redirection info
void redirect_set_heredoc_content(RedirInfo *info, const char *content, int quoted) {
    if (!info || !content) return;

    for (int i = 0; i < info->count; i++) {
        if (info->redirs[i].type == REDIR_HEREDOC ||
            info->redirs[i].type == REDIR_HEREDOC_NOTAB) {
            free(info->redirs[i].heredoc_content);
            info->redirs[i].heredoc_content = strdup(content);
            info->redirs[i].heredoc_quoted = quoted;
            return;
        }
    }
}
