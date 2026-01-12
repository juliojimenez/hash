#include "unity.h"
#include "../src/parser.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

// Test parse_line with simple command
void test_parse_line_simple_command(void) {
    char line[] = "echo hello";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with multiple arguments
void test_parse_line_multiple_args(void) {
    char line[] = "ls -la /tmp";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("ls", args[0]);
    TEST_ASSERT_EQUAL_STRING("-la", args[1]);
    TEST_ASSERT_EQUAL_STRING("/tmp", args[2]);
    TEST_ASSERT_NULL(args[3]);

    free(args);
}

// Test parse_line with extra whitespace
void test_parse_line_extra_whitespace(void) {
    char line[] = "  echo   hello   world  ";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", args[1]);
    TEST_ASSERT_EQUAL_STRING("world", args[2]);
    TEST_ASSERT_NULL(args[3]);

    free(args);
}

// Test parse_line with empty string
void test_parse_line_empty_string(void) {
    char line[] = "";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_NULL(args[0]);

    free(args);
}

// Test parse_line with only whitespace
void test_parse_line_whitespace_only(void) {
    char line[] = "   \t  \n  ";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_NULL(args[0]);

    free(args);
}

// Test parse_line with tabs
void test_parse_line_with_tabs(void) {
    char line[] = "echo\thello\tworld";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", args[1]);
    TEST_ASSERT_EQUAL_STRING("world", args[2]);
    TEST_ASSERT_NULL(args[3]);

    free(args);
}

// Test parse_line with newline
void test_parse_line_with_newline(void) {
    char line[] = "echo hello\n";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with double quotes
void test_parse_line_double_quotes(void) {
    char line[] = "echo \"hello world\"";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with single quotes
void test_parse_line_single_quotes(void) {
    char line[] = "echo 'hello world'";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with mixed quotes
void test_parse_line_mixed_quotes(void) {
    char line[] = "echo \"double\" 'single' unquoted";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("double", args[1]);
    TEST_ASSERT_EQUAL_STRING("single", args[2]);
    TEST_ASSERT_EQUAL_STRING("unquoted", args[3]);
    TEST_ASSERT_NULL(args[4]);

    free(args);
}

// Test parse_line with escaped double quote
void test_parse_line_escaped_double_quote(void) {
    char line[] = "echo \"He said \\\"hello\\\"\"";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("He said \"hello\"", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with escaped single quote
void test_parse_line_escaped_single_quote(void) {
    char line[] = "echo 'can'\\''t'";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("can't", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with backslash escape
void test_parse_line_escaped_backslash(void) {
    char line[] = "echo \"path\\\\to\\\\file\"";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("path\\to\\file", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with backslash-n in double quotes (POSIX: stays literal)
void test_parse_line_escaped_newline(void) {
    char line[] = "echo \"line1\\nline2\"";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    // POSIX: \n in double quotes is NOT interpreted - stays as backslash-n
    TEST_ASSERT_EQUAL_STRING("line1\\nline2", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with backslash-t in double quotes (POSIX: stays literal)
void test_parse_line_escaped_tab(void) {
    char line[] = "echo \"col1\\tcol2\"";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    // POSIX: \t in double quotes is NOT interpreted - stays as backslash-t
    TEST_ASSERT_EQUAL_STRING("col1\\tcol2", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with literal backslash in single quotes
void test_parse_line_single_quote_literal_backslash(void) {
    char line[] = "echo 'literal\\n'";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("literal\\n", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test parse_line with empty quotes
// POSIX: Empty quoted strings should be preserved as valid (empty) arguments
void test_parse_line_empty_quotes(void) {
    char line[] = "echo \"\" ''";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("", args[1]);  // Empty double-quoted string
    TEST_ASSERT_EQUAL_STRING("", args[2]);  // Empty single-quoted string
    TEST_ASSERT_NULL(args[3]);

    free(args);
}

// ============================================================================
// Arithmetic Expression Tokenization Tests
// ============================================================================

// Test $(()) kept as single token with spaces
void test_parse_arith_with_spaces(void) {
    char line[] = "echo $(( 5 * 2 ))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$(( 5 * 2 ))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test $(()) without spaces
void test_parse_arith_no_spaces(void) {
    char line[] = "echo $((5*2))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$((5*2))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test nested $(()) expressions
void test_parse_arith_nested(void) {
    char line[] = "echo $(( 1 + $((2 + 3)) ))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$(( 1 + $((2 + 3)) ))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// ============================================================================
// Command Substitution Tokenization Tests
// ============================================================================

// Test $() kept as single token with spaces
void test_parse_cmdsub_with_spaces(void) {
    char line[] = "echo $(echo hello world)";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo hello world)", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test nested $() expressions
void test_parse_cmdsub_nested(void) {
    char line[] = "echo $(echo $(echo nested))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo $(echo nested))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test $() with $(()) inside (command sub containing arithmetic)
void test_parse_cmdsub_with_arith(void) {
    char line[] = "echo $(echo $((5 + 3)))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo $((5 + 3)))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

// Test $(()) with $() inside (arithmetic containing command sub)
void test_parse_arith_with_cmdsub(void) {
    char line[] = "echo $((5 * $(echo 2)))";
    char **args = parse_line(line);

    TEST_ASSERT_NOT_NULL(args);
    TEST_ASSERT_EQUAL_STRING("echo", args[0]);
    TEST_ASSERT_EQUAL_STRING("$((5 * $(echo 2)))", args[1]);
    TEST_ASSERT_NULL(args[2]);

    free(args);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_line_simple_command);
    RUN_TEST(test_parse_line_multiple_args);
    RUN_TEST(test_parse_line_extra_whitespace);
    RUN_TEST(test_parse_line_empty_string);
    RUN_TEST(test_parse_line_whitespace_only);
    RUN_TEST(test_parse_line_with_tabs);
    RUN_TEST(test_parse_line_with_newline);
    RUN_TEST(test_parse_line_double_quotes);
    RUN_TEST(test_parse_line_single_quotes);
    RUN_TEST(test_parse_line_mixed_quotes);
    RUN_TEST(test_parse_line_escaped_double_quote);
    RUN_TEST(test_parse_line_escaped_single_quote);
    RUN_TEST(test_parse_line_escaped_backslash);
    RUN_TEST(test_parse_line_escaped_newline);
    RUN_TEST(test_parse_line_escaped_tab);
    RUN_TEST(test_parse_line_single_quote_literal_backslash);
    RUN_TEST(test_parse_line_empty_quotes);

    // Arithmetic tokenization
    RUN_TEST(test_parse_arith_with_spaces);
    RUN_TEST(test_parse_arith_no_spaces);
    RUN_TEST(test_parse_arith_nested);

    // Command substitution tokenization
    RUN_TEST(test_parse_cmdsub_with_spaces);
    RUN_TEST(test_parse_cmdsub_nested);
    RUN_TEST(test_parse_cmdsub_with_arith);
    RUN_TEST(test_parse_arith_with_cmdsub);

    return UNITY_END();
}
