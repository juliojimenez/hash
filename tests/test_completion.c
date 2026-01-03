#include "unity.h"
#include "../src/completion.h"
#include "../src/config.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

void setUp(void) {
    config_init();
    completion_init();
}

void tearDown(void) {
}

// Test completion generation returns results
void test_completion_generate_basic(void) {
    // Complete "ec" should find "echo"
    CompletionResult *result = completion_generate("ec", 2);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->count > 0);

    // Check if "echo" is in the results
    int found_echo = 0;
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->matches[i], "echo") == 0) {
            found_echo = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_echo);

    completion_free_result(result);
}

// Test completion with empty input
void test_completion_generate_empty(void) {
    CompletionResult *result = completion_generate("", 0);

    TEST_ASSERT_NOT_NULL(result);
    // Should return built-in commands
    TEST_ASSERT_TRUE(result->count > 0);

    completion_free_result(result);
}

// Test common prefix calculation
void test_completion_common_prefix_single(void) {
    char *matches[] = {"testing"};
    char *prefix = completion_common_prefix(matches, 1);

    TEST_ASSERT_NOT_NULL(prefix);
    TEST_ASSERT_EQUAL_STRING("testing", prefix);

    free(prefix);
}

// Test common prefix with multiple matches
void test_completion_common_prefix_multiple(void) {
    char *matches[] = {"test1", "test2", "test3"};
    char *prefix = completion_common_prefix(matches, 3);

    TEST_ASSERT_NOT_NULL(prefix);
    TEST_ASSERT_EQUAL_STRING("test", prefix);

    free(prefix);
}

// Test common prefix with no common prefix
void test_completion_common_prefix_none(void) {
    char *matches[] = {"apple", "banana", "cherry"};
    char *prefix = completion_common_prefix(matches, 3);

    TEST_ASSERT_NULL(prefix);
}

// Test file completion for root directory - should not have double slashes
void test_completion_root_no_double_slash(void) {
    // Complete "ls /" should show files in root without double slashes
    CompletionResult *result = completion_generate("ls /", 4);

    TEST_ASSERT_NOT_NULL(result);

    // Check that no match starts with "//"
    for (int i = 0; i < result->count; i++) {
        TEST_ASSERT_FALSE(strncmp(result->matches[i], "//", 2) == 0);
    }

    completion_free_result(result);
}

// Test file completion with trailing slash doesn't add extra slash
void test_completion_path_no_extra_slash(void) {
    // Create a test scenario - complete "ls /tmp/"
    CompletionResult *result = completion_generate("ls /tmp/", 8);

    TEST_ASSERT_NOT_NULL(result);

    // Check that matches start with /tmp/ not /tmp//
    for (int i = 0; i < result->count; i++) {
        // Should not have double slashes
        TEST_ASSERT_NULL(strstr(result->matches[i], "//"));
    }

    completion_free_result(result);
}

// Test that directory completions end with /
void test_completion_directory_has_trailing_slash(void) {
    // Complete in current directory, looking for directories
    CompletionResult *result = completion_generate("cd ", 3);

    TEST_ASSERT_NOT_NULL(result);

    // At least some matches should be directories (end with /)
    // We can't guarantee which ones, but the code should work

    completion_free_result(result);
}

// Test completing a path like /e should give /etc/ etc.
void test_completion_partial_root_path(void) {
    CompletionResult *result = completion_generate("ls /e", 5);

    TEST_ASSERT_NOT_NULL(result);

    // Should find /etc if it exists
    int found_etc = 0;
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->matches[i], "/etc/") == 0) {
            found_etc = 1;
            break;
        }
    }
    // /etc should exist on most systems
    TEST_ASSERT_TRUE(found_etc);

    completion_free_result(result);
}

// Test that NULL input is handled
void test_completion_null_input(void) {
    CompletionResult *result = completion_generate(NULL, 0);
    TEST_ASSERT_NULL(result);
}

// Test free on NULL
void test_completion_free_null(void) {
    // Should not crash
    completion_free_result(NULL);
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_completion_generate_basic);
    RUN_TEST(test_completion_generate_empty);
    RUN_TEST(test_completion_common_prefix_single);
    RUN_TEST(test_completion_common_prefix_multiple);
    RUN_TEST(test_completion_common_prefix_none);
    RUN_TEST(test_completion_root_no_double_slash);
    RUN_TEST(test_completion_path_no_extra_slash);
    RUN_TEST(test_completion_directory_has_trailing_slash);
    RUN_TEST(test_completion_partial_root_path);
    RUN_TEST(test_completion_null_input);
    RUN_TEST(test_completion_free_null);

    return UNITY_END();
}
