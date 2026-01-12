#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shellvar.h"
#include "hash.h"

// Shell variable entry
typedef struct ShellVar {
    char *name;
    char *value;
    int attrs;              // VAR_ATTR_READONLY, VAR_ATTR_EXPORT
    struct ShellVar *next;
} ShellVar;

// Hash table for variables
#define SHELLVAR_HASH_SIZE 256
static ShellVar *var_table[SHELLVAR_HASH_SIZE];

// Simple hash function
static unsigned int hash_name(const char *name) {
    unsigned int hash = 0;
    while (*name) {
        hash = hash * 31 + (unsigned char)*name++;
    }
    return hash % SHELLVAR_HASH_SIZE;
}

// Find a variable entry
static ShellVar *find_var(const char *name) {
    unsigned int h = hash_name(name);
    ShellVar *v = var_table[h];
    while (v) {
        if (strcmp(v->name, name) == 0) {
            return v;
        }
        v = v->next;
    }
    return NULL;
}

void shellvar_init(void) {
    memset(var_table, 0, sizeof(var_table));
}

void shellvar_cleanup(void) {
    for (int i = 0; i < SHELLVAR_HASH_SIZE; i++) {
        ShellVar *v = var_table[i];
        while (v) {
            ShellVar *next = v->next;
            free(v->name);
            free(v->value);
            free(v);
            v = next;
        }
        var_table[i] = NULL;
    }
}

int shellvar_set(const char *name, const char *value) {
    if (!name) return -1;

    ShellVar *v = find_var(name);

    if (v) {
        // Variable exists
        if (v->attrs & VAR_ATTR_READONLY) {
            fprintf(stderr, "%s: %s: readonly variable\n", HASH_NAME, name);
            return -1;
        }

        // Update value
        free(v->value);
        v->value = value ? strdup(value) : NULL;

        // If exported, sync to environment
        if (v->attrs & VAR_ATTR_EXPORT) {
            if (value) {
                setenv(name, value, 1);
            } else {
                unsetenv(name);
            }
        }
    } else {
        // Create new variable
        v = malloc(sizeof(ShellVar));
        if (!v) return -1;

        v->name = strdup(name);
        v->value = value ? strdup(value) : NULL;
        v->attrs = 0;

        // Insert into hash table
        unsigned int h = hash_name(name);
        v->next = var_table[h];
        var_table[h] = v;
    }

    return 0;
}

const char *shellvar_get(const char *name) {
    if (!name) return NULL;

    // First check our internal table
    const ShellVar *v = find_var(name);
    if (v && v->value) {
        return v->value;
    }

    // Fall back to environment
    return getenv(name);
}

int shellvar_unset(const char *name) {
    if (!name) return -1;

    ShellVar *v = find_var(name);

    if (v && (v->attrs & VAR_ATTR_READONLY)) {
        fprintf(stderr, "unset: %s is read-only\n", name);
        return -1;
    }

    // Remove from our table
    unsigned int h = hash_name(name);
    ShellVar *prev = NULL;
    v = var_table[h];
    while (v) {
        if (strcmp(v->name, name) == 0) {
            if (prev) {
                prev->next = v->next;
            } else {
                var_table[h] = v->next;
            }
            free(v->name);
            free(v->value);
            free(v);
            break;
        }
        prev = v;
        v = v->next;
    }

    // Also unset from environment
    unsetenv(name);

    return 0;
}

bool shellvar_isset(const char *name) {
    if (!name) return false;

    const ShellVar *v = find_var(name);
    if (v) return true;

    return getenv(name) != NULL;
}

int shellvar_set_readonly(const char *name) {
    if (!name) return -1;

    ShellVar *v = find_var(name);

    if (!v) {
        // Create entry for readonly even if not set
        v = malloc(sizeof(ShellVar));
        if (!v) return -1;

        v->name = strdup(name);
        v->value = NULL;
        v->attrs = 0;

        // Check environment for initial value
        const char *env_val = getenv(name);
        if (env_val) {
            v->value = strdup(env_val);
        }

        unsigned int h = hash_name(name);
        v->next = var_table[h];
        var_table[h] = v;
    }

    v->attrs |= VAR_ATTR_READONLY;
    return 0;
}

bool shellvar_is_readonly(const char *name) {
    if (!name) return false;

    const ShellVar *v = find_var(name);
    return v && (v->attrs & VAR_ATTR_READONLY);
}

int shellvar_set_export(const char *name) {
    if (!name) return -1;

    ShellVar *v = find_var(name);

    if (!v) {
        // Create entry for export
        v = malloc(sizeof(ShellVar));
        if (!v) return -1;

        v->name = strdup(name);
        v->value = NULL;
        v->attrs = 0;

        unsigned int h = hash_name(name);
        v->next = var_table[h];
        var_table[h] = v;
    }

    v->attrs |= VAR_ATTR_EXPORT;

    // Sync to environment
    if (v->value) {
        setenv(name, v->value, 1);
    }

    return 0;
}

