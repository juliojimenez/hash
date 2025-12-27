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
    char *args[] = {
        strdup("cat"),
        strdup("~/file.txt"),
        strdup("/tmp/other"),
        NULL
    };

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    // First arg unchanged
    TEST_ASSERT_EQUAL_STRING("cat", args[0]);

    // Second arg should be expanded
    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_TRUE(strncmp(args[1], home, strlen(home)) == 0);
    }

    // Third arg unchanged
    TEST_ASSERT_EQUAL_STRING("/tmp/other", args[2]);

    // Clean up
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
}

// Test multiple tildes in args
void test_expand_multiple_tildes(void) {
    char *args[] = {
        strdup("cp"),
        strdup("~/source.txt"),
        strdup("~/dest.txt"),
        NULL
    };

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_TRUE(strncmp(args[1], home, strlen(home)) == 0);
        TEST_ASSERT_TRUE(strncmp(args[2], home, strlen(home)) == 0);
    }

    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
}

// Test empty args
void test_expand_empty_args(void) {
    char *args[] = { NULL };

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test just tilde
void test_expand_just_tilde(void) {
    char *args[] = {
        strdup("cd"),
        strdup("~"),
        NULL
    };

    int result = expand_tilde(args);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *home = getenv("HOME");
    if (home) {
        TEST_ASSERT_EQUAL_STRING(home, args[1]);
    }

    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
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
