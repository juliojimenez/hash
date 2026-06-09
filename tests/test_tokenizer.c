#include "unity.h"
#include "../src/tokenizer.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

static void assert_token(Token *t, TokenType type, const char *expected, size_t line, size_t col) {
    TEST_ASSERT_EQUAL_INT(type, t->type);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, t->value.str, t->value.len);
    TEST_ASSERT_EQUAL_UINT(line, t->line);
    TEST_ASSERT_EQUAL_UINT(col, t->col);
}

static void assert_eof(Token *t) {
    TEST_ASSERT_EQUAL_INT(TOKEN_EOF, t->type);
}

void test_empty_input(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "");
    Token t = tokenizer_next(&tk);
    assert_eof(&t);
    tokenizer_destroy(&tk);
}

void test_whitespace_only(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "   \t   ");
    Token t = tokenizer_next(&tk);
    assert_eof(&t);
    tokenizer_destroy(&tk);
}

void test_simple_words(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo hello world");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "echo", 0, 0);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "hello", 0, 5);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "world", 0, 11);

    t = tokenizer_next(&tk);
    assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_pipe_operator(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a | b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_PIPE, "|", 0, 2);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 4);

    tokenizer_destroy(&tk);
}

void test_or_if(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a || b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_OR_IF, "||", 0, 2);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 5);
    t = tokenizer_next(&tk);
    assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_and_if(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a && b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_AND_IF, "&&", 0, 2);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 5);

    tokenizer_destroy(&tk);
}

void test_semi(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a; b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_SEMI, ";", 0, 1);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 3);

    tokenizer_destroy(&tk);
}

void test_ampersand(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a & b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_AMPERSAND, "&", 0, 2);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 4);

    tokenizer_destroy(&tk);
}

void test_dsemi(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a;;b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_DSEMI, ";;", 0, 1);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "b", 0, 3);

    tokenizer_destroy(&tk);
}

void test_lparen_rparen(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "( echo )");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_LPAREN, "(", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "echo", 0, 2);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_RPAREN, ")", 0, 7);

    tokenizer_destroy(&tk);
}

void test_redirect_less_great(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "< > >> << <<- <& >& <>");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_LESS, "<", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_GREAT, ">", 0, 2);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_DGREAT, ">>", 0, 4);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_DLESS, "<<", 0, 7);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_DLESSDASH, "<<-", 0, 10);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_LESSAND, "<&", 0, 14);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_GREATAND, ">&", 0, 17);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_LESSGREAT, "<>", 0, 20);
    t = tokenizer_next(&tk); assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_io_number(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "2>file");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_IO_NUMBER, "2", 0, 0);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_GREAT, ">", 0, 1);
    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "file", 0, 2);

    tokenizer_destroy(&tk);
}

void test_io_number_not_word(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "2file");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "2file", 0, 0);

    tokenizer_destroy(&tk);
}

void test_single_quotes(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo 'hello world'");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "'hello world'", 0, 5);
    t = tokenizer_next(&tk); assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_double_quotes(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \"hello world\"");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "\"hello world\"", 0, 5);
    t = tokenizer_next(&tk); assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_mixed_quotes(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \"double\" 'single' unquoted");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "\"double\"", 0, 5);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "'single'", 0, 14);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "unquoted", 0, 23);

    tokenizer_destroy(&tk);
}

void test_backslash_escape(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \\$var");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "\\$var", 0, 5);

    tokenizer_destroy(&tk);
}

void test_arithmetic_expansion(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo $(( 1 + 2 ))");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$(( 1 + 2 ))", 0, 5);

    tokenizer_destroy(&tk);
}

void test_nested_arithmetic(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo $(( 1 + $((2 + 3)) ))");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$(( 1 + $((2 + 3)) ))", 0, 5);

    tokenizer_destroy(&tk);
}

void test_cmdsub(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo $(echo hello)");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$(echo hello)", 0, 5);

    tokenizer_destroy(&tk);
}

void test_nested_cmdsub(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo $(echo $(echo nested))");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$(echo $(echo nested))", 0, 5);

    tokenizer_destroy(&tk);
}

void test_brace_expansion(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo ${HOME:-/tmp}");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "${HOME:-/tmp}", 0, 5);

    tokenizer_destroy(&tk);
}

void test_nested_brace(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo ${var:-${other}}");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "${var:-${other}}", 0, 5);

    tokenizer_destroy(&tk);
}

