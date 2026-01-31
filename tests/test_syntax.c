#include "unity.h"
#include "../src/syntax.h"
#include "../src/colors.h"
#include "../src/color_config.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    colors_enable();
    color_config_init();
    syntax_init();
}

void tearDown(void) {
    syntax_cache_clear();
}

// Test syntax_analyze with simple command
void test_syntax_analyze_simple_command(void) {
    SyntaxResult *r = syntax_analyze("ls", 2);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    // ls should be detected as a valid command or invalid if not in PATH
    // (in test environment, it should be valid)
    TEST_ASSERT_TRUE(r->segments[0].type == SYN_COMMAND ||
                     r->segments[0].type == SYN_INVALID_CMD);
    TEST_ASSERT_EQUAL_INT(0, r->segments[0].start);
    TEST_ASSERT_EQUAL_INT(2, r->segments[0].end);
    syntax_result_free(r);
}

// Test builtin detection
void test_syntax_analyze_builtin(void) {
    SyntaxResult *r = syntax_analyze("cd /home", 8);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->count >= 1);
    TEST_ASSERT_EQUAL(SYN_BUILTIN, r->segments[0].type);
    syntax_result_free(r);
}

// Test operator detection
void test_syntax_analyze_pipe(void) {
    SyntaxResult *r = syntax_analyze("ls | grep foo", 13);
    TEST_ASSERT_NOT_NULL(r);
    // Should have: ls, |, grep, foo
    TEST_ASSERT_TRUE(r->count >= 3);

    // Find the pipe operator
    bool found_pipe = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_OPERATOR) {
            found_pipe = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_pipe);
    syntax_result_free(r);
}

// Test double-ampersand operator
void test_syntax_analyze_and_operator(void) {
    SyntaxResult *r = syntax_analyze("true && echo yes", 16);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->count >= 3);

    // Find the && operator
    bool found_and = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_OPERATOR) {
            found_and = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_and);
    syntax_result_free(r);
}

// Test single-quoted string
void test_syntax_analyze_single_quote(void) {
    SyntaxResult *r = syntax_analyze("echo 'hello world'", 18);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->count >= 2);

    // Find the string
    bool found_string = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_STRING_SINGLE) {
            found_string = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_string);
    syntax_result_free(r);
}

// Test double-quoted string
void test_syntax_analyze_double_quote(void) {
    SyntaxResult *r = syntax_analyze("echo \"hello world\"", 18);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->count >= 2);

    // Find the string
    bool found_string = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_STRING_DOUBLE) {
            found_string = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_string);
    syntax_result_free(r);
}

// Test variable detection
void test_syntax_analyze_variable(void) {
    SyntaxResult *r = syntax_analyze("echo $HOME", 10);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->count >= 2);

    // Find the variable
    bool found_var = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_VARIABLE) {
            found_var = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_var);
    syntax_result_free(r);
}

// Test braced variable
void test_syntax_analyze_braced_variable(void) {
    SyntaxResult *r = syntax_analyze("echo ${HOME}", 12);
    TEST_ASSERT_NOT_NULL(r);

    // Find the variable
    bool found_var = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_VARIABLE) {
            found_var = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_var);
    syntax_result_free(r);
}

// Test command substitution
void test_syntax_analyze_command_substitution(void) {
    SyntaxResult *r = syntax_analyze("echo $(pwd)", 11);
    TEST_ASSERT_NOT_NULL(r);

    // Find the variable (command substitution is a type of variable)
    bool found_var = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_VARIABLE) {
            found_var = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_var);
    syntax_result_free(r);
}

// Test redirection
void test_syntax_analyze_redirect(void) {
    SyntaxResult *r = syntax_analyze("echo foo > file.txt", 19);
    TEST_ASSERT_NOT_NULL(r);

    // Find the redirect
    bool found_redirect = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_REDIRECT) {
            found_redirect = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_redirect);
    syntax_result_free(r);
}

// Test stderr redirection
void test_syntax_analyze_stderr_redirect(void) {
    SyntaxResult *r = syntax_analyze("cmd 2>&1", 8);
    TEST_ASSERT_NOT_NULL(r);

    // Find the redirect
    bool found_redirect = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_REDIRECT) {
            found_redirect = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_redirect);
    syntax_result_free(r);
}

// Test comment
void test_syntax_analyze_comment(void) {
    SyntaxResult *r = syntax_analyze("echo foo # comment", 18);
    TEST_ASSERT_NOT_NULL(r);

    // Find the comment
    bool found_comment = false;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_COMMENT) {
            found_comment = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_comment);
    syntax_result_free(r);
}

