#include "unity.h"
#include "../src/colors.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    colors_enable();
}

void tearDown(void) {
    // Restore default
    colors_enable();
}

// Test colors can be enabled
void test_colors_enable(void) {
    colors_enable();
    TEST_ASSERT_TRUE(colors_enabled);
}

// Test colors can be disabled
void test_colors_disable(void) {
    colors_disable();
    TEST_ASSERT_FALSE(colors_enabled);
}

// Test color_code returns code when enabled
void test_color_code_when_enabled(void) {
    colors_enable();
    const char *code = color_code(COLOR_RED);
    TEST_ASSERT_EQUAL_STRING(COLOR_RED, code);
}

// Test color_code returns empty string when disabled
void test_color_code_when_disabled(void) {
    colors_disable();
    const char *code = color_code(COLOR_RED);
    TEST_ASSERT_EQUAL_STRING("", code);
}

// Test all basic colors are defined
void test_color_definitions(void) {
    TEST_ASSERT_NOT_NULL(COLOR_RED);
    TEST_ASSERT_NOT_NULL(COLOR_GREEN);
    TEST_ASSERT_NOT_NULL(COLOR_YELLOW);
    TEST_ASSERT_NOT_NULL(COLOR_BLUE);
    TEST_ASSERT_NOT_NULL(COLOR_MAGENTA);
    TEST_ASSERT_NOT_NULL(COLOR_CYAN);
    TEST_ASSERT_NOT_NULL(COLOR_WHITE);
    TEST_ASSERT_NOT_NULL(COLOR_RESET);
}

// Test bright colors are defined
void test_bright_color_definitions(void) {
    TEST_ASSERT_NOT_NULL(COLOR_BRIGHT_RED);
    TEST_ASSERT_NOT_NULL(COLOR_BRIGHT_GREEN);
    TEST_ASSERT_NOT_NULL(COLOR_BRIGHT_YELLOW);
    TEST_ASSERT_NOT_NULL(COLOR_BRIGHT_BLUE);
}

// Test text styles are defined
void test_style_definitions(void) {
    TEST_ASSERT_NOT_NULL(COLOR_BOLD);
    TEST_ASSERT_NOT_NULL(COLOR_DIM);
    TEST_ASSERT_NOT_NULL(COLOR_UNDERLINE);
}

// Test NO_COLOR environment variable
void test_no_color_env(void) {
    // Save original value
    char *original = getenv("NO_COLOR");
    
    // Set NO_COLOR
    setenv("NO_COLOR", "1", 1);
    colors_init();
    TEST_ASSERT_FALSE(colors_enabled);
    
    // Restore
    if (original) {
        setenv("NO_COLOR", original, 1);
    } else {
        unsetenv("NO_COLOR");
    }
    colors_init();
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_colors_enable);
    RUN_TEST(test_colors_disable);
    RUN_TEST(test_color_code_when_enabled);
    RUN_TEST(test_color_code_when_disabled);
    RUN_TEST(test_color_definitions);
    RUN_TEST(test_bright_color_definitions);
    RUN_TEST(test_style_definitions);
    RUN_TEST(test_no_color_env);
    
    return UNITY_END();
}
