#include "unity.h"
#include "../src/script.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void setUp(void) {
    script_init();
}

void tearDown(void) {
    script_cleanup();
}

// ============================================================================
// Keyword Detection Tests
// ============================================================================

void test_is_keyword_if(void) {
    TEST_ASSERT_TRUE(script_is_keyword("if"));
}

void test_is_keyword_then(void) {
    TEST_ASSERT_TRUE(script_is_keyword("then"));
}

void test_is_keyword_else(void) {
    TEST_ASSERT_TRUE(script_is_keyword("else"));
}

void test_is_keyword_fi(void) {
    TEST_ASSERT_TRUE(script_is_keyword("fi"));
}

void test_is_keyword_for(void) {
    TEST_ASSERT_TRUE(script_is_keyword("for"));
}

void test_is_keyword_while(void) {
    TEST_ASSERT_TRUE(script_is_keyword("while"));
}

void test_is_keyword_do(void) {
    TEST_ASSERT_TRUE(script_is_keyword("do"));
}

void test_is_keyword_done(void) {
    TEST_ASSERT_TRUE(script_is_keyword("done"));
}

void test_is_keyword_case(void) {
    TEST_ASSERT_TRUE(script_is_keyword("case"));
}

void test_is_keyword_esac(void) {
    TEST_ASSERT_TRUE(script_is_keyword("esac"));
}

void test_is_keyword_in(void) {
    TEST_ASSERT_TRUE(script_is_keyword("in"));
}

void test_is_not_keyword(void) {
    TEST_ASSERT_FALSE(script_is_keyword("echo"));
    TEST_ASSERT_FALSE(script_is_keyword("ls"));
    TEST_ASSERT_FALSE(script_is_keyword("hello"));
}

void test_get_keyword_type(void) {
    TEST_ASSERT_EQUAL(TOK_IF, script_get_keyword_type("if"));
    TEST_ASSERT_EQUAL(TOK_THEN, script_get_keyword_type("then"));
    TEST_ASSERT_EQUAL(TOK_ELSE, script_get_keyword_type("else"));
    TEST_ASSERT_EQUAL(TOK_FI, script_get_keyword_type("fi"));
    TEST_ASSERT_EQUAL(TOK_FOR, script_get_keyword_type("for"));
    TEST_ASSERT_EQUAL(TOK_WHILE, script_get_keyword_type("while"));
    TEST_ASSERT_EQUAL(TOK_DO, script_get_keyword_type("do"));
    TEST_ASSERT_EQUAL(TOK_DONE, script_get_keyword_type("done"));
    TEST_ASSERT_EQUAL(TOK_CASE, script_get_keyword_type("case"));
    TEST_ASSERT_EQUAL(TOK_ESAC, script_get_keyword_type("esac"));
    TEST_ASSERT_EQUAL(TOK_IN, script_get_keyword_type("in"));
    TEST_ASSERT_EQUAL(TOK_WORD, script_get_keyword_type("echo"));
}

// ============================================================================
// Line Classification Tests
// ============================================================================

void test_classify_empty_line(void) {
    TEST_ASSERT_EQUAL(LINE_EMPTY, script_classify_line(""));
    TEST_ASSERT_EQUAL(LINE_EMPTY, script_classify_line("   "));
    TEST_ASSERT_EQUAL(LINE_EMPTY, script_classify_line("\t"));
}

void test_classify_comment(void) {
    TEST_ASSERT_EQUAL(LINE_EMPTY, script_classify_line("# comment"));
    TEST_ASSERT_EQUAL(LINE_EMPTY, script_classify_line("   # indented comment"));
}

void test_classify_if(void) {
    TEST_ASSERT_EQUAL(LINE_IF_START, script_classify_line("if [ -f file ]; then"));
    TEST_ASSERT_EQUAL(LINE_IF_START, script_classify_line("if test -f file"));
}

void test_classify_then(void) {
    TEST_ASSERT_EQUAL(LINE_THEN, script_classify_line("then"));
    TEST_ASSERT_EQUAL(LINE_THEN, script_classify_line("   then"));
}

void test_classify_elif(void) {
    TEST_ASSERT_EQUAL(LINE_ELIF, script_classify_line("elif [ condition ]"));
}

void test_classify_else(void) {
    TEST_ASSERT_EQUAL(LINE_ELSE, script_classify_line("else"));
}

