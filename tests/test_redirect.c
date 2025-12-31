#include "unity.h"
#include "../src/redirect.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test parsing input redirection
void test_parse_input_redirect(void) {
    char *args[] = {"cat", "<", "input.txt", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_INPUT, info->redirs[0].type);
    TEST_ASSERT_EQUAL_STRING("input.txt", info->redirs[0].filename);

    // Check cleaned args
    TEST_ASSERT_EQUAL_STRING("cat", info->args[0]);
    TEST_ASSERT_NULL(info->args[1]);

    redirect_free(info);
}

// Test parsing output redirection
void test_parse_output_redirect(void) {
    char *args[] = {"echo", "hello", ">", "output.txt", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_OUTPUT, info->redirs[0].type);
    TEST_ASSERT_EQUAL_STRING("output.txt", info->redirs[0].filename);

    // Check cleaned args
    TEST_ASSERT_EQUAL_STRING("echo", info->args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", info->args[1]);
    TEST_ASSERT_NULL(info->args[2]);

    redirect_free(info);
}

// Test parsing append redirection
void test_parse_append_redirect(void) {
    char *args[] = {"echo", "line", ">>", "file.txt", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_APPEND, info->redirs[0].type);

    redirect_free(info);
}

// Test parsing error redirection
void test_parse_error_redirect(void) {
    char *args[] = {"command", "2>", "error.log", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_ERROR, info->redirs[0].type);

    redirect_free(info);
}

// Test parsing 2>&1
void test_parse_error_to_output(void) {
    char *args[] = {"command", "2>&1", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_ERROR_TO_OUT, info->redirs[0].type);
    TEST_ASSERT_NULL(info->redirs[0].filename);

    redirect_free(info);
}

// Test parsing &> (both)
void test_parse_both_redirect(void) {
    char *args[] = {"command", "&>", "all.log", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(1, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_BOTH, info->redirs[0].type);

    redirect_free(info);
}

// Test multiple redirections
void test_parse_multiple_redirects(void) {
    char *args[] = {"cat", "<", "input.txt", ">", "output.txt", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(2, info->count);
    TEST_ASSERT_EQUAL_INT(REDIR_INPUT, info->redirs[0].type);
    TEST_ASSERT_EQUAL_INT(REDIR_OUTPUT, info->redirs[1].type);

    // Args should only have "cat"
    TEST_ASSERT_EQUAL_STRING("cat", info->args[0]);
    TEST_ASSERT_NULL(info->args[1]);

    redirect_free(info);
}

// Test no redirections
void test_parse_no_redirects(void) {
    char *args[] = {"echo", "hello", NULL};

    RedirInfo *info = redirect_parse(args);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_INT(0, info->count);

    // Args unchanged
    TEST_ASSERT_EQUAL_STRING("echo", info->args[0]);
    TEST_ASSERT_EQUAL_STRING("hello", info->args[1]);

    redirect_free(info);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_input_redirect);
    RUN_TEST(test_parse_output_redirect);
    RUN_TEST(test_parse_append_redirect);
    RUN_TEST(test_parse_error_redirect);
    RUN_TEST(test_parse_error_to_output);
    RUN_TEST(test_parse_both_redirect);
    RUN_TEST(test_parse_multiple_redirects);
    RUN_TEST(test_parse_no_redirects);

    return UNITY_END();
}