void test_simple_var(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo $HOME $? $$ $!");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$HOME", 0, 5);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$?", 0, 11);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$$", 0, 14);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$!", 0, 17);

    tokenizer_destroy(&tk);
}

void test_backtick(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo `echo hi`");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "`echo hi`", 0, 5);

    tokenizer_destroy(&tk);
}

void test_var_in_double_quotes(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \"hello $USER\"");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "\"hello $USER\"", 0, 5);

    tokenizer_destroy(&tk);
}

void test_newline_token(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a\nb");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "a", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_NEWLINE, "\n", 0, 1);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "b", 1, 0);

    tokenizer_destroy(&tk);
}

void test_comment_skipped(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "# comment\necho hi");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_NEWLINE, "\n", 0, 9);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "echo", 1, 0);

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "hi", 1, 5);

    t = tokenizer_next(&tk);
    assert_eof(&t);

    tokenizer_destroy(&tk);
}

void test_comment_not_midword(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "a#b");
    Token t;

    t = tokenizer_next(&tk);
    assert_token(&t, TOKEN_WORD, "a#b", 0, 0);

    tokenizer_destroy(&tk);
}

void test_empty_quotes(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \"\" ''");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "\"\"", 0, 5);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "''", 0, 8);

    tokenizer_destroy(&tk);
}

void test_multiple_tokens(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "for x in 1 2 3; do echo $x; done");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "for", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "x", 0, 4);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "in", 0, 6);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "1", 0, 9);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "2", 0, 11);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "3", 0, 13);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_SEMI, ";", 0, 14);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "do", 0, 16);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 19);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "$x", 0, 24);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_SEMI, ";", 0, 26);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "done", 0, 28);

    tokenizer_destroy(&tk);
}

void test_redirect_with_fd(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "cat < input 2> err >> log");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "cat", 0, 0);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_LESS, "<", 0, 4);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "input", 0, 6);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_IO_NUMBER, "2", 0, 12);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_GREAT, ">", 0, 13);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "err", 0, 15);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_DGREAT, ">>", 0, 19);
    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "log", 0, 22);

    tokenizer_destroy(&tk);
}

void test_line_continuation(void) {
    Tokenizer tk;
    tokenizer_init(&tk, "echo \\\nhello");
    Token t;

    t = tokenizer_next(&tk); assert_token(&t, TOKEN_WORD, "echo", 0, 0);
    t = tokenizer_next(&tk);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_UINT(7, t.value.len);
    TEST_ASSERT_EQUAL_CHAR('\\', t.value.str[0]);
    TEST_ASSERT_EQUAL_CHAR('\n', t.value.str[1]);
    TEST_ASSERT_EQUAL_STRING_LEN("hello", t.value.str + 2, 5);
    t = tokenizer_next(&tk);
    assert_eof(&t);

    tokenizer_destroy(&tk);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_input);
    RUN_TEST(test_whitespace_only);
    RUN_TEST(test_simple_words);

    // Operators
    RUN_TEST(test_pipe_operator);
    RUN_TEST(test_or_if);
    RUN_TEST(test_and_if);
    RUN_TEST(test_semi);
    RUN_TEST(test_ampersand);
    RUN_TEST(test_dsemi);
    RUN_TEST(test_lparen_rparen);
    RUN_TEST(test_redirect_less_great);

    // IO_NUMBER
    RUN_TEST(test_io_number);
    RUN_TEST(test_io_number_not_word);

    // Quotes
    RUN_TEST(test_single_quotes);
    RUN_TEST(test_double_quotes);
    RUN_TEST(test_mixed_quotes);
    RUN_TEST(test_backslash_escape);
    RUN_TEST(test_empty_quotes);

    // Expansions within words
    RUN_TEST(test_arithmetic_expansion);
    RUN_TEST(test_nested_arithmetic);
    RUN_TEST(test_cmdsub);
    RUN_TEST(test_nested_cmdsub);
    RUN_TEST(test_brace_expansion);
    RUN_TEST(test_nested_brace);
    RUN_TEST(test_simple_var);
    RUN_TEST(test_backtick);
    RUN_TEST(test_var_in_double_quotes);

    // Newlines and comments
    RUN_TEST(test_newline_token);
    RUN_TEST(test_comment_skipped);
    RUN_TEST(test_comment_not_midword);

    // Combined
    RUN_TEST(test_multiple_tokens);
    RUN_TEST(test_redirect_with_fd);
    RUN_TEST(test_line_continuation);

    return UNITY_END();
}
