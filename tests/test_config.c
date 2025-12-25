#include "unity.h"
#include "../src/config.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    config_init();
}

void tearDown(void) {
    // Clean up
}

// Test config initialization
void test_config_init(void) {
    config_init();
    TEST_ASSERT_EQUAL_INT(0, shell_config.alias_count);
    TEST_ASSERT_TRUE(shell_config.colors_enabled);
    TEST_ASSERT_TRUE(shell_config.show_welcome);
}

// Test adding an alias
void test_add_alias(void) {
    int result = config_add_alias("ll", "ls -lah");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);
}

// Test getting an alias
void test_get_alias(void) {
    config_add_alias("ll", "ls -lah");
    const char *value = config_get_alias("ll");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("ls -lah", value);
}

// Test getting nonexistent alias
void test_get_nonexistent_alias(void) {
    const char *value = config_get_alias("doesnotexist");
    TEST_ASSERT_NULL(value);
}

// Test updating an alias
void test_update_alias(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("ll", "ls -la");

    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);
    const char *value = config_get_alias("ll");
    TEST_ASSERT_EQUAL_STRING("ls -la", value);
}

// Test removing an alias
void test_remove_alias(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("la", "ls -A");

    int result = config_remove_alias("ll");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);

    const char *value = config_get_alias("ll");
    TEST_ASSERT_NULL(value);
}

// Test removing nonexistent alias
void test_remove_nonexistent_alias(void) {
    int result = config_remove_alias("doesnotexist");
    TEST_ASSERT_EQUAL_INT(-1, result);
}

// Test processing alias line
void test_process_alias_line(void) {
    char line[] = "alias ll='ls -lah'";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *value = config_get_alias("ll");
    TEST_ASSERT_EQUAL_STRING("ls -lah", value);
}

// Test processing alias with double quotes
void test_process_alias_double_quotes(void) {
    char line[] = "alias gs=\"git status\"";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *value = config_get_alias("gs");
    TEST_ASSERT_EQUAL_STRING("git status", value);
}

// Test processing comment
void test_process_comment(void) {
    char line[] = "# This is a comment";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(0, shell_config.alias_count);
}

// Test processing empty line
void test_process_empty_line(void) {
    char line[] = "";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test processing whitespace line
void test_process_whitespace_line(void) {
    char line[] = "   \t  ";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test processing export
void test_process_export(void) {
    char line[] = "export TEST_VAR=test_value";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);

    const char *value = getenv("TEST_VAR");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("test_value", value);
}

// Test processing set colors=on
void test_process_set_colors_on(void) {
    char line[] = "set colors=on";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(shell_config.colors_enabled);
}

// Test processing set welcome=off
void test_process_set_welcome_off(void) {
    char line[] = "set welcome=off";
    int result = config_process_line(line);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_FALSE(shell_config.show_welcome);
}

// Test multiple aliases
void test_multiple_aliases(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("la", "ls -A");
    config_add_alias("l", "ls -CF");

    TEST_ASSERT_EQUAL_INT(3, shell_config.alias_count);
    TEST_ASSERT_EQUAL_STRING("ls -lah", config_get_alias("ll"));
    TEST_ASSERT_EQUAL_STRING("ls -A", config_get_alias("la"));
    TEST_ASSERT_EQUAL_STRING("ls -CF", config_get_alias("l"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_config_init);
    RUN_TEST(test_add_alias);
    RUN_TEST(test_get_alias);
    RUN_TEST(test_get_nonexistent_alias);
    RUN_TEST(test_update_alias);
    RUN_TEST(test_remove_alias);
    RUN_TEST(test_remove_nonexistent_alias);
    RUN_TEST(test_process_alias_line);
    RUN_TEST(test_process_alias_double_quotes);
    RUN_TEST(test_process_comment);
    RUN_TEST(test_process_empty_line);
    RUN_TEST(test_process_whitespace_line);
    RUN_TEST(test_process_export);
    RUN_TEST(test_process_set_colors_on);
    RUN_TEST(test_process_set_welcome_off);
    RUN_TEST(test_multiple_aliases);

    return UNITY_END();
}
