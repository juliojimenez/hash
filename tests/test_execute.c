#include "unity.h"
#include "../src/execute.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

// Test execute with NULL args
void test_execute_null_args(void) {
    char *args[] = {NULL};
    int result = execute(args);

    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test execute with builtin command (cd)
void test_execute_builtin_cd(void) {
    char *args[] = {"cd", "/tmp", NULL};
    int result = execute(args);

    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test execute with builtin command (exit)
void test_execute_builtin_exit(void) {
    char *args[] = {"exit", NULL};
    int result = execute(args);

    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test execute with simple external command
void test_execute_external_command_true(void) {
    char *args[] = {"/bin/true", NULL};
    int result = execute(args);

    // Should return 1 (continue shell)
    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test execute with command that takes arguments
void test_execute_external_command_with_args(void) {
    char *args[] = {"echo", "test", NULL};
    int result = execute(args);

    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test execute with invalid command
void test_execute_invalid_command(void) {
    char *args[] = {"this_command_does_not_exist_12345", NULL};
    int result = execute(args);

    // Should return 1 (continue shell) even though command failed
    TEST_ASSERT_EQUAL_INT(1, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_execute_null_args);
    RUN_TEST(test_execute_builtin_cd);
    RUN_TEST(test_execute_builtin_exit);
    RUN_TEST(test_execute_external_command_true);
    RUN_TEST(test_execute_external_command_with_args);
    RUN_TEST(test_execute_invalid_command);

    return UNITY_END();
}
