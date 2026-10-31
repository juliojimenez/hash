#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>

/**
 * Selects which command engine runs the shell's input while the
 * line-oriented engine in script.c is replaced by the parser and tree
 * executor (tokenizer.c, ast_parser.c, exec.c).
 *
 * The AST engine is the default. HASH_ENGINE=ast|legacy in the environment
 * selects one; the --engine=ast|legacy command-line option overrides it.
 * The legacy engine stays selectable until it is deleted.
 */

typedef enum {
    ENGINE_LEGACY = 0,  /* script.c line processor */
    ENGINE_AST = 1      /* ast_parser.c + exec.c */
} ShellEngine;

/**
 * Pick the engine from HASH_ENGINE, or the built-in default.
 */
void engine_init(void);

/**
 * Select by name ("ast" or "legacy"). Returns false for anything else.
 */
bool engine_set_by_name(const char *name);

ShellEngine engine_current(void);
const char *engine_name(void);
bool engine_is_ast(void);

#endif