void test_classify_fi(void) {
    TEST_ASSERT_EQUAL(LINE_FI, script_classify_line("fi"));
}

void test_classify_for(void) {
    TEST_ASSERT_EQUAL(LINE_FOR_START, script_classify_line("for i in 1 2 3; do"));
    TEST_ASSERT_EQUAL(LINE_FOR_START, script_classify_line("for var in list"));
}

void test_classify_while(void) {
    TEST_ASSERT_EQUAL(LINE_WHILE_START, script_classify_line("while [ condition ]"));
}

void test_classify_do(void) {
    TEST_ASSERT_EQUAL(LINE_DO, script_classify_line("do"));
}

void test_classify_done(void) {
    TEST_ASSERT_EQUAL(LINE_DONE, script_classify_line("done"));
}

void test_classify_case(void) {
    TEST_ASSERT_EQUAL(LINE_CASE_START, script_classify_line("case x in"));
    TEST_ASSERT_EQUAL(LINE_CASE_START, script_classify_line("case $var in"));
    TEST_ASSERT_EQUAL(LINE_CASE_START, script_classify_line("   case word in"));
}

void test_classify_esac(void) {
    TEST_ASSERT_EQUAL(LINE_ESAC, script_classify_line("esac"));
    TEST_ASSERT_EQUAL(LINE_ESAC, script_classify_line("   esac"));
}

void test_classify_simple(void) {
    TEST_ASSERT_EQUAL(LINE_SIMPLE, script_classify_line("echo hello"));
    TEST_ASSERT_EQUAL(LINE_SIMPLE, script_classify_line("ls -la"));
    TEST_ASSERT_EQUAL(LINE_SIMPLE, script_classify_line("cat file.txt"));
}

// ============================================================================
// Context Stack Tests
// ============================================================================

void test_initial_context(void) {
    TEST_ASSERT_FALSE(script_in_control_structure());
    TEST_ASSERT_EQUAL(CTX_NONE, script_current_context());
}

void test_push_context(void) {
    TEST_ASSERT_EQUAL(1, script_push_context(CTX_IF));  // 1 = success, continue
    TEST_ASSERT_TRUE(script_in_control_structure());
    TEST_ASSERT_EQUAL(CTX_IF, script_current_context());
}

void test_pop_context(void) {
    script_push_context(CTX_IF);
    TEST_ASSERT_EQUAL(1, script_pop_context());  // 1 = success, continue
    TEST_ASSERT_FALSE(script_in_control_structure());
    TEST_ASSERT_EQUAL(CTX_NONE, script_current_context());
}

void test_nested_context(void) {
    script_push_context(CTX_IF);
    script_push_context(CTX_FOR);

    TEST_ASSERT_EQUAL(CTX_FOR, script_current_context());

    script_pop_context();
    TEST_ASSERT_EQUAL(CTX_IF, script_current_context());

    script_pop_context();
    TEST_ASSERT_EQUAL(CTX_NONE, script_current_context());
}

void test_pop_empty_context(void) {
    // Should fail gracefully
    TEST_ASSERT_EQUAL(-1, script_pop_context());
}

void test_should_execute_default(void) {
    TEST_ASSERT_TRUE(script_should_execute());
}

void test_count_loops_at_current_depth_empty(void) {
    // No loops at all
    TEST_ASSERT_EQUAL(0, script_count_loops_at_current_depth());
}

void test_count_loops_at_current_depth_one_loop(void) {
    // Push a while loop context
    script_push_context(CTX_WHILE);
    TEST_ASSERT_EQUAL(1, script_count_loops_at_current_depth());
    script_pop_context();
}

void test_count_loops_at_current_depth_multiple_loops(void) {
    // Push multiple loop contexts
    script_push_context(CTX_FOR);
    script_push_context(CTX_WHILE);
    script_push_context(CTX_UNTIL);
    TEST_ASSERT_EQUAL(3, script_count_loops_at_current_depth());
    script_pop_context();
    script_pop_context();
    script_pop_context();
}

void test_count_loops_at_current_depth_with_if(void) {
    // Non-loop contexts shouldn't be counted
    script_push_context(CTX_IF);
    script_push_context(CTX_WHILE);
    TEST_ASSERT_EQUAL(1, script_count_loops_at_current_depth());
    script_pop_context();
    script_pop_context();
}

