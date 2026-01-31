#include "unity.h"
#include "../src/danger.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test safe commands
void test_danger_safe_commands(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("ls -la", 6));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("cd /tmp", 7));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("echo hello", 10));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("cat file.txt", 12));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("grep pattern file", 17));
}

// Test rm without dangerous flags
void test_danger_rm_safe(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("rm file.txt", 11));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("rm -i file.txt", 14));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("rm foo bar", 10));
}

// Test rm -rf with root
void test_danger_rm_rf_root(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf /", 8));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf /*", 9));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -fr /", 8));
}

// Test rm -rf with home
void test_danger_rm_rf_home(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf ~", 8));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf ~/", 9));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf $HOME", 12));
}

// Test rm -rf with current directory
void test_danger_rm_rf_current(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf .", 8));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("rm -rf ./", 9));
}

// Test rm -rf with wildcard (medium danger)
void test_danger_rm_rf_wildcard(void) {
    TEST_ASSERT_EQUAL(DANGER_MEDIUM, danger_check("rm -rf *", 8));
    TEST_ASSERT_EQUAL(DANGER_MEDIUM, danger_check("rm -rf *.txt", 12));
}

// Test rm with specific paths (safe)
void test_danger_rm_rf_specific_path(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("rm -rf /tmp/test", 16));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("rm -rf ./subdir/", 16));
}

// Test chmod 777
void test_danger_chmod_777(void) {
    TEST_ASSERT_EQUAL(DANGER_MEDIUM, danger_check("chmod 777 file", 14));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("chmod -R 777 /", 14));
}

// Test chmod safe
void test_danger_chmod_safe(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("chmod 755 file", 14));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("chmod 644 file", 14));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("chmod +x script.sh", 18));
}

// Test dd to device
void test_danger_dd_device(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("dd if=/dev/zero of=/dev/sda", 27));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("dd of=/dev/sda if=image.iso", 27));
}

// Test dd safe
void test_danger_dd_safe(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("dd if=/dev/zero of=file.img", 27));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("dd if=input of=output", 21));
}

// Test mkfs commands
void test_danger_mkfs(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("mkfs.ext4 /dev/sda1", 19));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("mkfs -t ext4 /dev/sda1", 22));
}

// Test redirect to device
void test_danger_redirect_device(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("cat file > /dev/sda", 19));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check("echo test >/dev/sda", 19));
}

// Test empty and NULL input
void test_danger_empty_input(void) {
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check("", 0));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check(NULL, 0));
}

// Test danger_check_command directly
void test_danger_check_command_rm(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check_command("rm", " -rf /"));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check_command("rm", " -rf ~"));
    TEST_ASSERT_EQUAL(DANGER_MEDIUM, danger_check_command("rm", " -rf *"));
    TEST_ASSERT_EQUAL(DANGER_NONE, danger_check_command("rm", " file.txt"));
}

// Test danger_check_command with path
void test_danger_check_command_with_path(void) {
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check_command("/bin/rm", " -rf /"));
    TEST_ASSERT_EQUAL(DANGER_HIGH, danger_check_command("/usr/bin/dd", " of=/dev/sda"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_danger_safe_commands);
    RUN_TEST(test_danger_rm_safe);
    RUN_TEST(test_danger_rm_rf_root);
    RUN_TEST(test_danger_rm_rf_home);
    RUN_TEST(test_danger_rm_rf_current);
    RUN_TEST(test_danger_rm_rf_wildcard);
    RUN_TEST(test_danger_rm_rf_specific_path);
    RUN_TEST(test_danger_chmod_777);
    RUN_TEST(test_danger_chmod_safe);
    RUN_TEST(test_danger_dd_device);
    RUN_TEST(test_danger_dd_safe);
    RUN_TEST(test_danger_mkfs);
    RUN_TEST(test_danger_redirect_device);
    RUN_TEST(test_danger_empty_input);
    RUN_TEST(test_danger_check_command_rm);
    RUN_TEST(test_danger_check_command_with_path);

    return UNITY_END();
}
