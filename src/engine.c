#include <stdlib.h>
#include <string.h>
#include "engine.h"

#define ENGINE_DEFAULT ENGINE_LEGACY

static ShellEngine current_engine = ENGINE_DEFAULT;

void engine_init(void) {
    current_engine = ENGINE_DEFAULT;
    const char *from_env = getenv("HASH_ENGINE");
    if (from_env && *from_env) {
        engine_set_by_name(from_env);
    }
}

bool engine_set_by_name(const char *name) {
    if (!name) {
        return false;
    }
    if (strcmp(name, "ast") == 0) {
        current_engine = ENGINE_AST;
        return true;
    }
    if (strcmp(name, "legacy") == 0) {
        current_engine = ENGINE_LEGACY;
        return true;
    }
    return false;
}

ShellEngine engine_current(void) {
    return current_engine;
}

const char *engine_name(void) {
    return current_engine == ENGINE_AST ? "ast" : "legacy";
}

bool engine_is_ast(void) {
    return current_engine == ENGINE_AST;
}
