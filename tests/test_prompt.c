#include "unity.h"
#include "../src/prompt.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void setUp(void) {
    prompt_init();
}

void tearDown(void) {
    // Clean up
}

// Test prompt initialization
void test_prompt_init(void) {
    prompt_init();
    TEST_ASSERT_FALSE(prompt_config.use_custom_ps1);
}

// Test setting custom PS1
void test_set_ps1(void) {
    prompt_set_ps1("\\u@\\h:\\w\\$ ");
    TEST_ASSERT_TRUE(prompt_config.use_custom_ps1);
}

// Test get current directory
void test_get_current_dir(void) {
    char *dir = prompt_get_current_dir();
    TEST_ASSERT_NOT_NULL(dir);
    // Should not be empty
    TEST_ASSERT_TRUE(strlen(dir) > 0);
}

// Test get username
void test_get_user(void) {
    char *user = prompt_get_user();
    TEST_ASSERT_NOT_NULL(user);
    TEST_ASSERT_TRUE(strlen(user) > 0);
}

// Test get hostname
void test_get_hostname(void) {
    char *host = prompt_get_hostname();
    TEST_ASSERT_NOT_NULL(host);
    TEST_ASSERT_TRUE(strlen(host) > 0);
}

// Test prompt generation
void test_prompt_generate(void) {
    char *prompt = prompt_generate(0);
    TEST_ASSERT_NOT_NULL(prompt);
    TEST_ASSERT_TRUE(strlen(prompt) > 0);
}

// Test prompt with success exit code
void test_prompt_success_exit(void) {
    char *prompt = prompt_generate(0);
    TEST_ASSERT_NOT_NULL(prompt);
    // Should contain prompt characters
    TEST_ASSERT_TRUE(strlen(prompt) > 0);
}

// Test prompt with failure exit code
void test_prompt_failure_exit(void) {
    char *prompt = prompt_generate(1);
    TEST_ASSERT_NOT_NULL(prompt);
    // Should contain prompt characters
    TEST_ASSERT_TRUE(strlen(prompt) > 0);
}

// Test git branch detection (may return NULL if not in repo)
void test_git_branch(void) {
    char *branch = prompt_git_branch();
    // May be NULL if not in a git repo, which is fine
    if (branch != NULL) {
        TEST_ASSERT_TRUE(strlen(branch) > 0);
    }
}

// Test git dirty check
void test_git_dirty(void) {
    // Just verify it doesn't crash
    bool dirty = prompt_git_dirty();
    // Result depends on repo state, just check it returns
    (void)dirty;  // Suppress unused warning
    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_prompt_init);
    RUN_TEST(test_set_ps1);
    RUN_TEST(test_get_current_dir);
    RUN_TEST(test_get_user);
    RUN_TEST(test_get_hostname);
    RUN_TEST(test_prompt_generate);
    RUN_TEST(test_prompt_success_exit);
    RUN_TEST(test_prompt_failure_exit);
    RUN_TEST(test_git_branch);
    RUN_TEST(test_git_dirty);

    return UNITY_END();
}
