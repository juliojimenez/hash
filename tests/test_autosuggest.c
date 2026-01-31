#include "unity.h"
#include "../src/autosuggest.h"
#include "../src/history.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void setUp(void) {
    // Use a non-existent file for tests
    setenv("HISTFILE", "/tmp/hash_test_autosuggest_nonexistent", 1);
    setenv("HISTSIZE", "100", 1);
    setenv("HISTFILESIZE", "200", 1);
    unsetenv("HISTCONTROL");

    history_init();
    autosuggest_invalidate();
}

void tearDown(void) {
    history_clear();
    autosuggest_invalidate();
    unlink("/tmp/hash_test_autosuggest_nonexistent");
    unsetenv("HISTFILE");
    unsetenv("HISTSIZE");
    unsetenv("HISTFILESIZE");
}

// Test basic suggestion from history
void test_autosuggest_basic(void) {
    history_add("ls -la /tmp");

    const char *suggestion = autosuggest_get("ls", 2);
    TEST_ASSERT_NOT_NULL(suggestion);
    TEST_ASSERT_EQUAL_STRING(" -la /tmp", suggestion);
}

// Test no suggestion when no history
void test_autosuggest_no_history(void) {
    const char *suggestion = autosuggest_get("ls", 2);
    TEST_ASSERT_NULL(suggestion);
}

// Test no suggestion when input doesn't match
void test_autosuggest_no_match(void) {
    history_add("ls -la");

    const char *suggestion = autosuggest_get("cd", 2);
    TEST_ASSERT_NULL(suggestion);
}

// Test exact match returns NULL (nothing to suggest)
void test_autosuggest_exact_match(void) {
    history_add("ls");

    const char *suggestion = autosuggest_get("ls", 2);
    TEST_ASSERT_NULL(suggestion);
}

// Test suggestion with longer prefix
void test_autosuggest_longer_prefix(void) {
    history_add("git commit -m 'message'");

    const char *suggestion = autosuggest_get("git co", 6);
    TEST_ASSERT_NOT_NULL(suggestion);
    TEST_ASSERT_EQUAL_STRING("mmit -m 'message'", suggestion);
}

// Test suggestion caching - same prefix should return same result
void test_autosuggest_caching(void) {
    history_add("echo hello world");

    const char *suggestion1 = autosuggest_get("echo", 4);
    TEST_ASSERT_NOT_NULL(suggestion1);

    const char *suggestion2 = autosuggest_get("echo", 4);
    TEST_ASSERT_NOT_NULL(suggestion2);
    TEST_ASSERT_EQUAL_STRING(suggestion1, suggestion2);
}

// Test invalidate clears cache
void test_autosuggest_invalidate(void) {
    history_add("test command");

    const char *suggestion1 = autosuggest_get("test", 4);
    TEST_ASSERT_NOT_NULL(suggestion1);

    autosuggest_invalidate();

    // After invalidate, should still find the suggestion (from history)
    const char *suggestion2 = autosuggest_get("test", 4);
    TEST_ASSERT_NOT_NULL(suggestion2);
}

// Test empty input returns NULL
void test_autosuggest_empty_input(void) {
    history_add("ls -la");

    const char *suggestion = autosuggest_get("", 0);
    TEST_ASSERT_NULL(suggestion);
}

// Test NULL input returns NULL
void test_autosuggest_null_input(void) {
    const char *suggestion = autosuggest_get(NULL, 0);
    TEST_ASSERT_NULL(suggestion);
}

// Test suggestion with multiple history entries (should get most recent match)
void test_autosuggest_most_recent(void) {
    history_add("ls -la");
    history_add("ls -lh");

    const char *suggestion = autosuggest_get("ls", 2);
    TEST_ASSERT_NOT_NULL(suggestion);
    // history_search_prefix searches backwards, so should find most recent
    TEST_ASSERT_EQUAL_STRING(" -lh", suggestion);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_autosuggest_basic);
    RUN_TEST(test_autosuggest_no_history);
    RUN_TEST(test_autosuggest_no_match);
    RUN_TEST(test_autosuggest_exact_match);
    RUN_TEST(test_autosuggest_longer_prefix);
    RUN_TEST(test_autosuggest_caching);
    RUN_TEST(test_autosuggest_invalidate);
    RUN_TEST(test_autosuggest_empty_input);
    RUN_TEST(test_autosuggest_null_input);
    RUN_TEST(test_autosuggest_most_recent);

    return UNITY_END();
}
