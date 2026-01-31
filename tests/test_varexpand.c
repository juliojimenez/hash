#include "unity.h"
#include "../src/varexpand.h"
#include "../src/script.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Helper to strip \x03 IFS markers from expanded result (in-place)
// These markers are added by varexpand for IFS splitting and get
// processed later in the expansion pipeline
static void strip_ifs_markers(char *s) {
    if (!s) return;
    char *read = s;
    char *write = s;
    while (*read) {
        if (*read != '\x03') {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
}

void setUp(void) {
    script_init();
}

void tearDown(void) {
    script_cleanup();
}

// Test basic variable expansion
void test_expand_simple_var(void) {
    setenv("TEST_VAR", "hello", 1);

    char *result = varexpand_expand("$TEST_VAR", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);

    free(result);
    unsetenv("TEST_VAR");
}

// Test ${VAR} syntax
void test_expand_braced_var(void) {
    setenv("MY_VAR", "world", 1);

    char *result = varexpand_expand("${MY_VAR}", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("world", result);

    free(result);
    unsetenv("MY_VAR");
}

// Test variable in middle of string
void test_expand_var_in_string(void) {
    setenv("USER", "julio", 1);

    char *result = varexpand_expand("Hello $USER!", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("Hello julio!", result);

    free(result);
}

// Test multiple variables
void test_expand_multiple_vars(void) {
    setenv("FIRST", "foo", 1);
    setenv("SECOND", "bar", 1);

    char *result = varexpand_expand("$FIRST and $SECOND", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("foo and bar", result);

    free(result);
    unsetenv("FIRST");
    unsetenv("SECOND");
}

// Test $? (exit code)
void test_expand_exit_code(void) {
    char *result = varexpand_expand("Exit code: $?", 42);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("Exit code: 42", result);

    free(result);
}

// Test $$ (process ID)
void test_expand_pid(void) {
    char *result = varexpand_expand("PID: $$", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);

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
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("Running hash", result);

    free(result);
}

// Test undefined variable
void test_expand_undefined_var(void) {
    unsetenv("UNDEFINED_VAR_12345");

    char *result = varexpand_expand("Value: $UNDEFINED_VAR_12345!", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);  // Empty expansion adds \x03 markers
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
    strip_ifs_markers(result);  // Error case may add markers
    TEST_ASSERT_EQUAL_STRING("", result);

    free(result);
}

// Test concatenation with braces
void test_expand_braced_concat(void) {
    setenv("VAR", "test", 1);

    char *result = varexpand_expand("${VAR}ing", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
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

// ============================================================================
// Positional Parameter Tests
// ============================================================================

// Test $1 positional parameter
void test_expand_positional_1(void) {
    // Set up positional parameters
    script_state.positional_params = malloc(2 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("first_arg");
    script_state.positional_count = 2;

    char *result = varexpand_expand("arg1: $1", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("arg1: first_arg", result);

    free(result);
}

// Test $2 positional parameter
void test_expand_positional_2(void) {
    script_state.positional_params = malloc(3 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("first");
    script_state.positional_params[2] = strdup("second");
    script_state.positional_count = 3;

    char *result = varexpand_expand("$1 and $2", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("first and second", result);

    free(result);
}

// Test ${1} braced positional parameter
void test_expand_positional_braced(void) {
    script_state.positional_params = malloc(2 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("value");
    script_state.positional_count = 2;

    char *result = varexpand_expand("${1}suffix", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("valuesuffix", result);

    free(result);
}

// Test undefined positional parameter (expands to empty)
void test_expand_positional_undefined(void) {
    script_state.positional_params = malloc(1 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_count = 1;

    char *result = varexpand_expand("arg: $1!", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);  // Undefined positional adds markers
    TEST_ASSERT_EQUAL_STRING("arg: !", result);  // $1 is undefined

    free(result);
}

// Test $0 returns script name when set (POSIX behavior)
void test_expand_positional_0_with_params(void) {
    script_state.positional_params = malloc(2 * sizeof(char*));
    script_state.positional_params[0] = strdup("myscript.sh");
    script_state.positional_params[1] = strdup("arg1");
    script_state.positional_count = 2;

    // With positional params, $0 should be script_state.positional_params[0]
    char *result = varexpand_expand("$0", 0);

    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    // POSIX: $0 returns the script name when set
    TEST_ASSERT_EQUAL_STRING("myscript.sh", result);

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

    // Positional parameters
    RUN_TEST(test_expand_positional_1);
    RUN_TEST(test_expand_positional_2);
    RUN_TEST(test_expand_positional_braced);
    RUN_TEST(test_expand_positional_undefined);
    RUN_TEST(test_expand_positional_0_with_params);

    return UNITY_END();
}
