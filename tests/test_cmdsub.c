#include "unity.h"
#include "../src/cmdsub.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test simple command substitution with $(...)
void test_cmdsub_simple_dollar_paren(void) {
    char *result = cmdsub_expand("$(echo hello)");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);

    free(result);
}

// Test command substitution with backticks
void test_cmdsub_backticks(void) {
    char *result = cmdsub_expand("`echo world`");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("world", result);

    free(result);
}

// Test command substitution in middle of string
void test_cmdsub_in_string(void) {
    char *result = cmdsub_expand("Hello $(echo there) friend");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Hello there friend", result);

    free(result);
}

// Test multiple command substitutions
void test_cmdsub_multiple(void) {
    char *result = cmdsub_expand("$(echo a) and $(echo b)");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("a and b", result);

    free(result);
}

// Test nested command substitution
void test_cmdsub_nested(void) {
    char *result = cmdsub_expand("$(echo $(echo nested))");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("nested", result);

    free(result);
}

// Test command substitution with pwd
void test_cmdsub_pwd(void) {
    char *result = cmdsub_expand("$(pwd)");

    TEST_ASSERT_NOT_NULL(result);
    // Should not be empty
    TEST_ASSERT_TRUE(strlen(result) > 0);
    // Should start with / (absolute path)
    TEST_ASSERT_EQUAL_CHAR('/', result[0]);

    free(result);
}

// Test no command substitution returns NULL
void test_cmdsub_no_substitution(void) {
    char *result = cmdsub_expand("plain text");

    TEST_ASSERT_NULL(result);  // No substitution needed
}

// Test escaped dollar sign
void test_cmdsub_escaped_dollar(void) {
    char *result = cmdsub_expand("\\$(echo hello)");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("$(echo hello)", result);

    free(result);
}

// Test escaped backtick
void test_cmdsub_escaped_backtick(void) {
    char *result = cmdsub_expand("\\`echo hello\\`");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("`echo hello`", result);

    free(result);
}

// Test command that outputs multiple lines (newlines stripped)
void test_cmdsub_multiline_output(void) {
    char *result = cmdsub_expand("$(printf 'line1\\nline2\\n')");

    TEST_ASSERT_NOT_NULL(result);
    // Trailing newlines should be stripped, but internal newline preserved
    TEST_ASSERT_EQUAL_STRING("line1\nline2", result);

    free(result);
}

// Test empty command
void test_cmdsub_empty_command(void) {
    char *result = cmdsub_expand("$()");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("", result);

    free(result);
}

// Test command with arguments
void test_cmdsub_with_args(void) {
    // Use printf instead of echo -n for cross-platform compatibility
    // (macOS echo doesn't support -n the same way as GNU echo)
    char *result = cmdsub_expand("$(printf '%s' test)");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("test", result);

    free(result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cmdsub_simple_dollar_paren);
    RUN_TEST(test_cmdsub_backticks);
    RUN_TEST(test_cmdsub_in_string);
    RUN_TEST(test_cmdsub_multiple);
    RUN_TEST(test_cmdsub_nested);
    RUN_TEST(test_cmdsub_pwd);
    RUN_TEST(test_cmdsub_no_substitution);
    RUN_TEST(test_cmdsub_escaped_dollar);
    RUN_TEST(test_cmdsub_escaped_backtick);
    RUN_TEST(test_cmdsub_multiline_output);
    RUN_TEST(test_cmdsub_empty_command);
    RUN_TEST(test_cmdsub_with_args);

    return UNITY_END();
}
