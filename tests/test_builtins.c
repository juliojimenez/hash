#include "unity.h"
#include "../src/builtins.h"
#include "../src/execute.h"
#include "../src/shellvar.h"
#include <sys/stat.h>
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

// A scratch directory with real/sub and a symlink "link" to real, for -L/-P.
// Returns the directory's path in buf, or NULL (and skips the test) when it
// cannot be made.
static char *cd_fixture(char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "/tmp/hash-cd-test-XXXXXX");
    if (mkdtemp(buf) == NULL) {
        TEST_IGNORE_MESSAGE("mkdtemp failed");
        return NULL;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/real", buf);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/real/sub", buf);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/link", buf);
    if (symlink("real", path) != 0) {
        TEST_IGNORE_MESSAGE("symlink failed");
        return NULL;
    }
    return buf;
}

static void cd_fixture_remove(const char *dir) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/link", dir);
    unlink(path);
    snprintf(path, sizeof(path), "%s/real/sub", dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/real", dir);
    rmdir(path);
    rmdir(dir);
}

// The physical path of a directory, through getcwd()
static void physical_path(const char *dir, char *out, size_t outsz) {
    char save[PATH_MAX];
    getcwd(save, sizeof(save));
    chdir(dir);
    getcwd(out, outsz);
    chdir(save);
}

// cd -L (the default) keeps the symlink in PWD; cd .. then goes to the link's parent
void test_shell_cd_logical_keeps_symlink_and_dotdot_follows_it(void) {
    char dir[PATH_MAX];
    if (!cd_fixture(dir, sizeof(dir))) return;
    char physdir[PATH_MAX];
    physical_path(dir, physdir, sizeof(physdir));
    char *root[] = {"cd", physdir, NULL};
    shell_cd(root);

    char *args[] = {"cd", "link/sub", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(args));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    char expect[PATH_MAX];
    snprintf(expect, sizeof(expect), "%s/link/sub", physdir);
    TEST_ASSERT_EQUAL_STRING(expect, shellvar_get("PWD"));
    TEST_ASSERT_EQUAL_STRING(expect, getenv("PWD"));
    TEST_ASSERT_EQUAL_STRING(physdir, shellvar_get("OLDPWD"));

    char *up[] = {"cd", "../..", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(up));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING(physdir, shellvar_get("PWD"));   // parent of the link, not of real/sub

    chdir(original_dir);
    cd_fixture_remove(dir);
}

// cd -P resolves the symlink and PWD is the physical path
void test_shell_cd_physical_resolves_symlink(void) {
    char dir[PATH_MAX];
    if (!cd_fixture(dir, sizeof(dir))) return;
    char physdir[PATH_MAX];
    physical_path(dir, physdir, sizeof(physdir));
    char *root[] = {"cd", physdir, NULL};
    shell_cd(root);

    char *args[] = {"cd", "-P", "link", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(args));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    char expect[PATH_MAX];
    snprintf(expect, sizeof(expect), "%s/real", physdir);
    TEST_ASSERT_EQUAL_STRING(expect, shellvar_get("PWD"));

    // The last of -L and -P wins
    char *both[] = {"cd", "-P", "-L", "../link", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(both));
    snprintf(expect, sizeof(expect), "%s/link", physdir);
    TEST_ASSERT_EQUAL_STRING(expect, shellvar_get("PWD"));
    char *joined[] = {"cd", "-LP", ".", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(joined));
    snprintf(expect, sizeof(expect), "%s/real", physdir);
    TEST_ASSERT_EQUAL_STRING(expect, shellvar_get("PWD"));

    chdir(original_dir);
    cd_fixture_remove(dir);
}

// cd -- ends the options; cd -- - still means the previous directory
void test_shell_cd_double_dash(void) {
    char *args[] = {"cd", "--", "/usr", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(args));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    TEST_ASSERT_EQUAL_STRING("/usr", cwd);

    char *back[] = {"cd", "--", "-", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(back));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    getcwd(cwd, sizeof(cwd));
    TEST_ASSERT_EQUAL_STRING(original_dir, cwd);
}

// An unknown option fails with status 2 and does not change directory
void test_shell_cd_invalid_option(void) {
    char *args[] = {"cd", "-x", "/usr", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(args));
    TEST_ASSERT_EQUAL_INT(2, last_command_exit_code);
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    TEST_ASSERT_EQUAL_STRING(original_dir, cwd);
}

// A failed cd reports status 1 and leaves PWD alone
void test_shell_cd_failure_status(void) {
    const char *before = shellvar_get("PWD");
    char saved[PATH_MAX];
    snprintf(saved, sizeof(saved), "%s", before ? before : "");
    char *args[] = {"cd", "/this/directory/does/not/exist/12345", NULL};
    TEST_ASSERT_EQUAL_INT(1, shell_cd(args));
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING(saved, shellvar_get("PWD") ? shellvar_get("PWD") : "");
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

// Every builtin must document itself for `help`
void test_every_builtin_has_help_text(void) {
    for (int i = 0; i < BUILTIN_FUNC_MAX; i++) {
        TEST_ASSERT_NOT_NULL_MESSAGE(builtins[i].name, "builtin without a name");
        TEST_ASSERT_NOT_NULL_MESSAGE(builtins[i].synopsis, builtins[i].name);
        TEST_ASSERT_NOT_NULL_MESSAGE(builtins[i].description, builtins[i].name);
        TEST_ASSERT_TRUE_MESSAGE(builtins[i].synopsis[0] != '\0', builtins[i].name);
        TEST_ASSERT_TRUE_MESSAGE(builtins[i].description[0] != '\0', builtins[i].name);
        // The synopsis starts with the name, so the listing reads as a table
        TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(builtins[i].name, builtins[i].synopsis, strlen(builtins[i].name), builtins[i].name);
        // Descriptions end with a newline, one sentence block per line
        size_t len = strlen(builtins[i].description);
        TEST_ASSERT_TRUE_MESSAGE(builtins[i].description[len - 1] == '\n', builtins[i].name);
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_shell_exit_returns_zero);
    RUN_TEST(test_shell_cd_valid_directory);
    RUN_TEST(test_shell_cd_no_arguments);
    RUN_TEST(test_shell_cd_invalid_directory);
    RUN_TEST(test_shell_cd_logical_keeps_symlink_and_dotdot_follows_it);
    RUN_TEST(test_shell_cd_physical_resolves_symlink);
    RUN_TEST(test_shell_cd_double_dash);
    RUN_TEST(test_shell_cd_invalid_option);
    RUN_TEST(test_shell_cd_failure_status);
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

    RUN_TEST(test_every_builtin_has_help_text);
    return UNITY_END();
}
