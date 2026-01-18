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
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with multiple arguments
void test_parse_line_multiple_args(void) {
    char line[] = "ls -la /tmp";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("ls", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("-la", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("/tmp", parsed.tokens[2]);
    TEST_ASSERT_NULL(parsed.tokens[3]);

    parse_result_free(&parsed);
}

// Test parse_line with extra whitespace
void test_parse_line_extra_whitespace(void) {
    char line[] = "  echo   hello   world  ";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("world", parsed.tokens[2]);
    TEST_ASSERT_NULL(parsed.tokens[3]);

    parse_result_free(&parsed);
}

// Test parse_line with empty string
void test_parse_line_empty_string(void) {
    char line[] = "";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_NULL(parsed.tokens[0]);

    parse_result_free(&parsed);
}

// Test parse_line with only whitespace
void test_parse_line_whitespace_only(void) {
    char line[] = "   \t  \n  ";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_NULL(parsed.tokens[0]);

    parse_result_free(&parsed);
}

// Test parse_line with tabs
void test_parse_line_with_tabs(void) {
    char line[] = "echo\thello\tworld";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("world", parsed.tokens[2]);
    TEST_ASSERT_NULL(parsed.tokens[3]);

    parse_result_free(&parsed);
}

// Test parse_line with newline
void test_parse_line_with_newline(void) {
    char line[] = "echo hello\n";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with double quotes
void test_parse_line_double_quotes(void) {
    char line[] = "echo \"hello world\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with single quotes
void test_parse_line_single_quotes(void) {
    char line[] = "echo 'hello world'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with mixed quotes
void test_parse_line_mixed_quotes(void) {
    char line[] = "echo \"double\" 'single' unquoted";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("double", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("single", parsed.tokens[2]);
    TEST_ASSERT_EQUAL_STRING("unquoted", parsed.tokens[3]);
    TEST_ASSERT_NULL(parsed.tokens[4]);

    parse_result_free(&parsed);
}

// Test parse_line with escaped double quote
void test_parse_line_escaped_double_quote(void) {
    char line[] = "echo \"He said \\\"hello\\\"\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("He said \"hello\"", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with escaped single quote
void test_parse_line_escaped_single_quote(void) {
    char line[] = "echo 'can'\\''t'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("can't", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with backslash escape
void test_parse_line_escaped_backslash(void) {
    char line[] = "echo \"path\\\\to\\\\file\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("path\\to\\file", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with backslash-n in double quotes (POSIX: stays literal)
void test_parse_line_escaped_newline(void) {
    char line[] = "echo \"line1\\nline2\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    // POSIX: \n in double quotes is NOT interpreted - stays as backslash-n
    TEST_ASSERT_EQUAL_STRING("line1\\nline2", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with backslash-t in double quotes (POSIX: stays literal)
void test_parse_line_escaped_tab(void) {
    char line[] = "echo \"col1\\tcol2\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    // POSIX: \t in double quotes is NOT interpreted - stays as backslash-t
    TEST_ASSERT_EQUAL_STRING("col1\\tcol2", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with literal backslash in single quotes
void test_parse_line_single_quote_literal_backslash(void) {
    char line[] = "echo 'literal\\n'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("literal\\n", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with empty quotes
// POSIX: Empty quoted strings should be preserved as valid (empty) arguments
void test_parse_line_empty_quotes(void) {
    char line[] = "echo \"\" ''";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("", parsed.tokens[1]);  // Empty double-quoted string
    TEST_ASSERT_EQUAL_STRING("", parsed.tokens[2]);  // Empty single-quoted string
    TEST_ASSERT_NULL(parsed.tokens[3]);

    parse_result_free(&parsed);
}

// ============================================================================
// Arithmetic Expression Tokenization Tests
// ============================================================================

// Test $(()) kept as single token with spaces
void test_parse_arith_with_spaces(void) {
    char line[] = "echo $(( 5 * 2 ))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$(( 5 * 2 ))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test $(()) without spaces
void test_parse_arith_no_spaces(void) {
    char line[] = "echo $((5*2))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$((5*2))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test nested $(()) expressions
void test_parse_arith_nested(void) {
    char line[] = "echo $(( 1 + $((2 + 3)) ))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$(( 1 + $((2 + 3)) ))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// ============================================================================
// Command Substitution Tokenization Tests
// ============================================================================

// Test $() kept as single token with spaces
void test_parse_cmdsub_with_spaces(void) {
    char line[] = "echo $(echo hello world)";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo hello world)", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test nested $() expressions
void test_parse_cmdsub_nested(void) {
    char line[] = "echo $(echo $(echo nested))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo $(echo nested))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test $() with $(()) inside (command sub containing arithmetic)
void test_parse_cmdsub_with_arith(void) {
    char line[] = "echo $(echo $((5 + 3)))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$(echo $((5 + 3)))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test $(()) with $() inside (arithmetic containing command sub)
void test_parse_arith_with_cmdsub(void) {
    char line[] = "echo $((5 * $(echo 2)))";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("$((5 * $(echo 2)))", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
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
