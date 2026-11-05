// Fuzz harness for the tokenizer: tokenize arbitrary input to the end. Nothing
// is parsed or executed. Dual build like fuzz_ast_parser.c: AFL++ persistent
// mode under afl-clang-fast, one pass over each named file otherwise.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/tokenizer.h"

#define MAX_INPUT 65536

static void tokenize_once(const unsigned char *buf, size_t len) {
    char *input = malloc(len + 1);
    if (!input) {
        return;
    }
    memcpy(input, buf, len);
    input[len] = '\0';

    Tokenizer tk;
    tokenizer_init(&tk, input);
    // Every token but EOF consumes at least one byte, so more tokens than
    // bytes means the tokenizer stopped advancing: make that a crash
    size_t budget = len + 2;
    for (;;) {
        Token t = tokenizer_next(&tk);
        if (t.type == TOKEN_EOF || t.type == TOKEN_ERROR) {
            break;
        }
        if (budget-- == 0) {
            abort();
        }
    }
    tokenizer_destroy(&tk);
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
        tokenize_once(buf, (size_t)len);
    }
    return 0;
}

#else

static int tokenize_file(const char *path) {
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
    tokenize_once(buf, len);
    free(buf);
    return 0;
}

int main(int argc, char **argv) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        rc |= tokenize_file(argv[i]);
    }
    return rc;
}

#endif
