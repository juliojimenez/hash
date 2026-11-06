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
    // In some VM environments (like FreeBSD CI), hostname may be empty
    // Just verify it doesn't crash and returns non-NULL
    TEST_ASSERT_NOT_NULL(host);
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

// Presets and the newer escapes (#333)
void test_prompt_presets(void) {
    size_t count = 0;
    const PromptPreset *all = prompt_presets(&count);
    TEST_ASSERT_EQUAL_INT(5, (int)count);
    TEST_ASSERT_EQUAL_STRING("classic", all[0].name);
    TEST_ASSERT_TRUE(prompt_set_preset("minimal"));
    TEST_ASSERT_EQUAL_STRING("minimal", prompt_current_preset());
    TEST_ASSERT_EQUAL_STRING(all[2].ps1, prompt_config.ps1);
    TEST_ASSERT_NULL(getenv("PS1"));                    // a preset drops an inherited PS1
    TEST_ASSERT_FALSE(prompt_set_preset("nosuch"));
    TEST_ASSERT_EQUAL_STRING("minimal", prompt_current_preset());
    prompt_set_ps1("\\w $ ");
    TEST_ASSERT_NULL(prompt_current_preset());          // a hand-made PS1 is no preset
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT_TRUE(strlen(prompt_render(all[i].ps1, 0)) > 0);
        TEST_ASSERT_TRUE(strlen(all[i].blurb) > 10);
    }
}

void test_prompt_escapes_status_time_host(void) {
    const char *r = prompt_render("[\\?]", 0);
    TEST_ASSERT_NOT_NULL(strstr(r, "[]"));              // \? is empty on success
    r = prompt_render("[\\?]", 3);
    TEST_ASSERT_NOT_NULL(strstr(r, "[3]"));
    r = prompt_render("\\t", 0);
    const char *stamp = strchr(r, ':');                 // HH:MM:SS somewhere in the wrapped text
    TEST_ASSERT_NOT_NULL(stamp);
    TEST_ASSERT_TRUE(stamp - r >= 2 && stamp[3] == ':');
    r = prompt_render("\\s \\v", 0);
    TEST_ASSERT_NOT_NULL(strstr(r, "hash "));
    r = prompt_render("\\[x\\]", 0);
    TEST_ASSERT_NULL(strstr(r, "[x]"));                 // bash's markers are dropped (colour codes contain [ too)
    TEST_ASSERT_NOT_NULL(strstr(r, "x"));
    r = prompt_render("\\h", 0);
    TEST_ASSERT_NULL(strchr(r, '.'));                   // \h stops at the first dot
    char full[256];
    gethostname(full, sizeof(full));
    if (strchr(full, '.')) {
        TEST_ASSERT_NOT_NULL(strstr(prompt_render("\\H", 0), full));
    }
    r = prompt_render("\\j", 0);
    TEST_ASSERT_NOT_NULL(strstr(r, "0"));
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

    RUN_TEST(test_prompt_presets);
    RUN_TEST(test_prompt_escapes_status_time_host);
    return UNITY_END();
}
