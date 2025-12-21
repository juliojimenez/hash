#include "unity.h"
#include "../src/builtins.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char original_dir[PATH_MAX];

void setUp(void) {
    // Save current directory
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}

void tearDown(void) {
    // Restore original directory
    if (chdir(original_dir) != 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }
}

// Test shell_exit returns 0
void test_shell_exit_returns_zero(void) {
    char *args[] = {"exit", NULL};
    int result = shell_exit(args);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test shell_cd to valid directory
void test_shell_cd_valid_directory(void) {
    char *args[] = {"cd", "/tmp", NULL};
    int result = shell_cd(args);

    TEST_ASSERT_EQUAL_INT(1, result);

    // Verify we actually changed directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    TEST_ASSERT_EQUAL_STRING("/tmp", cwd);
}

// Test shell_cd with no arguments
void test_shell_cd_no_arguments(void) {
    char *args[] = {"cd", NULL};
    int result = shell_cd(args);

    // Should return 1 (continue shell) but print error
    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test shell_cd to invalid directory
void test_shell_cd_invalid_directory(void) {
    char *args[] = {"cd", "/this/directory/does/not/exist/12345", NULL};
    int result = shell_cd(args);

    // Should return 1 (continue shell) but print error
    TEST_ASSERT_EQUAL_INT(1, result);

    // Verify we stayed in original directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    TEST_ASSERT_EQUAL_STRING(original_dir, cwd);
}

// Test try_builtin with cd command
void test_try_builtin_cd(void) {
    char *args[] = {"cd", "/tmp", NULL};
    int result = try_builtin(args);

    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test try_builtin with exit command
void test_try_builtin_exit(void) {
    char *args[] = {"exit", NULL};
    int result = try_builtin(args);

    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test try_builtin with non-builtin command
void test_try_builtin_not_builtin(void) {
    char *args[] = {"ls", "-la", NULL};
    int result = try_builtin(args);

    TEST_ASSERT_EQUAL_INT(-1, result);
}

// Test try_builtin with NULL args
void test_try_builtin_null_args(void) {
    char *args[] = {NULL};
    int result = try_builtin(args);

    TEST_ASSERT_EQUAL_INT(-1, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_shell_exit_returns_zero);
    RUN_TEST(test_shell_cd_valid_directory);
    RUN_TEST(test_shell_cd_no_arguments);
    RUN_TEST(test_shell_cd_invalid_directory);
    RUN_TEST(test_try_builtin_cd);
    RUN_TEST(test_try_builtin_exit);
    RUN_TEST(test_try_builtin_not_builtin);
    RUN_TEST(test_try_builtin_null_args);

    return UNITY_END();
}
