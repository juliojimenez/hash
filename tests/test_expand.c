#include "unity.h"
#include "../src/expand.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test expanding ~ to home directory
void test_expand_tilde_home(void) {
    char *expanded = expand_tilde_path("~");

    TEST_ASSERT_NOT_NULL(expanded);

    // Should expand to home directory
    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_EQUAL_STRING(home, expanded);
    }

    free(expanded);
}

// Test expanding ~/path
void test_expand_tilde_with_path(void) {
    char *expanded = expand_tilde_path("~/Documents");

    TEST_ASSERT_NOT_NULL(expanded);

    // Should start with home directory
    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_TRUE(strncmp(expanded, home, strlen(home)) == 0);
        TEST_ASSERT_TRUE(strstr(expanded, "Documents") != NULL);
    }

    free(expanded);
}

// Test non-tilde path returns NULL
void test_expand_non_tilde(void) {
    char *expanded = expand_tilde_path("/tmp/test");
    TEST_ASSERT_NULL(expanded);
}

// Test expand_tilde on args array
void test_expand_tilde_args(void) {
    // Create line buffer that args will point into
    char line_buf[256] = "cat ~/file.txt /tmp/other";

    char *args[4];
    args[0] = line_buf;              // "cat"
    args[1] = line_buf + 4;          // "~/file.txt"
    args[2] = line_buf + 15;         // "/tmp/other"
    args[3] = NULL;

    // Manually null-terminate (simulating parse_line behavior)
    line_buf[3] = '\0';
    line_buf[14] = '\0';

    // Save original pointer to expanded arg
    char *original_arg1 = args[1];

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    // First arg unchanged
    TEST_ASSERT_EQUAL_STRING("cat", args[0]);

    // Second arg should be expanded (and is now a different pointer)
    TEST_ASSERT_NOT_EQUAL(original_arg1, args[1]);
    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_TRUE(strncmp(args[1], home, strlen(home)) == 0);
    }

    // Third arg unchanged
    TEST_ASSERT_EQUAL_STRING("/tmp/other", args[2]);

    // Free only the expanded arg
    if (args[1] != original_arg1) {
        free(args[1]);
    }
}

// Test multiple tildes in args
void test_expand_multiple_tildes(void) {
    char line_buf[256] = "cp ~/source.txt ~/dest.txt";

    char *args[4];
    args[0] = line_buf;        // "cp"
    args[1] = line_buf + 3;    // "~/source.txt"
    args[2] = line_buf + 16;   // "~/dest.txt"
    args[3] = NULL;

    line_buf[2] = '\0';
    line_buf[15] = '\0';

    char *orig_arg1 = args[1];
    char *orig_arg2 = args[2];

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_TRUE(strncmp(args[1], home, strlen(home)) == 0);
        TEST_ASSERT_TRUE(strncmp(args[2], home, strlen(home)) == 0);
    }

    // Free expanded args
    if (args[1] != orig_arg1) free(args[1]);
    if (args[2] != orig_arg2) free(args[2]);
}

// Test empty args
void test_expand_empty_args(void) {
    char *args[] = { NULL };

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test just tilde
void test_expand_just_tilde(void) {
    char line_buf[64] = "cd ~";

    char *args[3];
    args[0] = line_buf;      // "cd"
    args[1] = line_buf + 3;  // "~"
    args[2] = NULL;

    line_buf[2] = '\0';

    char *orig_arg1 = args[1];

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_EQUAL_STRING(home, args[1]);
    }

    if (args[1] != orig_arg1) free(args[1]);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_expand_tilde_home);
    RUN_TEST(test_expand_tilde_with_path);
    RUN_TEST(test_expand_non_tilde);
    RUN_TEST(test_expand_tilde_args);
    RUN_TEST(test_expand_multiple_tildes);
    RUN_TEST(test_expand_empty_args);
    RUN_TEST(test_expand_just_tilde);

    return UNITY_END();
}