void test_push_case_context(void) {
    TEST_ASSERT_EQUAL(1, script_push_context(CTX_CASE));  // 1 = success, continue
    TEST_ASSERT_TRUE(script_in_control_structure());
    TEST_ASSERT_EQUAL(CTX_CASE, script_current_context());
    script_pop_context();
}

void test_count_loops_with_case(void) {
    // Case contexts shouldn't be counted as loops
    script_push_context(CTX_CASE);
    script_push_context(CTX_FOR);
    TEST_ASSERT_EQUAL(1, script_count_loops_at_current_depth());
    script_pop_context();
    script_pop_context();
}

// ============================================================================
// Break/Continue Pending Tests (POSIX dynamic scoping)
// ============================================================================

void test_break_pending_initial(void) {
    // Initially, no break is pending
    TEST_ASSERT_EQUAL(0, script_get_break_pending());
}

void test_continue_pending_initial(void) {
    // Initially, no continue is pending
    TEST_ASSERT_EQUAL(0, script_get_continue_pending());
}

void test_set_break_pending(void) {
    script_set_break_pending(2);
    TEST_ASSERT_EQUAL(2, script_get_break_pending());
    // Clean up
    script_clear_break_continue();
    TEST_ASSERT_EQUAL(0, script_get_break_pending());
}

void test_set_continue_pending(void) {
    script_set_continue_pending(3);
    TEST_ASSERT_EQUAL(3, script_get_continue_pending());
    // Clean up
    script_clear_break_continue();
    TEST_ASSERT_EQUAL(0, script_get_continue_pending());
}

void test_clear_break_continue(void) {
    script_set_break_pending(1);
    script_set_continue_pending(2);
    TEST_ASSERT_EQUAL(1, script_get_break_pending());
    TEST_ASSERT_EQUAL(2, script_get_continue_pending());

    script_clear_break_continue();
    TEST_ASSERT_EQUAL(0, script_get_break_pending());
    TEST_ASSERT_EQUAL(0, script_get_continue_pending());
}

void test_count_loops_lexical_scoping(void) {
    // By default, use lexical scoping for break/continue
    // break/continue inside a function should NOT affect the caller's loops
    script_push_context(CTX_FOR);  // This loop is at function_call_depth 0

    // Simulate entering a function (increment function_call_depth)
    script_state.function_call_depth++;

    // Loop was pushed at depth 0, but we're now at depth 1
    // With lexical scoping (default), only loops at current depth should be counted
    TEST_ASSERT_EQUAL(0, script_count_loops_at_current_depth());

    // Restore
    script_state.function_call_depth--;
    script_pop_context();
}

// ============================================================================
// Function Management Tests
// ============================================================================

void test_define_function(void) {
    TEST_ASSERT_EQUAL(0, script_define_function("hello", "echo Hello"));

    ShellFunction *func = script_get_function("hello");
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_EQUAL_STRING("hello", func->name);
    TEST_ASSERT_EQUAL_STRING("echo Hello", func->body);
}

void test_get_undefined_function(void) {
    ShellFunction *func = script_get_function("undefined");
    TEST_ASSERT_NULL(func);
}

void test_redefine_function(void) {
    script_define_function("greet", "echo Hi");
    script_define_function("greet", "echo Hello");

    ShellFunction *func = script_get_function("greet");
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_EQUAL_STRING("echo Hello", func->body);
}

// ============================================================================
// Script Processing Tests
// ============================================================================

void test_process_empty_line(void) {
    TEST_ASSERT_EQUAL(1, script_process_line(""));  // 1 = continue processing
    TEST_ASSERT_EQUAL(1, script_process_line("   "));
}

void test_process_comment(void) {
    TEST_ASSERT_EQUAL(1, script_process_line("# this is a comment"));  // 1 = continue
}

void test_process_simple_command(void) {
    // This would actually execute, but should return cleanly
    TEST_ASSERT_GREATER_OR_EQUAL(0, script_process_line("true"));
}

// ============================================================================
// Execute String Tests
// ============================================================================

void test_execute_empty_string(void) {
    TEST_ASSERT_EQUAL(0, script_execute_string(""));
}

void test_execute_simple_command(void) {
    int result = script_execute_string("true");
    TEST_ASSERT_EQUAL(0, result);
}

