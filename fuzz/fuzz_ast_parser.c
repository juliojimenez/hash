// Fuzz harness for the AST parser: parse_program() on arbitrary input, then
// walk the tree with ast_sexpr(). Nothing is executed.
//
// Built with afl-clang-fast this runs in AFL++ persistent mode (the fuzz job
// in .github/workflows/sanitizers.yml). Built with a plain compiler it parses
// each file named on the command line once, which is how the corpus sweep and
// local sanitizer runs use it:
//
//   clang -std=gnu99 -fsanitize=address,undefined -Isrc fuzz/fuzz_ast_parser.c \
//       $(ls build/*.o | grep -v main.o) -o fuzz/fuzz_ast_parser
//   ./fuzz/fuzz_ast_parser fuzz/seeds/* examples/*.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/arena.h"
#include "../src/ast.h"
#include "../src/ast_parser.h"

#define MAX_INPUT 65536

static void parse_once(const unsigned char *buf, size_t len) {
    char *input = malloc(len + 1);
    if (!input) {
        return;
    }
    memcpy(input, buf, len);
    input[len] = '\0';

    Arena arena;
    arena_init(&arena, 0);
    AstParseResult parsed = parse_program(input, &arena);
    if (parsed.status == PARSE_OK && parsed.root) {
        char *text = ast_sexpr(parsed.root, &arena);
        (void)text;
    }
    arena_free(&arena);
    free(input);
}

#ifdef __AFL_FUZZ_TESTCASE_LEN

__AFL_FUZZ_INIT();

int main(void) {
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        if (len < 0 || len > MAX_INPUT) {
            continue;
        }
        parse_once(buf, (size_t)len);
    }
    return 0;
}

#else

static int parse_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 1;
    }
    unsigned char *buf = malloc(MAX_INPUT);
    if (!buf) {
        fclose(f);
        return 1;
    }
    size_t len = fread(buf, 1, MAX_INPUT, f);
    fclose(f);
    parse_once(buf, len);
    free(buf);
    return 0;
}

int main(int argc, char **argv) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        rc |= parse_file(argv[i]);
    }
    return rc;
}

#endif
