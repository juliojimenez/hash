#include "unity.h"
#include "../src/pipeline.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test parsing simple pipe
void test_parse_simple_pipe(void) {
    char line[] = "ls | grep txt";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NOT_NULL(pipe);
    TEST_ASSERT_EQUAL_INT(2, pipe->count);
    TEST_ASSERT_EQUAL_STRING("ls", pipe->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_STRING("grep txt", pipe->commands[1].cmd_line);

    pipeline_free(pipe);
}

// Test parsing three-stage pipe
void test_parse_three_stage_pipe(void) {
    char line[] = "cat file | grep pattern | wc -l";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NOT_NULL(pipe);
    TEST_ASSERT_EQUAL_INT(3, pipe->count);
    TEST_ASSERT_EQUAL_STRING("cat file", pipe->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_STRING("grep pattern", pipe->commands[1].cmd_line);
    TEST_ASSERT_EQUAL_STRING("wc -l", pipe->commands[2].cmd_line);

    pipeline_free(pipe);
}

// Test parsing pipe with whitespace
void test_parse_pipe_whitespace(void) {
    char line[] = "  ls  |  grep txt  |  wc  ";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NOT_NULL(pipe);
    TEST_ASSERT_EQUAL_INT(3, pipe->count);
    TEST_ASSERT_EQUAL_STRING("ls", pipe->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_STRING("grep txt", pipe->commands[1].cmd_line);
    TEST_ASSERT_EQUAL_STRING("wc", pipe->commands[2].cmd_line);

    pipeline_free(pipe);
}

// Test pipe in quotes is ignored
void test_parse_pipe_in_quotes(void) {
    char line[] = "echo \"test | test\"";
    Pipeline *pipe = pipeline_parse(line);

    // Should return NULL (single command, no actual pipes)
    TEST_ASSERT_NULL(pipe);
}

// Test || is not a pipe (it's OR operator)
void test_parse_or_not_pipe(void) {
    char line[] = "false || echo fallback";
    Pipeline *pipe = pipeline_parse(line);

    // Should return NULL (|| is not a pipe)
    TEST_ASSERT_NULL(pipe);
}

// Test single command returns NULL
void test_parse_single_command(void) {
    char line[] = "ls -la";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NULL(pipe);
}

// Test empty line
void test_parse_empty_line(void) {
    char line[] = "";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NULL(pipe);
}

// Test pipe with complex commands
void test_parse_complex_pipe(void) {
    char line[] = "ls -la /tmp | grep -v '^d' | wc -l";
    Pipeline *pipe = pipeline_parse(line);

    TEST_ASSERT_NOT_NULL(pipe);
    TEST_ASSERT_EQUAL_INT(3, pipe->count);
    TEST_ASSERT_EQUAL_STRING("ls -la /tmp", pipe->commands[0].cmd_line);

    pipeline_free(pipe);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_simple_pipe);
    RUN_TEST(test_parse_three_stage_pipe);
    RUN_TEST(test_parse_pipe_whitespace);
    RUN_TEST(test_parse_pipe_in_quotes);
    RUN_TEST(test_parse_or_not_pipe);
    RUN_TEST(test_parse_single_command);
    RUN_TEST(test_parse_empty_line);
    RUN_TEST(test_parse_complex_pipe);

    return UNITY_END();
}
