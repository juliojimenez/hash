#include "unity.h"
#include "../src/test_builtin.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

void setUp(void) {
}

void tearDown(void) {
}

// ============================================================================
// File Tests
// ============================================================================

void test_file_exists(void) {
    char *args[] = { "test", "-e", "/etc/passwd", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - file exists
}

void test_file_not_exists(void) {
    char *args[] = { "test", "-e", "/nonexistent/file/12345", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - file doesn't exist
}

void test_file_is_regular(void) {
    char *args[] = { "test", "-f", "/etc/passwd", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_file_is_directory(void) {
    char *args[] = { "test", "-d", "/tmp", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_file_is_not_directory(void) {
    char *args[] = { "test", "-d", "/etc/passwd", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - passwd is a file, not dir
}

void test_file_is_readable(void) {
    char *args[] = { "test", "-r", "/etc/passwd", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

// ============================================================================
// String Tests
// ============================================================================

void test_string_empty(void) {
    char *args[] = { "test", "-z", "", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - empty string
}

void test_string_not_empty(void) {
    char *args[] = { "test", "-z", "hello", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - not empty
}

void test_string_nonempty_n(void) {
    char *args[] = { "test", "-n", "hello", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - not empty
}

void test_string_equal(void) {
    char *args[] = { "test", "hello", "=", "hello", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_string_not_equal(void) {
    char *args[] = { "test", "hello", "!=", "world", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - they are not equal
}

void test_string_equal_fails(void) {
    char *args[] = { "test", "hello", "=", "world", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - they are not equal
}

// ============================================================================
// Integer Tests
// ============================================================================

void test_int_equal(void) {
    char *args[] = { "test", "42", "-eq", "42", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_int_not_equal(void) {
    char *args[] = { "test", "42", "-ne", "43", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_int_less_than(void) {
    char *args[] = { "test", "5", "-lt", "10", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_int_less_than_fails(void) {
    char *args[] = { "test", "10", "-lt", "5", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False
}

void test_int_greater_than(void) {
    char *args[] = { "test", "10", "-gt", "5", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_int_less_or_equal(void) {
    char *args[] = { "test", "5", "-le", "5", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_int_greater_or_equal(void) {
    char *args[] = { "test", "5", "-ge", "5", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

// ============================================================================
// Logical Operators
// ============================================================================

void test_not_operator(void) {
    char *args[] = { "test", "!", "-f", "/nonexistent", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - file doesn't exist, negated
}

void test_and_operator(void) {
    char *args[] = { "test", "-d", "/tmp", "-a", "-r", "/tmp", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - both conditions true
}

void test_and_operator_fails(void) {
    char *args[] = { "test", "-d", "/tmp", "-a", "-f", "/tmp", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - /tmp is not a file
}

void test_or_operator(void) {
    char *args[] = { "test", "-f", "/nonexistent", "-o", "-d", "/tmp", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - second condition is true
}

// ============================================================================
// Bracket Syntax
// ============================================================================

void test_bracket_syntax(void) {
    char *args[] = { "[", "-f", "/etc/passwd", "]", NULL };
    int result = builtin_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_bracket_missing_close(void) {
    char *args[] = { "[", "-f", "/etc/passwd", NULL };
    int result = builtin_bracket(args);
    TEST_ASSERT_EQUAL_INT(2, result);  // Error
}

void test_bracket_string_equal(void) {
    char *args[] = { "[", "abc", "=", "abc", "]", NULL };
    int result = builtin_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_empty_args(void) {
    char *args[] = { "test", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - no args
}

void test_single_string_arg(void) {
    char *args[] = { "test", "nonempty", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - nonempty string
}

void test_single_empty_string(void) {
    char *args[] = { "test", "", NULL };
    int result = builtin_test(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - empty string
}

// ============================================================================
// [[ ]] Double Bracket Tests
// ============================================================================

void test_double_bracket_file_exists(void) {
    char *args[] = { "[[", "-f", "/etc/passwd", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_double_bracket_missing_close(void) {
    char *args[] = { "[[", "-f", "/etc/passwd", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(2, result);  // Error - missing ]]
}

void test_double_bracket_string_equal(void) {
    char *args[] = { "[[", "hello", "==", "hello", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_double_bracket_string_not_equal(void) {
    char *args[] = { "[[", "hello", "!=", "world", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

void test_double_bracket_pattern_match(void) {
    char *args[] = { "[[", "hello.txt", "==", "*.txt", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - pattern match
}

void test_double_bracket_pattern_no_match(void) {
    char *args[] = { "[[", "hello.txt", "==", "*.md", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - no match
}

void test_double_bracket_regex_match(void) {
    char *args[] = { "[[", "hello123", "=~", "^hello[0-9]+$", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - regex match
}

void test_double_bracket_regex_no_match(void) {
    char *args[] = { "[[", "hello", "=~", "^world", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // False - no match
}

void test_double_bracket_string_less_than(void) {
    char *args[] = { "[[", "apple", "<", "banana", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - apple < banana
}

void test_double_bracket_string_greater_than(void) {
    char *args[] = { "[[", "zebra", ">", "apple", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - zebra > apple
}

void test_double_bracket_and(void) {
    char *args[] = { "[[", "-d", "/tmp", "&&", "-r", "/tmp", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - both conditions
}

void test_double_bracket_or(void) {
    char *args[] = { "[[", "-f", "/nonexistent", "||", "-d", "/tmp", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - second is true
}

void test_double_bracket_not(void) {
    char *args[] = { "[[", "!", "-f", "/nonexistent", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True - file doesn't exist
}

void test_double_bracket_int_compare(void) {
    char *args[] = { "[[", "10", "-gt", "5", "]]", NULL };
    int result = builtin_double_bracket(args);
    TEST_ASSERT_EQUAL_INT(0, result);  // True
}

int main(void) {
    UNITY_BEGIN();

    // File tests
    RUN_TEST(test_file_exists);
    RUN_TEST(test_file_not_exists);
    RUN_TEST(test_file_is_regular);
    RUN_TEST(test_file_is_directory);
    RUN_TEST(test_file_is_not_directory);
    RUN_TEST(test_file_is_readable);

    // String tests
    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_not_empty);
    RUN_TEST(test_string_nonempty_n);
    RUN_TEST(test_string_equal);
    RUN_TEST(test_string_not_equal);
    RUN_TEST(test_string_equal_fails);

    // Integer tests
    RUN_TEST(test_int_equal);
    RUN_TEST(test_int_not_equal);
    RUN_TEST(test_int_less_than);
    RUN_TEST(test_int_less_than_fails);
    RUN_TEST(test_int_greater_than);
    RUN_TEST(test_int_less_or_equal);
    RUN_TEST(test_int_greater_or_equal);

    // Logical operators
    RUN_TEST(test_not_operator);
    RUN_TEST(test_and_operator);
    RUN_TEST(test_and_operator_fails);
    RUN_TEST(test_or_operator);

    // Bracket syntax
    RUN_TEST(test_bracket_syntax);
    RUN_TEST(test_bracket_missing_close);
    RUN_TEST(test_bracket_string_equal);

    // Edge cases
    RUN_TEST(test_empty_args);
    RUN_TEST(test_single_string_arg);
    RUN_TEST(test_single_empty_string);

    // Double bracket tests
    RUN_TEST(test_double_bracket_file_exists);
    RUN_TEST(test_double_bracket_missing_close);
    RUN_TEST(test_double_bracket_string_equal);
    RUN_TEST(test_double_bracket_string_not_equal);
    RUN_TEST(test_double_bracket_pattern_match);
    RUN_TEST(test_double_bracket_pattern_no_match);
    RUN_TEST(test_double_bracket_regex_match);
    RUN_TEST(test_double_bracket_regex_no_match);
    RUN_TEST(test_double_bracket_string_less_than);
    RUN_TEST(test_double_bracket_string_greater_than);
    RUN_TEST(test_double_bracket_and);
    RUN_TEST(test_double_bracket_or);
    RUN_TEST(test_double_bracket_not);
    RUN_TEST(test_double_bracket_int_compare);

    return UNITY_END();
}
