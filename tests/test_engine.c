#include "unity.h"
#include "../src/engine.h"
#include <stdlib.h>

void setUp(void) {
    unsetenv("HASH_ENGINE");
    engine_init();
}

void tearDown(void) {
    unsetenv("HASH_ENGINE");
}

void test_default_is_legacy(void) {
    TEST_ASSERT_EQUAL_INT(ENGINE_LEGACY, engine_current());
    TEST_ASSERT_EQUAL_STRING("legacy", engine_name());
    TEST_ASSERT_FALSE(engine_is_ast());
}

void test_environment_selects_ast(void) {
    setenv("HASH_ENGINE", "ast", 1);
    engine_init();
    TEST_ASSERT_TRUE(engine_is_ast());
    TEST_ASSERT_EQUAL_STRING("ast", engine_name());
}

void test_unknown_environment_value_keeps_default(void) {
    setenv("HASH_ENGINE", "turbo", 1);
    engine_init();
    TEST_ASSERT_EQUAL_INT(ENGINE_LEGACY, engine_current());
}

void test_set_by_name(void) {
    TEST_ASSERT_TRUE(engine_set_by_name("ast"));
    TEST_ASSERT_TRUE(engine_is_ast());
    TEST_ASSERT_TRUE(engine_set_by_name("legacy"));
    TEST_ASSERT_FALSE(engine_is_ast());
    TEST_ASSERT_FALSE(engine_set_by_name("nope"));
    TEST_ASSERT_FALSE(engine_set_by_name(NULL));
    TEST_ASSERT_FALSE(engine_is_ast());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_legacy);
    RUN_TEST(test_environment_selects_ast);
    RUN_TEST(test_unknown_environment_value_keeps_default);
    RUN_TEST(test_set_by_name);
    return UNITY_END();
}
