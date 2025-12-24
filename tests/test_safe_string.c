#include "unity.h"
#include "../src/safe_string.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

// Test safe_strcpy basic usage
void test_safe_strcpy_basic(void) {
    char dst[20];
    size_t result = safe_strcpy(dst, "hello", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello", dst);
    TEST_ASSERT_EQUAL_size_t(5, result);
}

// Test safe_strcpy truncation
void test_safe_strcpy_truncation(void) {
    char dst[6];
    size_t result = safe_strcpy(dst, "hello world", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello", dst);
    TEST_ASSERT_EQUAL_size_t(11, result);  // Returns full source length
    TEST_ASSERT_EQUAL_CHAR('\0', dst[5]);  // Null-terminated
}

// Test safe_strcpy null termination
void test_safe_strcpy_always_null_terminates(void) {
    char dst[5];
    safe_strcpy(dst, "1234567890", sizeof(dst));

    TEST_ASSERT_EQUAL_CHAR('\0', dst[4]);
    TEST_ASSERT_EQUAL_STRING("1234", dst);
}

// Test safe_strcpy with empty string
void test_safe_strcpy_empty(void) {
    char dst[10] = "test";
    safe_strcpy(dst, "", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("", dst);
}

// Test safe_strcpy with size 1 (only room for null)
void test_safe_strcpy_size_one(void) {
    char dst[1];
    safe_strcpy(dst, "hello", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("", dst);
}

// Test safe_strlen basic usage
void test_safe_strlen_basic(void) {
    const char *str = "hello";
    size_t len = safe_strlen(str, 100);

    TEST_ASSERT_EQUAL_size_t(5, len);
}

// Test safe_strlen with limit
void test_safe_strlen_limited(void) {
    const char *str = "hello world";
    size_t len = safe_strlen(str, 5);

    TEST_ASSERT_EQUAL_size_t(5, len);
}

// Test safe_strlen with null
void test_safe_strlen_null(void) {
    size_t len = safe_strlen(NULL, 100);
    TEST_ASSERT_EQUAL_size_t(0, len);
}

// Test safe_strcat basic usage
void test_safe_strcat_basic(void) {
    char dst[20] = "hello";
    safe_strcat(dst, " world", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello world", dst);
}

// Test safe_strcat truncation
void test_safe_strcat_truncation(void) {
    char dst[10] = "hello";
    safe_strcat(dst, " world", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello wor", dst);
    TEST_ASSERT_EQUAL_CHAR('\0', dst[9]);
}

// Test safe_strcmp basic usage
void test_safe_strcmp_equal(void) {
    int result = safe_strcmp("hello", "hello", 10);
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test safe_strcmp different
void test_safe_strcmp_different(void) {
    int result = safe_strcmp("hello", "world", 10);
    TEST_ASSERT_NOT_EQUAL(0, result);
}

// Test safe_strcmp with limit
void test_safe_strcmp_limited(void) {
    int result = safe_strcmp("hello", "help", 3);
    TEST_ASSERT_EQUAL_INT(0, result);  // First 3 chars match
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_safe_strcpy_basic);
    RUN_TEST(test_safe_strcpy_truncation);
    RUN_TEST(test_safe_strcpy_always_null_terminates);
    RUN_TEST(test_safe_strcpy_empty);
    RUN_TEST(test_safe_strcpy_size_one);
    RUN_TEST(test_safe_strlen_basic);
    RUN_TEST(test_safe_strlen_limited);
    RUN_TEST(test_safe_strlen_null);
    RUN_TEST(test_safe_strcat_basic);
    RUN_TEST(test_safe_strcat_truncation);
    RUN_TEST(test_safe_strcmp_equal);
    RUN_TEST(test_safe_strcmp_different);
    RUN_TEST(test_safe_strcmp_limited);

    return UNITY_END();
}