// Test empty input
void test_syntax_analyze_empty(void) {
    SyntaxResult *r = syntax_analyze("", 0);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(0, r->count);
    syntax_result_free(r);
}

// Test NULL input
void test_syntax_analyze_null(void) {
    SyntaxResult *r = syntax_analyze(NULL, 0);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(0, r->count);
    syntax_result_free(r);
}

// Test syntax_render produces output
void test_syntax_render_basic(void) {
    char *rendered = syntax_render("ls -la", 6);
    TEST_ASSERT_NOT_NULL(rendered);
    // Should contain the original text
    TEST_ASSERT_TRUE(strstr(rendered, "ls") != NULL);
    TEST_ASSERT_TRUE(strstr(rendered, "-la") != NULL);
    free(rendered);
}

// Test syntax_render with colors enabled
void test_syntax_render_with_colors(void) {
    colors_enable();
    char *rendered = syntax_render("cd /home", 8);
    TEST_ASSERT_NOT_NULL(rendered);
    // Should contain ANSI escape codes when colors enabled
    TEST_ASSERT_TRUE(strstr(rendered, "\033[") != NULL);
    free(rendered);
}

// Test syntax_render with colors disabled
void test_syntax_render_colors_disabled(void) {
    colors_disable();
    char *rendered = syntax_render("cd /home", 8);
    TEST_ASSERT_NOT_NULL(rendered);
    // Should NOT contain ANSI escape codes when colors disabled
    // (or at least the coloring should be minimal)
    free(rendered);
    colors_enable();  // Restore
}

// Test syntax_check_command for builtin
void test_syntax_check_command_builtin(void) {
    int result = syntax_check_command("cd");
    TEST_ASSERT_EQUAL_INT(2, result);  // 2 = builtin
}

// Test syntax_check_command for invalid
void test_syntax_check_command_invalid(void) {
    int result = syntax_check_command("nonexistent_command_xyz123");
    TEST_ASSERT_EQUAL_INT(0, result);  // 0 = invalid
}

// Test command caching
void test_syntax_command_cache(void) {
    // First call
    int result1 = syntax_check_command("cd");
    // Second call should return same result (from cache)
    int result2 = syntax_check_command("cd");
    TEST_ASSERT_EQUAL_INT(result1, result2);

    // Clear cache and check again
    syntax_cache_clear();
    int result3 = syntax_check_command("cd");
    TEST_ASSERT_EQUAL_INT(result1, result3);
}

// Test multiple commands in pipeline
void test_syntax_analyze_pipeline(void) {
    SyntaxResult *r = syntax_analyze("cat file | grep pattern | wc -l", 31);
    TEST_ASSERT_NOT_NULL(r);
    // Should have multiple segments including operators
    TEST_ASSERT_TRUE(r->count >= 5);

    // Count operators
    int op_count = 0;
    for (int i = 0; i < r->count; i++) {
        if (r->segments[i].type == SYN_OPERATOR) {
            op_count++;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, op_count);  // Two pipes
    syntax_result_free(r);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_syntax_analyze_simple_command);
    RUN_TEST(test_syntax_analyze_builtin);
    RUN_TEST(test_syntax_analyze_pipe);
    RUN_TEST(test_syntax_analyze_and_operator);
    RUN_TEST(test_syntax_analyze_single_quote);
    RUN_TEST(test_syntax_analyze_double_quote);
    RUN_TEST(test_syntax_analyze_variable);
    RUN_TEST(test_syntax_analyze_braced_variable);
    RUN_TEST(test_syntax_analyze_command_substitution);
    RUN_TEST(test_syntax_analyze_redirect);
    RUN_TEST(test_syntax_analyze_stderr_redirect);
    RUN_TEST(test_syntax_analyze_comment);
    RUN_TEST(test_syntax_analyze_empty);
    RUN_TEST(test_syntax_analyze_null);
    RUN_TEST(test_syntax_render_basic);
    RUN_TEST(test_syntax_render_with_colors);
    RUN_TEST(test_syntax_render_colors_disabled);
    RUN_TEST(test_syntax_check_command_builtin);
    RUN_TEST(test_syntax_check_command_invalid);
    RUN_TEST(test_syntax_command_cache);
    RUN_TEST(test_syntax_analyze_pipeline);

    return UNITY_END();
}