bool shellvar_is_exported(const char *name) {
    if (!name) return false;

    const ShellVar *v = find_var(name);
    return v && (v->attrs & VAR_ATTR_EXPORT);
}

void shellvar_list_readonly(void) {
    for (int i = 0; i < SHELLVAR_HASH_SIZE; i++) {
        ShellVar *v = var_table[i];
        while (v) {
            if (v->attrs & VAR_ATTR_READONLY) {
                if (v->value) {
                    printf("readonly %s='%s'\n", v->name, v->value);
                } else {
                    printf("readonly %s\n", v->name);
                }
            }
            v = v->next;
        }
    }
}

void shellvar_list_exported(void) {
    // First, list variables from our internal table that are marked for export
    // This includes variables without values (export x without assignment)
    for (int i = 0; i < SHELLVAR_HASH_SIZE; i++) {
        ShellVar *v = var_table[i];
        while (v) {
            if (v->attrs & VAR_ATTR_EXPORT) {
                if (v->value) {
                    printf("export %s=\"%s\"\n", v->name, v->value);
                } else {
                    printf("export %s\n", v->name);
                }
            }
            v = v->next;
        }
    }

    // Also list from environment for variables not in our table
    extern char **environ;
    for (char **env = environ; *env; env++) {
        // Parse name from NAME=value
        char *equals = strchr(*env, '=');
        if (equals) {
            size_t name_len = equals - *env;
            char name[256];
            if (name_len < sizeof(name)) {
                memcpy(name, *env, name_len);
                name[name_len] = '\0';
                // Only print if not already in our table
                if (!find_var(name)) {
                    printf("export %s\n", *env);
                }
            }
        }
    }
}

void shellvar_sync_to_env(const char *name) {
    const ShellVar *v = find_var(name);
    if (v && (v->attrs & VAR_ATTR_EXPORT) && v->value) {
        setenv(name, v->value, 1);
    }
}

void shellvar_sync_from_env(void) {
    // Import critical environment variables
    // This is called at shell startup
    extern char **environ;
    for (char **env = environ; *env; env++) {
        const char *eq = strchr(*env, '=');
        if (eq) {
            size_t namelen = eq - *env;
            char *name = malloc(namelen + 1);
            if (name) {
                memcpy(name, *env, namelen);
                name[namelen] = '\0';

                // Create entry marked as exported
                ShellVar *v = malloc(sizeof(ShellVar));
                if (v) {
                    v->name = name;
                    v->value = strdup(eq + 1);
                    v->attrs = VAR_ATTR_EXPORT;

                    unsigned int h = hash_name(name);
                    v->next = var_table[h];
                    var_table[h] = v;
                } else {
                    free(name);
                }
            }
        }
    }
}

// Helper to print a value with proper quoting for shell re-sourcing
static void print_quoted_value(const char *value) {
    if (!value || *value == '\0') {
        printf("''");
        return;
    }

    // Check if value needs quoting (contains spaces, special chars, etc.)
    bool needs_quote = false;
    for (const char *p = value; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\'' ||
            *p == '"' || *p == '\\' || *p == '$' || *p == '`' ||
            *p == '!' || *p == '*' || *p == '?' || *p == '[' ||
            *p == ']' || *p == '(' || *p == ')' || *p == '{' ||
            *p == '}' || *p == '|' || *p == '&' || *p == ';' ||
            *p == '<' || *p == '>' || *p == '#' || *p == '~') {
            needs_quote = true;
            break;
        }
    }

    if (!needs_quote) {
        printf("%s", value);
        return;
    }

    // Use single quotes, escaping embedded single quotes as '\''
    printf("'");
    for (const char *p = value; *p; p++) {
        if (*p == '\'') {
            printf("'\\''");  // End quote, escaped quote, start quote
        } else {
            putchar(*p);
        }
    }
    printf("'");
}

// List all shell variables (for `set` with no arguments)
void shellvar_list_all(void) {
    extern char **environ;

    // First, list all variables from our internal table (includes local variables)
    for (int i = 0; i < SHELLVAR_HASH_SIZE; i++) {
        for (ShellVar *v = var_table[i]; v; v = v->next) {
            if (v->value) {  // Only print if variable has a value
                printf("%s=", v->name);
                print_quoted_value(v->value);
                printf("\n");
            }
        }
    }

    // Then, add any environment variables not in our table
    for (char **env = environ; *env; env++) {
        const char *eq = strchr(*env, '=');
        if (eq) {
            size_t name_len = eq - *env;
            char name[256];
            if (name_len < sizeof(name)) {
                memcpy(name, *env, name_len);
                name[name_len] = '\0';
                // Only print if not already in our table
                if (!find_var(name)) {
                    fwrite(*env, 1, eq - *env, stdout);
                    printf("=");
                    print_quoted_value(eq + 1);
                    printf("\n");
                }
            }
        }
    }
}
