#include <stdio.h>
#include <unistd.h>
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

// Test substring search - basic match
void test_history_search_substring_basic(void) {
    history_add("git status");
    history_add("ls -la");
    history_add("git commit -m 'test'");
    history_add("echo hello");

    int result_idx = -1;
    const char *match = history_search_substring("git", -1, 1, &result_idx);

    TEST_ASSERT_NOT_NULL(match);
    TEST_ASSERT_EQUAL_STRING("git commit -m 'test'", match);
    TEST_ASSERT_EQUAL_INT(2, result_idx);
}

// Test substring search - cycle to older match
void test_history_search_substring_cycle(void) {
    history_add("git status");
    history_add("ls -la");
    history_add("git commit");

    int result_idx = -1;
    const char *match = history_search_substring("git", -1, 1, &result_idx);
    TEST_ASSERT_EQUAL_STRING("git commit", match);
    TEST_ASSERT_EQUAL_INT(2, result_idx);

    // Search for older match starting from position before current match
    match = history_search_substring("git", result_idx - 1, 1, &result_idx);
    TEST_ASSERT_EQUAL_STRING("git status", match);
    TEST_ASSERT_EQUAL_INT(0, result_idx);
}

// Test substring search - no match
void test_history_search_substring_no_match(void) {
    history_add("ls -la");
    history_add("pwd");

    int result_idx = -1;
    const char *match = history_search_substring("git", -1, 1, &result_idx);

    TEST_ASSERT_NULL(match);
    TEST_ASSERT_EQUAL_INT(-1, result_idx);
}

// Test substring search - middle of command
void test_history_search_substring_middle(void) {
    history_add("echo hello world");
    history_add("cat file.txt");

    int result_idx = -1;
    const char *match = history_search_substring("hello", -1, 1, &result_idx);

    TEST_ASSERT_NOT_NULL(match);
    TEST_ASSERT_EQUAL_STRING("echo hello world", match);
}

// Test forward search
void test_history_search_substring_forward(void) {
    history_add("git status");
    history_add("ls -la");
    history_add("git commit");

    int result_idx = -1;
    // Start forward search from beginning
    const char *match = history_search_substring("git", 0, -1, &result_idx);
    TEST_ASSERT_EQUAL_STRING("git status", match);
    TEST_ASSERT_EQUAL_INT(0, result_idx);

    // Continue forward search
    match = history_search_substring("git", result_idx + 1, -1, &result_idx);
    TEST_ASSERT_EQUAL_STRING("git commit", match);
    TEST_ASSERT_EQUAL_INT(2, result_idx);
}

// Test empty query
void test_history_search_substring_empty(void) {
    history_add("git status");

    int result_idx = -1;
    const char *match = history_search_substring("", -1, 1, &result_idx);

    TEST_ASSERT_NULL(match);
}

// Test empty history
void test_history_search_substring_empty_history(void) {
    int result_idx = -1;
    const char *match = history_search_substring("git", -1, 1, &result_idx);

    TEST_ASSERT_NULL(match);
    TEST_ASSERT_EQUAL_INT(-1, result_idx);
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

// ============================================================================
// File format: multi-line entries and the old one-line format
// ============================================================================

#define HIST_TEST_FILE "/tmp/hash_test_history_format_12345"

void test_history_save_load_multiline(void) {
    setenv("HISTFILE", HIST_TEST_FILE, 1);
    unlink(HIST_TEST_FILE);
    history_init();
    history_add("for i in 1 2; do\necho $i\ndone");
    history_add("echo single");
    history_save();
    // Extended format: a #<seconds> line before each entry
    FILE *f = fopen(HIST_TEST_FILE, "r");
    TEST_ASSERT_NOT_NULL(f);
    char first[64] = {0};
    TEST_ASSERT_NOT_NULL(fgets(first, sizeof(first), f));
    fclose(f);
    TEST_ASSERT_EQUAL_CHAR('#', first[0]);
    TEST_ASSERT_TRUE(first[1] >= '0' && first[1] <= '9');
    TEST_ASSERT_TRUE(history_get_time(0) > 0);

    history_clear();
    history_load();
    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING("for i in 1 2; do\necho $i\ndone", history_get(0));
    TEST_ASSERT_EQUAL_STRING("echo single", history_get(1));
    TEST_ASSERT_TRUE(history_get_time(0) > 0);
    unlink(HIST_TEST_FILE);
    setenv("HISTFILE", "/tmp/hash_test_history_nonexistent_12345", 1);
}

void test_history_load_old_one_line_format(void) {
    setenv("HISTFILE", HIST_TEST_FILE, 1);
    FILE *f = fopen(HIST_TEST_FILE, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("echo old-one\necho old-two\n\necho old-three\n", f);
    fclose(f);
    history_init();
    TEST_ASSERT_EQUAL_INT(3, history_count());  // the blank line is skipped
    TEST_ASSERT_EQUAL_STRING("echo old-one", history_get(0));
    TEST_ASSERT_EQUAL_STRING("echo old-three", history_get(2));
    TEST_ASSERT_TRUE(history_get_time(0) > 0);  // old entries get the load time
    unlink(HIST_TEST_FILE);
    setenv("HISTFILE", "/tmp/hash_test_history_nonexistent_12345", 1);
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
    RUN_TEST(test_history_search_substring_basic);
    RUN_TEST(test_history_search_substring_cycle);
    RUN_TEST(test_history_search_substring_no_match);
    RUN_TEST(test_history_search_substring_middle);
    RUN_TEST(test_history_search_substring_forward);
    RUN_TEST(test_history_search_substring_empty);
    RUN_TEST(test_history_search_substring_empty_history);
    RUN_TEST(test_history_expand_last);
    RUN_TEST(test_history_expand_number);
    RUN_TEST(test_history_expand_relative);
    RUN_TEST(test_history_expand_prefix);
    RUN_TEST(test_history_expand_escaped);
    RUN_TEST(test_history_expand_none);

    RUN_TEST(test_history_save_load_multiline);
    RUN_TEST(test_history_load_old_one_line_format);
    return UNITY_END();
}
