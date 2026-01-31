#include "unity.h"
#include "../src/color_config.h"
#include "../src/colors.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    colors_enable();
    color_config_init();
}

void tearDown(void) {
    // Restore defaults
    colors_enable();
    color_config_init();
    // Clear any test env vars
    unsetenv("HASH_COLOR_PROMPT");
    unsetenv("HASH_COLOR_COMMAND");
    unsetenv("HASH_SYNTAX_HIGHLIGHT");
    unsetenv("HASH_AUTOSUGGEST");
    unsetenv("HASH_DANGER_HIGHLIGHT");
}

// Test color_config_init sets default prompt to bold
void test_color_config_init_prompt_bold(void) {
    color_config_init();
    TEST_ASSERT_TRUE(strstr(color_config.prompt, COLOR_BOLD) != NULL);
}

// Test color_config_init sets default path color
void test_color_config_init_path_color(void) {
    color_config_init();
    TEST_ASSERT_TRUE(strstr(color_config.prompt_path, COLOR_BOLD) != NULL);
    TEST_ASSERT_TRUE(strstr(color_config.prompt_path, COLOR_BLUE) != NULL);
}

// Test color_config_init sets git colors
void test_color_config_init_git_colors(void) {
    color_config_init();
    TEST_ASSERT_TRUE(strstr(color_config.prompt_git_clean, COLOR_GREEN) != NULL);
    TEST_ASSERT_TRUE(strstr(color_config.prompt_git_dirty, COLOR_YELLOW) != NULL);
    TEST_ASSERT_TRUE(strstr(color_config.prompt_git_branch, COLOR_CYAN) != NULL);
}

// Test color_config_init enables all features by default
void test_color_config_init_features_enabled(void) {
    color_config_init();
    TEST_ASSERT_TRUE(color_config.syntax_highlight_enabled);
    TEST_ASSERT_TRUE(color_config.autosuggestion_enabled);
    TEST_ASSERT_TRUE(color_config.danger_highlight_enabled);
}

// Test color_config_parse with single color
void test_color_config_parse_single(void) {
    char buf[32];
    int result = color_config_parse("red", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(COLOR_RED, buf);
}

// Test color_config_parse with combined colors
void test_color_config_parse_combined(void) {
    char buf[64];
    int result = color_config_parse("bold,red", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(strstr(buf, "\033[1m") != NULL);  // bold
    TEST_ASSERT_TRUE(strstr(buf, "\033[31m") != NULL); // red
}

// Test color_config_parse with bright color
void test_color_config_parse_bright(void) {
    char buf[32];
    int result = color_config_parse("bright_blue", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(COLOR_BRIGHT_BLUE, buf);
}

// Test color_config_parse with background color
void test_color_config_parse_background(void) {
    char buf[32];
    int result = color_config_parse("bg_red", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(COLOR_BG_RED, buf);
}

// Test color_config_parse with invalid color name
void test_color_config_parse_invalid(void) {
    char buf[32];
    int result = color_config_parse("notacolor", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(-1, result);
}

// Test color_config_parse with NULL input
void test_color_config_parse_null(void) {
    char buf[32];
    int result = color_config_parse(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(-1, result);
}

// Test color_config_set changes the config
void test_color_config_set(void) {
    int result = color_config_set("prompt", "green");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(COLOR_GREEN, color_config.prompt);
}

// Test color_config_set with combined value
void test_color_config_set_combined(void) {
    int result = color_config_set("path", "bold,cyan");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(strstr(color_config.prompt_path, COLOR_BOLD) != NULL);
    TEST_ASSERT_TRUE(strstr(color_config.prompt_path, COLOR_CYAN) != NULL);
}

// Test color_config_set with invalid element
void test_color_config_set_invalid_element(void) {
    int result = color_config_set("notanelement", "red");
    TEST_ASSERT_EQUAL_INT(-1, result);
}

// Test color_config_get returns color when enabled
void test_color_config_get_enabled(void) {
    colors_enable();
    color_config_init();
    const char *color = color_config_get(color_config.prompt);
    TEST_ASSERT_NOT_NULL(color);
    TEST_ASSERT_TRUE(strlen(color) > 0);
}

// Test color_config_get returns empty string when disabled
void test_color_config_get_disabled(void) {
    colors_disable();
    const char *color = color_config_get(color_config.prompt);
    TEST_ASSERT_EQUAL_STRING("", color);
}

// Test color_config_load_env loads from environment
void test_color_config_load_env(void) {
    color_config_init();
    setenv("HASH_COLOR_PROMPT", "magenta", 1);
    color_config_load_env();
    TEST_ASSERT_EQUAL_STRING(COLOR_MAGENTA, color_config.prompt);
}

// Test color_config_load_env loads combined colors
void test_color_config_load_env_combined(void) {
    color_config_init();
    setenv("HASH_COLOR_COMMAND", "bold,green", 1);
    color_config_load_env();
    TEST_ASSERT_TRUE(strstr(color_config.syn_command, COLOR_BOLD) != NULL);
    TEST_ASSERT_TRUE(strstr(color_config.syn_command, COLOR_GREEN) != NULL);
    unsetenv("HASH_COLOR_COMMAND");
}

// Test feature toggle env vars
void test_color_config_load_env_feature_toggles(void) {
    color_config_init();
    setenv("HASH_SYNTAX_HIGHLIGHT", "0", 1);
    setenv("HASH_AUTOSUGGEST", "off", 1);
    color_config_load_env();
    TEST_ASSERT_FALSE(color_config.syntax_highlight_enabled);
    TEST_ASSERT_FALSE(color_config.autosuggestion_enabled);
}

// Test feature toggle with "on" value
void test_color_config_load_env_feature_on(void) {
    color_config_init();
    color_config.syntax_highlight_enabled = false;
    setenv("HASH_SYNTAX_HIGHLIGHT", "on", 1);
    color_config_load_env();
    TEST_ASSERT_TRUE(color_config.syntax_highlight_enabled);
}

// Test whitespace trimming in parse
void test_color_config_parse_whitespace(void) {
    char buf[64];
    int result = color_config_parse("bold, red", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(strstr(buf, COLOR_BOLD) != NULL);
    TEST_ASSERT_TRUE(strstr(buf, COLOR_RED) != NULL);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_color_config_init_prompt_bold);
    RUN_TEST(test_color_config_init_path_color);
    RUN_TEST(test_color_config_init_git_colors);
    RUN_TEST(test_color_config_init_features_enabled);
    RUN_TEST(test_color_config_parse_single);
    RUN_TEST(test_color_config_parse_combined);
    RUN_TEST(test_color_config_parse_bright);
    RUN_TEST(test_color_config_parse_background);
    RUN_TEST(test_color_config_parse_invalid);
    RUN_TEST(test_color_config_parse_null);
    RUN_TEST(test_color_config_set);
    RUN_TEST(test_color_config_set_combined);
    RUN_TEST(test_color_config_set_invalid_element);
    RUN_TEST(test_color_config_get_enabled);
    RUN_TEST(test_color_config_get_disabled);
    RUN_TEST(test_color_config_load_env);
    RUN_TEST(test_color_config_load_env_combined);
    RUN_TEST(test_color_config_load_env_feature_toggles);
    RUN_TEST(test_color_config_load_env_feature_on);
    RUN_TEST(test_color_config_parse_whitespace);

    return UNITY_END();
}
