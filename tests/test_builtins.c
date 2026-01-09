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
    // Use /usr which exists on both Linux and macOS without symlink issues
    char *args[] = {"cd", "/usr", NULL};
    int result = shell_cd(args);

    TEST_ASSERT_EQUAL_INT(1, result);

    // Verify we actually changed directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    TEST_ASSERT_EQUAL_STRING("/usr", cwd);
}

// Test shell_cd with no arguments (should go to home)
void test_shell_cd_no_arguments(void) {
    char *args[] = {"cd", NULL};
    int result = shell_cd(args);

    // Should return 1 (continue shell) and change to home directory
    TEST_ASSERT_EQUAL_INT(1, result);

    // Verify we're now in home directory
    const char *home = getenv("HOME");
    if (home) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            TEST_ASSERT_EQUAL_STRING(home, cwd);
        } else {
            TEST_FAIL_MESSAGE("getcwd failed");
        }
    }
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
    // Use /usr which exists on both Linux and macOS without symlink issues
    char *args[] = {"cd", "/usr", NULL};
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

// Test shell_logout in non-login shell (should fail)
void test_shell_logout_not_login_shell(void) {
    // By default, is_login_shell is false
    builtins_set_login_shell(false);

    char *args[] = {"logout", NULL};
    int result = shell_logout(args);

    // Should return 1 (continue shell) because not a login shell
    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test shell_logout in login shell (should succeed)
void test_shell_logout_login_shell(void) {
    // Set as login shell
    builtins_set_login_shell(true);

    char *args[] = {"logout", NULL};
    int result = shell_logout(args);

    // Should return 0 (exit shell)
    TEST_ASSERT_EQUAL_INT(0, result);

    // Reset to non-login shell for other tests
    builtins_set_login_shell(false);
}

// Test shell_command with no arguments
void test_shell_command_no_args(void) {
    char *args[] = {"command", NULL};
    int result = shell_command(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_command -v with builtin
void test_shell_command_v_builtin(void) {
    char *args[] = {"command", "-v", "echo", NULL};
    int result = shell_command(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_command -V with builtin
void test_shell_command_V_builtin(void) {
    char *args[] = {"command", "-V", "echo", NULL};
    int result = shell_command(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_exec with no arguments
void test_shell_exec_no_args(void) {
    char *args[] = {"exec", NULL};
    int result = shell_exec(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_times returns successfully
void test_shell_times(void) {
    char *args[] = {"times", NULL};
    int result = shell_times(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_type with no arguments
void test_shell_type_no_args(void) {
    char *args[] = {"type", NULL};
    int result = shell_type(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test shell_type with builtin
void test_shell_type_builtin(void) {
    char *args[] = {"type", "cd", NULL};
    int result = shell_type(args);
    TEST_ASSERT_EQUAL_INT(1, result);  // Continue shell
}

// Test try_builtin recognizes new builtins
void test_try_builtin_command(void) {
    char *args[] = {"command", NULL};
    int result = try_builtin(args);
    TEST_ASSERT_NOT_EQUAL_INT(-1, result);  // Should recognize command
}

void test_try_builtin_exec(void) {
    char *args[] = {"exec", NULL};
    int result = try_builtin(args);
    TEST_ASSERT_NOT_EQUAL_INT(-1, result);  // Should recognize exec
}

void test_try_builtin_times(void) {
    char *args[] = {"times", NULL};
    int result = try_builtin(args);
    TEST_ASSERT_NOT_EQUAL_INT(-1, result);  // Should recognize times
}

void test_try_builtin_type(void) {
    char *args[] = {"type", NULL};
    int result = try_builtin(args);
    TEST_ASSERT_NOT_EQUAL_INT(-1, result);  // Should recognize type
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
    RUN_TEST(test_shell_logout_not_login_shell);
    RUN_TEST(test_shell_logout_login_shell);
    RUN_TEST(test_shell_command_no_args);
    RUN_TEST(test_shell_command_v_builtin);
    RUN_TEST(test_shell_command_V_builtin);
    RUN_TEST(test_shell_exec_no_args);
    RUN_TEST(test_shell_times);
    RUN_TEST(test_shell_type_no_args);
    RUN_TEST(test_shell_type_builtin);
    RUN_TEST(test_try_builtin_command);
    RUN_TEST(test_try_builtin_exec);
    RUN_TEST(test_try_builtin_times);
    RUN_TEST(test_try_builtin_type);

    return UNITY_END();
}
