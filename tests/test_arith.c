#include "unity.h"
#include "../src/arith.h"
#include "../src/shellvar.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
    // Initialize shell variable system
    shellvar_init();
    // Clear test variables
    unsetenv("x");
    unsetenv("y");
    unsetenv("n");
}

void tearDown(void) {
    shellvar_cleanup();
}

// Test basic addition
void test_arith_add(void) {
    long result;
    int ret = arith_evaluate("1 + 2", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(3, result);
}

// Test subtraction
void test_arith_subtract(void) {
    long result;
    int ret = arith_evaluate("5 - 3", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(2, result);
}

// Test multiplication
void test_arith_multiply(void) {
    long result;
    int ret = arith_evaluate("4 * 5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(20, result);
}

// Test division
void test_arith_divide(void) {
    long result;
    int ret = arith_evaluate("20 / 4", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(5, result);
}

// Test modulo
void test_arith_modulo(void) {
    long result;
    int ret = arith_evaluate("17 % 5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(2, result);
}

// Test parentheses
void test_arith_parens(void) {
    long result;
    int ret = arith_evaluate("(2 + 3) * 4", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(20, result);
}

// Test operator precedence
void test_arith_precedence(void) {
    long result;
    int ret = arith_evaluate("2 + 3 * 4", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(14, result);  // 3*4=12, 2+12=14
}

// Test unary minus
void test_arith_unary_minus(void) {
    long result;
    int ret = arith_evaluate("-5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(-5, result);
}

// Test variable (undefined = 0)
void test_arith_undefined_var(void) {
    long result;
    int ret = arith_evaluate("x + 1", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);  // x is undefined, so 0+1=1
}

// Test variable (defined)
void test_arith_defined_var(void) {
    shellvar_set("x", "10");
    long result;
    int ret = arith_evaluate("x + 5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(15, result);
}

// Test variable with $ prefix
void test_arith_dollar_var(void) {
    shellvar_set("n", "7");
    long result;
    int ret = arith_evaluate("$n * 2", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(14, result);
}

// Test comparison operators
void test_arith_less_than(void) {
    long result;
    int ret = arith_evaluate("3 < 5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_arith_greater_than(void) {
    long result;
    int ret = arith_evaluate("5 > 3", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_arith_equal(void) {
    long result;
    int ret = arith_evaluate("5 == 5", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_arith_not_equal(void) {
    long result;
    int ret = arith_evaluate("5 != 3", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test logical operators
void test_arith_logical_and(void) {
    long result;
    int ret = arith_evaluate("1 && 1", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_arith_logical_or(void) {
    long result;
    int ret = arith_evaluate("0 || 1", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_arith_logical_not(void) {
    long result;
    int ret = arith_evaluate("!0", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, result);
}

// Test ternary operator
void test_arith_ternary_true(void) {
    long result;
    int ret = arith_evaluate("1 ? 10 : 20", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(10, result);
}

void test_arith_ternary_false(void) {
    long result;
    int ret = arith_evaluate("0 ? 10 : 20", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(20, result);
}

// Test assignment
void test_arith_assignment(void) {
    long result;
    int ret = arith_evaluate("x = 42", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(42, result);
    TEST_ASSERT_EQUAL_STRING("42", shellvar_get("x"));
}

// Test increment
void test_arith_pre_increment(void) {
    shellvar_set("x", "5");
    long result;
    int ret = arith_evaluate("++x", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(6, result);
    TEST_ASSERT_EQUAL_STRING("6", shellvar_get("x"));
}

void test_arith_post_increment(void) {
    shellvar_set("x", "5");
    long result;
    int ret = arith_evaluate("x++", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(5, result);  // Returns old value
    TEST_ASSERT_EQUAL_STRING("6", shellvar_get("x"));  // But variable is incremented
}

// Test has_arith function
void test_has_arith_true(void) {
    TEST_ASSERT_EQUAL_INT(1, has_arith("echo $((1+2))"));
    TEST_ASSERT_EQUAL_INT(1, has_arith("$((x))"));
}

void test_has_arith_false(void) {
    TEST_ASSERT_EQUAL_INT(0, has_arith("echo hello"));
    TEST_ASSERT_EQUAL_INT(0, has_arith("$(command)"));  // Command sub, not arith
    TEST_ASSERT_EQUAL_INT(0, has_arith("$variable"));
}

// Test arith_expand
void test_arith_expand_simple(void) {
    char *result = arith_expand("Result is $((2 + 3))");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Result is 5", result);
    free(result);
}

void test_arith_expand_multiple(void) {
    char *result = arith_expand("$((1+1)) and $((2*2))");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("2 and 4", result);
    free(result);
}

void test_arith_expand_nested_parens(void) {
    char *result = arith_expand("$((((2+3))*2))");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("10", result);
    free(result);
}

// Test division by zero
void test_arith_divide_by_zero(void) {
    long result;
    int ret = arith_evaluate("5 / 0", &result);
    TEST_ASSERT_EQUAL_INT(-1, ret);  // Should fail
}

// Test complex expression (like factorial)
void test_arith_complex(void) {
    shellvar_set("n", "5");
    long result;
    int ret = arith_evaluate("n * 4", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(20, result);
}

// Test n - 1 (important for factorial)
void test_arith_n_minus_1(void) {
    shellvar_set("n", "5");
    long result;
    int ret = arith_evaluate("$n - 1", &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(4, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_arith_add);
    RUN_TEST(test_arith_subtract);
    RUN_TEST(test_arith_multiply);
    RUN_TEST(test_arith_divide);
    RUN_TEST(test_arith_modulo);
    RUN_TEST(test_arith_parens);
    RUN_TEST(test_arith_precedence);
    RUN_TEST(test_arith_unary_minus);
    RUN_TEST(test_arith_undefined_var);
    RUN_TEST(test_arith_defined_var);
    RUN_TEST(test_arith_dollar_var);
    RUN_TEST(test_arith_less_than);
    RUN_TEST(test_arith_greater_than);
    RUN_TEST(test_arith_equal);
    RUN_TEST(test_arith_not_equal);
    RUN_TEST(test_arith_logical_and);
    RUN_TEST(test_arith_logical_or);
    RUN_TEST(test_arith_logical_not);
    RUN_TEST(test_arith_ternary_true);
    RUN_TEST(test_arith_ternary_false);
    RUN_TEST(test_arith_assignment);
    RUN_TEST(test_arith_pre_increment);
    RUN_TEST(test_arith_post_increment);
    RUN_TEST(test_has_arith_true);
    RUN_TEST(test_has_arith_false);
    RUN_TEST(test_arith_expand_simple);
    RUN_TEST(test_arith_expand_multiple);
    RUN_TEST(test_arith_expand_nested_parens);
    RUN_TEST(test_arith_divide_by_zero);
    RUN_TEST(test_arith_complex);
    RUN_TEST(test_arith_n_minus_1);

    return UNITY_END();
}