void test_execute_failing_command(void) {
    int result = script_execute_string("false");
    TEST_ASSERT_EQUAL(1, result);
}

// ============================================================================
// Case Statement Tests
// ============================================================================

void test_execute_simple_case(void) {
    int result = script_execute_string("case a in\na) true;;\nesac");
    TEST_ASSERT_EQUAL(0, result);
}

void test_execute_case_no_match(void) {
    int result = script_execute_string("case x in\na) true;;\nesac");
    TEST_ASSERT_EQUAL(0, result);  // No match = exit 0
}

void test_execute_case_wildcard(void) {
    int result = script_execute_string("case abc in\n*) true;;\nesac");
    TEST_ASSERT_EQUAL(0, result);
}

void test_execute_case_multiple_patterns(void) {
    int result = script_execute_string("case b in\na|b|c) true;;\nesac");
    TEST_ASSERT_EQUAL(0, result);
}

void test_execute_case_exit_code(void) {
    int result = script_execute_string("case a in\na) false;;\nesac");
    TEST_ASSERT_EQUAL(1, result);  // false returns 1
}

int main(void) {
    UNITY_BEGIN();

    // Keyword detection
    RUN_TEST(test_is_keyword_if);
    RUN_TEST(test_is_keyword_then);
    RUN_TEST(test_is_keyword_else);
    RUN_TEST(test_is_keyword_fi);
    RUN_TEST(test_is_keyword_for);
    RUN_TEST(test_is_keyword_while);
    RUN_TEST(test_is_keyword_do);
    RUN_TEST(test_is_keyword_done);
    RUN_TEST(test_is_keyword_case);
    RUN_TEST(test_is_keyword_esac);
    RUN_TEST(test_is_keyword_in);
    RUN_TEST(test_is_not_keyword);
    RUN_TEST(test_get_keyword_type);

    // Line classification
    RUN_TEST(test_classify_empty_line);
    RUN_TEST(test_classify_comment);
    RUN_TEST(test_classify_if);
    RUN_TEST(test_classify_then);
    RUN_TEST(test_classify_elif);
    RUN_TEST(test_classify_else);
    RUN_TEST(test_classify_fi);
    RUN_TEST(test_classify_for);
    RUN_TEST(test_classify_while);
    RUN_TEST(test_classify_do);
    RUN_TEST(test_classify_done);
    RUN_TEST(test_classify_case);
    RUN_TEST(test_classify_esac);
    RUN_TEST(test_classify_simple);

    // Context stack
    RUN_TEST(test_initial_context);
    RUN_TEST(test_push_context);
    RUN_TEST(test_pop_context);
    RUN_TEST(test_nested_context);
    RUN_TEST(test_pop_empty_context);
    RUN_TEST(test_should_execute_default);
    RUN_TEST(test_count_loops_at_current_depth_empty);
    RUN_TEST(test_count_loops_at_current_depth_one_loop);
    RUN_TEST(test_count_loops_at_current_depth_multiple_loops);
    RUN_TEST(test_count_loops_at_current_depth_with_if);
    RUN_TEST(test_push_case_context);
    RUN_TEST(test_count_loops_with_case);

    // Break/continue pending (POSIX lexical scoping)
    RUN_TEST(test_break_pending_initial);
    RUN_TEST(test_continue_pending_initial);
    RUN_TEST(test_set_break_pending);
    RUN_TEST(test_set_continue_pending);
    RUN_TEST(test_clear_break_continue);
    RUN_TEST(test_count_loops_lexical_scoping);

    // Function management
    RUN_TEST(test_define_function);
    RUN_TEST(test_get_undefined_function);
    RUN_TEST(test_redefine_function);

    // Script processing
    RUN_TEST(test_process_empty_line);
    RUN_TEST(test_process_comment);
    RUN_TEST(test_process_simple_command);

    // Execute string
    RUN_TEST(test_execute_empty_string);
    RUN_TEST(test_execute_simple_command);
    RUN_TEST(test_execute_failing_command);

    // Case statement tests
    RUN_TEST(test_execute_simple_case);
    RUN_TEST(test_execute_case_no_match);
    RUN_TEST(test_execute_case_wildcard);
    RUN_TEST(test_execute_case_multiple_patterns);
    RUN_TEST(test_execute_case_exit_code);

    return UNITY_END();
}
