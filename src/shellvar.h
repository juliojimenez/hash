#ifndef SHELLVAR_H
#define SHELLVAR_H

#include <stdbool.h>

// Variable attributes
#define VAR_ATTR_READONLY  0x01
#define VAR_ATTR_EXPORT    0x02

// Initialize shell variable system
void shellvar_init(void);

// Clean up shell variable system
void shellvar_cleanup(void);

// Set a shell variable
// Returns 0 on success, -1 on error (e.g., readonly variable)
int shellvar_set(const char *name, const char *value);

// Get a shell variable value
// Returns NULL if not set
const char *shellvar_get(const char *name);

// Unset a shell variable
// Returns 0 on success, -1 on error (e.g., readonly variable)
int shellvar_unset(const char *name);

// Check if a variable is set
bool shellvar_isset(const char *name);

// Mark variable as readonly
// Returns 0 on success, -1 on error
int shellvar_set_readonly(const char *name);

// Check if variable is readonly
bool shellvar_is_readonly(const char *name);

// Mark variable for export
// Returns 0 on success, -1 on error
int shellvar_set_export(const char *name);

// Check if variable is exported
bool shellvar_is_exported(const char *name);

// List all readonly variables (for `readonly` with no args)
void shellvar_list_readonly(void);

// List all exported variables (for `export` with no args)
void shellvar_list_exported(void);

// Sync a shell variable to environment (for exported vars)
void shellvar_sync_to_env(const char *name);

// Sync environment to shell variables on startup
void shellvar_sync_from_env(void);

// List all shell variables (for `set` with no arguments)
void shellvar_list_all(void);

#endif // SHELLVAR_H
