#include "unity.h"
#include "../src/varexpand.h"
#include "../src/script.h"
#include "../src/shellvar.h"
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

// "$$" inside double quotes (the parser marks each $ with \x02) is the pid too (#349)
void test_expand_pid_quoted(void) {
    char *result = varexpand_expand("\x02$\x02$", 0);
    TEST_ASSERT_NOT_NULL(result);
    char expected[64];
    snprintf(expected, sizeof(expected), "%d", (int)shell_pid());
    TEST_ASSERT_EQUAL_STRING(expected, result);
    free(result);
}

// ${$} ${?} ${!} ${-} ${#} in braces, unquoted and quoted (#349)
void test_expand_special_braced(void) {
    char expected[64];
    char *result = varexpand_expand("${$}", 0);
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    snprintf(expected, sizeof(expected), "%d", (int)shell_pid());
    TEST_ASSERT_EQUAL_STRING(expected, result);
    free(result);

    result = varexpand_expand("rc=${?}", 3);
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("rc=3", result);
    free(result);

    result = varexpand_expand("\x02${?}", 7);   // "${?}"
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("7", result);
    free(result);

    result = varexpand_expand("[${!}]", 0);     // no background job yet: empty
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("[]", result);
    free(result);

    result = varexpand_expand("${#}", 0);       // no positional parameters
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("0", result);
    free(result);

    TEST_ASSERT_FALSE(varexpand_had_error());
}

// Modifiers and length apply to special parameters: ${?:-x} ${?:+set} ${#?}
void test_expand_special_braced_modifiers(void) {
    char *result = varexpand_expand("${?:-x} ${?:+set} ${#?}", 42);
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("42 set 2", result);
    free(result);

    // Assigning a special parameter is an error, not an expansion
    varexpand_clear_error();
    result = varexpand_expand("${?=5}", 1);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(varexpand_had_error());
    varexpand_clear_error();
    free(result);
}

// ${@} and ${*} in braces behave like $@ and $*
void test_expand_special_braced_positional(void) {
    script_state.positional_params = malloc(3 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("one");
    script_state.positional_params[2] = strdup("two");
    script_state.positional_count = 3;

    char *result = varexpand_expand("${#}", 0);
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("2", result);
    free(result);

    result = varexpand_expand("${@}", 0);       // fields separated by \x04
    TEST_ASSERT_NOT_NULL(result);
    strip_ifs_markers(result);
    TEST_ASSERT_EQUAL_STRING("one\x04two", result);
    free(result);

    result = varexpand_expand("\x02${*}", 0);   // "${*}": one field joined with IFS[0]
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("one two", result);
    free(result);
}

// A value holding a real 0x01..0x04 byte comes out as \x01 + byte, so the
// byte is not read as a marker later; "$@" keeps its \x04 separators raw (#354)
void test_expand_marker_bytes_in_values(void) {
    setenv("HASH_CC", "p\x01q\x03r", 1);
    char *result = varexpand_expand("$HASH_CC", 0);          // unquoted: inside \x03 markers
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03p\x01\x01q\x01\x03r\x03", result);
    free(result);
    result = varexpand_expand("\x02$HASH_CC", 0);            // "$HASH_CC"
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("p\x01\x01q\x01\x03r", result);
    free(result);
    unsetenv("HASH_CC");

    script_state.positional_params = malloc(3 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("a\x04" "b");
    script_state.positional_params[2] = strdup("c");
    script_state.positional_count = 3;
    result = varexpand_expand("\x02$@", 0);                  // "$@"
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("a\x01\x04" "b\x04" "c", result);
    free(result);
}

// The word of ${var:-word} keeps its own quotes (the preprocessor passes them
// through since #375): a quoted segment is one protected field, an unquoted
// one splits; ${var:=word} stores the word clean (#375)
void test_expand_default_word_quotes(void) {
    unsetenv("HASH_DW");
    char *result = varexpand_expand("${HASH_DW:-\"a b\"}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03\x01" "a\x01 \x01" "b\x03", result);
    free(result);
    result = varexpand_expand("${HASH_DW:-a b}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03" "a b\x03", result);
    free(result);
    result = varexpand_expand("${HASH_DW:-'q r'}x", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03\x01" "q\x01 \x01" "r\x03" "x", result);
    free(result);
    // a $(...) already substituted into the word (its \x03 region) splits unquoted,
    // and is one field when the whole ${...} is quoted
    result = varexpand_expand("${HASH_DW:-\x03" "a b\x03}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03" "a b\x03", result);
    free(result);
    result = varexpand_expand("\x02${HASH_DW:-\x03" "a b\x03}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x01" "a\x01 \x01" "b", result);
    free(result);
    // := stores the value without markers
    result = varexpand_expand("${HASH_DW:=\x03" "hi there\x03}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hi there", shellvar_get("HASH_DW"));
    free(result);
    shellvar_unset("HASH_DW");
}

// ${1+"$@"}: each parameter stays one field, the \x04 separators stay raw
void test_expand_default_word_quoted_at(void) {
    script_state.positional_params = malloc(3 * sizeof(char*));
    script_state.positional_params[0] = strdup("script.sh");
    script_state.positional_params[1] = strdup("a b");
    script_state.positional_params[2] = strdup("c");
    script_state.positional_count = 3;
    char *result = varexpand_expand("${1+\"$@\"}", 0);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("\x03\x01" "a\x01 \x01" "b\x04\x01" "c\x03", result);
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
    RUN_TEST(test_expand_default_word_quotes);
    RUN_TEST(test_expand_default_word_quoted_at);
    RUN_TEST(test_expand_marker_bytes_in_values);
    RUN_TEST(test_expand_pid_quoted);
    RUN_TEST(test_expand_special_braced);
    RUN_TEST(test_expand_special_braced_modifiers);
    RUN_TEST(test_expand_special_braced_positional);
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
