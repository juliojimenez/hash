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

// A real 0x01..0x04 byte in the input is not one of the parser's markers: it
// leaves parse_line as \x01 + byte, which the later marker removal turns back
// into the byte (#354)
void test_parse_line_protects_marker_bytes(void) {
    char line[] = "echo a\x01" "b c\x03" "d";
    ParseResult parsed = parse_line(line);
    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("a\x01\x01" "b", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("c\x01\x03" "d", parsed.tokens[2]);
    TEST_ASSERT_NULL(parsed.tokens[3]);
    parse_result_free(&parsed);

    char quoted[] = "echo 'x\x02" "y' \"\x04" "z\"";
    parsed = parse_line(quoted);
    TEST_ASSERT_EQUAL_STRING("x\x01\x02" "y", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("\x01\x04" "z", parsed.tokens[2]);
    parse_result_free(&parsed);
}

// The word of ${var:-word} and of the ${var#word} pattern forms keeps its
// quotes for varexpand to honour (#375, #355); a } inside those quotes does
// not end the expansion
void test_parse_line_keeps_quotes_in_default_word(void) {
    char line[] = "echo ${w:-\"a b\"} ${v+'x y'} ${u:=\"q\"} ${p#\"a\"} ${q%\"a}\"}";
    ParseResult parsed = parse_line(line);
    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("${w:-\"a b\"}", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("${v+'x y'}", parsed.tokens[2]);
    TEST_ASSERT_EQUAL_STRING("${u:=\"q\"}", parsed.tokens[3]);
    TEST_ASSERT_EQUAL_STRING("${p#\"a\"}", parsed.tokens[4]);
    TEST_ASSERT_EQUAL_STRING("${q%\"a}\"}", parsed.tokens[5]);
    TEST_ASSERT_NULL(parsed.tokens[6]);
    parse_result_free(&parsed);
}

// A variable name directly followed by a quote keeps its boundary: the quote
// is removed here, so $v"ro" would read as $vro; it is written as ${v}ro (#374)
void test_parse_line_braces_variable_before_quote(void) {
    char line[] = "echo $v\"ro\" \"$v\"ro $v'ro' $1\"x\" $?\"x\" $$\"x\" $v ro $v";
    ParseResult parsed = parse_line(line);
    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("${v}ro", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("\x02${v}ro", parsed.tokens[2]);
    TEST_ASSERT_EQUAL_STRING("${v}ro", parsed.tokens[3]);
    TEST_ASSERT_EQUAL_STRING("${1}x", parsed.tokens[4]);
    TEST_ASSERT_EQUAL_STRING("${?}x", parsed.tokens[5]);
    TEST_ASSERT_EQUAL_STRING("${$}x", parsed.tokens[6]);
    TEST_ASSERT_EQUAL_STRING("$v", parsed.tokens[7]);       // no quote follows: unchanged
    TEST_ASSERT_EQUAL_STRING("ro", parsed.tokens[8]);
    TEST_ASSERT_EQUAL_STRING("$v", parsed.tokens[9]);
    TEST_ASSERT_NULL(parsed.tokens[10]);
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
    // an escaped quote is a marked literal (\x01 before it): a bare one flipped the quote tracking of later passes
    TEST_ASSERT_EQUAL_STRING("He said \x01\"hello\x01\"", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with escaped single quote
void test_parse_line_escaped_single_quote(void) {
    char line[] = "echo 'can'\\''t'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    // an escaped quote is a marked literal (\x01 before it): a bare one flipped the quote tracking of later passes
    TEST_ASSERT_EQUAL_STRING("can\x01't", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);

    parse_result_free(&parsed);
}

// Test parse_line with backslash escape
void test_parse_line_escaped_backslash(void) {
    char line[] = "echo \"path\\\\to\\\\file\"";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    // an escaped backslash in double quotes is a marked literal now (\x01 before it), like one outside quotes
    TEST_ASSERT_EQUAL_STRING("path\x01\\to\x01\\file", parsed.tokens[1]);
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

// Test parse_line with literal backslash in single quotes: the backslash is
// marked (\x01) whatever follows it, so it cannot escape the byte after the
// closing quote ('a\'$x expands $x, 'a\'`echo q` runs the command)
void test_parse_line_single_quote_literal_backslash(void) {
    char line[] = "echo 'literal\\n'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("echo", parsed.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("literal\x01\\n", parsed.tokens[1]);
    TEST_ASSERT_NULL(parsed.tokens[2]);
    parse_result_free(&parsed);

    char line2[] = "echo 'a\\'$x";
    parsed = parse_line(line2);
    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("a\x01\\$x", parsed.tokens[1]);
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

// $'...' decodes C-style escapes into quoted text: a pattern character comes
// out marked, an ordinary one plain
void test_parse_line_dollar_single_quotes(void) {
    char line[] = "echo $'a\\tb' $'*' $$'x'";
    ParseResult parsed = parse_line(line);

    TEST_ASSERT_NOT_NULL(parsed.tokens);
    TEST_ASSERT_EQUAL_STRING("a\tb", parsed.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("\x01*", parsed.tokens[2]);
    TEST_ASSERT_EQUAL_STRING("${$}x", parsed.tokens[3]);  // the quote after $$ ends the name: ${$} keeps the boundary
    TEST_ASSERT_NULL(parsed.tokens[4]);
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
    RUN_TEST(test_parse_line_dollar_single_quotes);
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
    RUN_TEST(test_parse_line_protects_marker_bytes);
    RUN_TEST(test_parse_line_keeps_quotes_in_default_word);
    RUN_TEST(test_parse_line_braces_variable_before_quote);

    return UNITY_END();
}
