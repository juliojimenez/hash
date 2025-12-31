#include "unity.h"
#include "../src/varexpand.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test basic variable expansion
void test_expand_simple_var(void) {
    setenv("TEST_VAR", "hello", 1);

    char *result = varexpand_expand("$TEST_VAR", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);

    free(result);
    unsetenv("TEST_VAR");
}

// Test ${VAR} syntax
void test_expand_braced_var(void) {
    setenv("MY_VAR", "world", 1);

    char *result = varexpand_expand("${MY_VAR}", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("world", result);

    free(result);
    unsetenv("MY_VAR");
}

// Test variable in middle of string
void test_expand_var_in_string(void) {
    setenv("USER", "julio", 1);

    char *result = varexpand_expand("Hello $USER!", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Hello julio!", result);

    free(result);
}

// Test multiple variables
void test_expand_multiple_vars(void) {
    setenv("FIRST", "foo", 1);
    setenv("SECOND", "bar", 1);

    char *result = varexpand_expand("$FIRST and $SECOND", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("foo and bar", result);

    free(result);
    unsetenv("FIRST");
    unsetenv("SECOND");
}

// Test $? (exit code)
void test_expand_exit_code(void) {
    char *result = varexpand_expand("Exit code: $?", 42);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Exit code: 42", result);

    free(result);
}

// Test $$ (process ID)
void test_expand_pid(void) {
    char *result = varexpand_expand("PID: $$", 0);

    TEST_ASSERT_NOT_NULL(result);

    // Should contain a number
    char expected[64];
    snprintf(expected, sizeof(expected), "PID: %d", getpid());
    TEST_ASSERT_EQUAL_STRING(expected, result);

    free(result);
}

// Test $0 (shell name)
void test_expand_shell_name(void) {
    char *result = varexpand_expand("Running $0", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Running hash", result);

    free(result);
}

// Test undefined variable
void test_expand_undefined_var(void) {
    unsetenv("UNDEFINED_VAR_12345");

    char *result = varexpand_expand("Value: $UNDEFINED_VAR_12345!", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Value: !", result);  // Expands to empty

    free(result);
}

// Test escaped dollar sign
void test_expand_escaped_dollar(void) {
    char *result = varexpand_expand("Price: \\$5", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Price: $5", result);

    free(result);
}

// Test dollar at end of string
void test_expand_dollar_at_end(void) {
    char *result = varexpand_expand("test$", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("test$", result);  // Literal $

    free(result);
}

// Test empty braces
void test_expand_empty_braces(void) {
    char *result = varexpand_expand("${}", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("", result);

    free(result);
}

// Test concatenation with braces
void test_expand_braced_concat(void) {
    setenv("VAR", "test", 1);

    char *result = varexpand_expand("${VAR}ing", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("testing", result);

    free(result);
    unsetenv("VAR");
}

// Test no expansion needed
void test_expand_no_vars(void) {
    char *result = varexpand_expand("plain text", 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("plain text", result);

    free(result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_expand_simple_var);
    RUN_TEST(test_expand_braced_var);
    RUN_TEST(test_expand_var_in_string);
    RUN_TEST(test_expand_multiple_vars);
    RUN_TEST(test_expand_exit_code);
    RUN_TEST(test_expand_pid);
    RUN_TEST(test_expand_shell_name);
    RUN_TEST(test_expand_undefined_var);
    RUN_TEST(test_expand_escaped_dollar);
    RUN_TEST(test_expand_dollar_at_end);
    RUN_TEST(test_expand_empty_braces);
    RUN_TEST(test_expand_braced_concat);
    RUN_TEST(test_expand_no_vars);

    return UNITY_END();
}
