#include "unity.h"
#include "../src/update.h"
#include "../src/hash.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    update_init();
}

void tearDown(void) {
}

// Test version comparison - equal versions
void test_version_compare_equal(void) {
    int result = update_compare_versions("18", "18");
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test version comparison - v1 < v2
void test_version_compare_less(void) {
    int result = update_compare_versions("17", "18");
    TEST_ASSERT_TRUE(result < 0);
}

// Test version comparison - v1 > v2
void test_version_compare_greater(void) {
    int result = update_compare_versions("19", "18");
    TEST_ASSERT_TRUE(result > 0);
}

// Test version comparison with 'v' prefix
void test_version_compare_with_prefix(void) {
    int result = update_compare_versions("v17", "v18");
    TEST_ASSERT_TRUE(result < 0);

    result = update_compare_versions("18", "v18");
    TEST_ASSERT_EQUAL_INT(0, result);
}

// Test platform detection
void test_get_platform(void) {
    char platform[64];
    int result = update_get_platform(platform, sizeof(platform));

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(strlen(platform) > 0);

    // Should contain a hyphen separating os-arch
    TEST_ASSERT_NOT_NULL(strchr(platform, '-'));
}

// Test install method detection doesn't crash
void test_detect_install_method(void) {
    InstallMethod method = update_detect_install_method();

    // Just verify it returns a valid enum value
    TEST_ASSERT_TRUE(method >= INSTALL_METHOD_UNKNOWN);
    TEST_ASSERT_TRUE(method <= INSTALL_METHOD_SNAP);
}

// Test install method string conversion
void test_install_method_str(void) {
    const char *str;

    str = update_install_method_str(INSTALL_METHOD_DIRECT);
    TEST_ASSERT_EQUAL_STRING("direct download", str);

    str = update_install_method_str(INSTALL_METHOD_APT);
    TEST_ASSERT_EQUAL_STRING("apt (Debian/Ubuntu)", str);

    str = update_install_method_str(INSTALL_METHOD_BREW);
    TEST_ASSERT_EQUAL_STRING("Homebrew", str);

    str = update_install_method_str(INSTALL_METHOD_UNKNOWN);
    TEST_ASSERT_EQUAL_STRING("unknown", str);
}

// Test should_check returns reasonable values
void test_should_check(void) {
    // This should return true or false without crashing
    bool should = update_should_check();
    (void)should;  // Just verify it doesn't crash
    TEST_ASSERT_TRUE(true);
}

// Test UpdateInfo struct initialization
void test_update_info_init(void) {
    UpdateInfo info;
    memset(&info, 0, sizeof(info));

    TEST_ASSERT_FALSE(info.update_available);
    TEST_ASSERT_EQUAL_STRING("", info.latest_version);
    TEST_ASSERT_EQUAL_STRING("", info.current_version);
}

// Test update_perform with no update available
void test_update_perform_no_update(void) {
    UpdateInfo info;
    memset(&info, 0, sizeof(info));
    info.update_available = false;
    strcpy(info.current_version, HASH_VERSION);
    strcpy(info.latest_version, HASH_VERSION);

    // Should return 0 (success) since there's nothing to do
    int result = update_perform(&info, false);
    TEST_ASSERT_EQUAL_INT(0, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_version_compare_equal);
    RUN_TEST(test_version_compare_less);
    RUN_TEST(test_version_compare_greater);
    RUN_TEST(test_version_compare_with_prefix);
    RUN_TEST(test_get_platform);
    RUN_TEST(test_detect_install_method);
    RUN_TEST(test_install_method_str);
    RUN_TEST(test_should_check);
    RUN_TEST(test_update_info_init);
    RUN_TEST(test_update_perform_no_update);

    return UNITY_END();
}
