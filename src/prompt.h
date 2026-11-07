#ifndef PROMPT_H
#define PROMPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_PROMPT_LENGTH 2048

// Prompt configuration
typedef struct {
    char ps1[MAX_PROMPT_LENGTH];
    bool use_custom_ps1;
} PromptConfig;

extern PromptConfig prompt_config;

// Initialize prompt system
void prompt_init(void);

// Set up default fancy prompt (call only when stdin is a tty)
void prompt_set_fancy_default(void);

// Generate and return the prompt string
// last_exit_code: exit code of previous command
char *prompt_generate(int last_exit_code);

// Continuation prompt (PS2) shown while a compound command is still open
const char *prompt_continuation(void);

// Set custom PS1
void prompt_set_ps1(const char *ps1);

/* Named prompt presets, selectable with `set prompt=NAME` */
typedef struct {
    const char *name;
    const char *ps1;
    const char *blurb;
} PromptPreset;

#define PROMPT_PRESET_DEFAULT "classic"

/* The preset table and its size */
const PromptPreset *prompt_presets(size_t *count);
/* Use a preset: PS1 from it, any PS1 in the environment dropped; false when unknown */
bool prompt_set_preset(const char *name);
/* The preset in effect, or NULL when PS1 was set some other way */
const char *prompt_current_preset(void);
/* Render a PS1 the way the configured prompt is rendered (escapes, colours, wrapper) */
const char *prompt_render(const char *ps1, int last_exit_code);
/* Print every preset with a rendered sample, for `set prompt=list` */
void prompt_print_presets(FILE *out);

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
