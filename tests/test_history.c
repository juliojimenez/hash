#include "unity.h"
#include "../src/history.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void setUp(void) {
    // Use a non-existent file for tests to avoid loading user's actual history
    setenv("HISTFILE", "/tmp/hash_test_history_nonexistent_12345", 1);
    setenv("HISTSIZE", "100", 1);
    setenv("HISTFILESIZE", "200", 1);

    // Clear HISTCONTROL so tests use default behavior
    unsetenv("HISTCONTROL");

    // Initialize history (won't load anything since file doesn't exist)
    history_init();
}

void tearDown(void) {
    history_clear();

    // Clean up test file if it was created
    unlink("/tmp/hash_test_history_nonexistent_12345");

    // Restore defaults
    unsetenv("HISTFILE");
    unsetenv("HISTSIZE");
    unsetenv("HISTFILESIZE");
    unsetenv("HISTCONTROL");
}

// Test adding commands to history
void test_history_add(void) {
    history_add("ls -la");
    history_add("cd /tmp");

    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING("ls -la", history_get(0));
    TEST_ASSERT_EQUAL_STRING("cd /tmp", history_get(1));
}

// Test skipping empty lines
void test_history_skip_empty(void) {
    history_add("command1");
    history_add("");
    history_add("   ");
    history_add("command2");

    TEST_ASSERT_EQUAL_INT(2, history_count());
}

// Test skipping duplicate commands
void test_history_skip_duplicate(void) {
    history_add("ls");
    history_add("ls");
    history_add("pwd");
    history_add("pwd");

    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING("ls", history_get(0));
    TEST_ASSERT_EQUAL_STRING("pwd", history_get(1));
}

// Test skipping commands starting with space
void test_history_skip_space_prefix(void) {
    history_add("command1");
    history_add(" private");
    history_add("command2");

    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING("command1", history_get(0));
    TEST_ASSERT_EQUAL_STRING("command2", history_get(1));
}

// Test history navigation - prev
void test_history_prev(void) {
    history_add("cmd1");
    history_add("cmd2");
    history_add("cmd3");

    const char *prev = history_prev();
    TEST_ASSERT_EQUAL_STRING("cmd3", prev);

    prev = history_prev();
    TEST_ASSERT_EQUAL_STRING("cmd2", prev);

    prev = history_prev();
    TEST_ASSERT_EQUAL_STRING("cmd1", prev);

    // At oldest, should return same
    prev = history_prev();
    TEST_ASSERT_EQUAL_STRING("cmd1", prev);
}

// Test history navigation - next
void test_history_next(void) {
    history_add("cmd1");
    history_add("cmd2");
    history_add("cmd3");

    // Go to oldest
    history_prev();
    history_prev();
    history_prev();

    // Move forward
    const char *next = history_next();
    TEST_ASSERT_EQUAL_STRING("cmd2", next);

    next = history_next();
    TEST_ASSERT_EQUAL_STRING("cmd3", next);

    // At end, should return NULL
    next = history_next();
    TEST_ASSERT_NULL(next);
}

// Test history reset position
void test_history_reset(void) {
    history_add("cmd1");
    history_add("cmd2");

    history_prev();
    TEST_ASSERT_EQUAL_INT(1, history_get_position());

    history_reset_position();
    TEST_ASSERT_EQUAL_INT(-1, history_get_position());
}

// Test history search by prefix
void test_history_search_prefix(void) {
    history_add("git status");
    history_add("ls -la");
    history_add("git commit");
    history_add("pwd");

    const char *result = history_search_prefix("git");
    TEST_ASSERT_EQUAL_STRING("git commit", result);
}

// Test !! expansion
void test_history_expand_last(void) {
    history_add("echo hello");

    char *expanded = history_expand("!!");
    TEST_ASSERT_NOT_NULL(expanded);
    TEST_ASSERT_EQUAL_STRING("echo hello", expanded);

    free(expanded);
}

// Test !n expansion
void test_history_expand_number(void) {
    history_add("first");
    history_add("second");
    history_add("third");

    char *expanded = history_expand("!1");
    TEST_ASSERT_NOT_NULL(expanded);
    TEST_ASSERT_EQUAL_STRING("second", expanded);

    free(expanded);
}

// Test !-n expansion
void test_history_expand_relative(void) {
    history_add("cmd1");
    history_add("cmd2");
    history_add("cmd3");

    char *expanded = history_expand("!-2");
    TEST_ASSERT_NOT_NULL(expanded);
    TEST_ASSERT_EQUAL_STRING("cmd2", expanded);

    free(expanded);
}

// Test !prefix expansion
void test_history_expand_prefix(void) {
    history_add("git status");
    history_add("ls -la");
    history_add("git commit");

    char *expanded = history_expand("!git");
    TEST_ASSERT_NOT_NULL(expanded);
    TEST_ASSERT_EQUAL_STRING("git commit", expanded);

    free(expanded);
}

// Test escaped !
void test_history_expand_escaped(void) {
    char *expanded = history_expand("echo \\!");
    TEST_ASSERT_NOT_NULL(expanded);
    TEST_ASSERT_EQUAL_STRING("echo !", expanded);

    free(expanded);
}

// Test no expansion needed
void test_history_expand_none(void) {
    char *expanded = history_expand("echo hello");
    TEST_ASSERT_NULL(expanded);  // No expansion, returns NULL
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_history_add);
    RUN_TEST(test_history_skip_empty);
    RUN_TEST(test_history_skip_duplicate);
    RUN_TEST(test_history_skip_space_prefix);
    RUN_TEST(test_history_prev);
    RUN_TEST(test_history_next);
    RUN_TEST(test_history_reset);
    RUN_TEST(test_history_search_prefix);
    RUN_TEST(test_history_expand_last);
    RUN_TEST(test_history_expand_number);
    RUN_TEST(test_history_expand_relative);
    RUN_TEST(test_history_expand_prefix);
    RUN_TEST(test_history_expand_escaped);
    RUN_TEST(test_history_expand_none);

    return UNITY_END();
}
