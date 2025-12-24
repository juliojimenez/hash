#ifndef PROMPT_H
#define PROMPT_H

#include <stdbool.h>

#define MAX_PROMPT_LENGTH 2048

// Prompt configuration
typedef struct {
    char ps1[MAX_PROMPT_LENGTH];
    bool use_custom_ps1;
} PromptConfig;

extern PromptConfig prompt_config;

// Initialize prompt system
void prompt_init(void);

// Generate and return the prompt string
// last_exit_code: exit code of previous command
char *prompt_generate(int last_exit_code);

// Set custom PS1
void prompt_set_ps1(const char *ps1);

// Get current git branch (returns NULL if not in a repo)
char *prompt_git_branch(void);

// Check if git repo has uncommitted changes
bool prompt_git_dirty(void);

// Get current working directory
char *prompt_get_cwd(void);

// Get current directory name only (not full path)
char *prompt_get_current_dir(void);

// Get username
char *prompt_get_user(void);

// Get hostname
char *prompt_get_hostname(void);

#endif // PROMPT_H
