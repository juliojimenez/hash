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

// ============== Glob expansion tests ==============

// Test has_glob_chars with asterisk
void test_has_glob_chars_asterisk(void) {
    TEST_ASSERT_TRUE(has_glob_chars("*.txt"));
    TEST_ASSERT_TRUE(has_glob_chars("file*"));
    TEST_ASSERT_TRUE(has_glob_chars("*"));
    TEST_ASSERT_TRUE(has_glob_chars("src/*.c"));
}

// Test has_glob_chars with question mark
void test_has_glob_chars_question(void) {
    TEST_ASSERT_TRUE(has_glob_chars("file?.txt"));
    TEST_ASSERT_TRUE(has_glob_chars("?"));
    TEST_ASSERT_TRUE(has_glob_chars("test?"));
}

// Test has_glob_chars with brackets
void test_has_glob_chars_brackets(void) {
    TEST_ASSERT_TRUE(has_glob_chars("file[abc].txt"));
    TEST_ASSERT_TRUE(has_glob_chars("file[0-9]"));
    TEST_ASSERT_TRUE(has_glob_chars("[abc]"));
}

// Test has_glob_chars returns false for non-glob strings
void test_has_glob_chars_no_glob(void) {
    TEST_ASSERT_FALSE(has_glob_chars("hello"));
    TEST_ASSERT_FALSE(has_glob_chars("/path/to/file.txt"));
    TEST_ASSERT_FALSE(has_glob_chars(""));
    TEST_ASSERT_FALSE(has_glob_chars(NULL));
}

// Test has_glob_chars with escaped characters
void test_has_glob_chars_escaped(void) {
    TEST_ASSERT_FALSE(has_glob_chars("\\*"));
    TEST_ASSERT_FALSE(has_glob_chars("\\?"));
    TEST_ASSERT_FALSE(has_glob_chars("file\\*.txt"));
}

// Test has_glob_chars with incomplete brackets
void test_has_glob_chars_incomplete_bracket(void) {
    TEST_ASSERT_FALSE(has_glob_chars("file[abc"));  // No closing bracket
    TEST_ASSERT_FALSE(has_glob_chars("["));
}

// Test expand_glob with no glob characters (should not modify)
void test_expand_glob_no_glob(void) {
    char *arg0 = strdup("echo");
    char *arg1 = strdup("hello");
    char **args = malloc(3 * sizeof(char *));
    args[0] = arg0;
    args[1] = arg1;
    args[2] = NULL;
    int arg_count = 2;

    int result = expand_glob(&args, &arg_count);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(2, arg_count);

    free(arg0);
    free(arg1);
    free(args);
}

// Test expand_glob with NULL args
void test_expand_glob_null(void) {
    int arg_count = 0;
    TEST_ASSERT_EQUAL_INT(-1, expand_glob(NULL, &arg_count));

    char **args = NULL;
    TEST_ASSERT_EQUAL_INT(-1, expand_glob(&args, &arg_count));
}

int main(void) {
    UNITY_BEGIN();

    // Tilde expansion tests
    RUN_TEST(test_expand_tilde_home);
    RUN_TEST(test_expand_tilde_with_path);
    RUN_TEST(test_expand_non_tilde);
    RUN_TEST(test_expand_tilde_args);
    RUN_TEST(test_expand_multiple_tildes);
    RUN_TEST(test_expand_empty_args);
    RUN_TEST(test_expand_just_tilde);

    // Glob expansion tests
    RUN_TEST(test_has_glob_chars_asterisk);
    RUN_TEST(test_has_glob_chars_question);
    RUN_TEST(test_has_glob_chars_brackets);
    RUN_TEST(test_has_glob_chars_no_glob);
    RUN_TEST(test_has_glob_chars_escaped);
    RUN_TEST(test_has_glob_chars_incomplete_bracket);
    RUN_TEST(test_expand_glob_no_glob);
    RUN_TEST(test_expand_glob_null);

    return UNITY_END();
}
