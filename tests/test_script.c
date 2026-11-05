#include "unity.h"
#include "../src/script.h"
#include "../src/history.h"
#include "../src/hash.h"
#include "../src/config.h"
#include "../src/execute.h"
#include <stdio.h>
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

// ============================================================================
// Line Classification Tests
// ============================================================================

// ============================================================================
// Context Stack Tests
// ============================================================================

void test_pop_empty_context(void) {
    // Should fail gracefully
    TEST_ASSERT_EQUAL(-1, script_pop_context());
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

// ============================================================================
// History Recording Tests
// ============================================================================

#define HIST_TEST_FILE "/tmp/hash_test_script_history_54321"
#define SCRIPT_TEST_FILE "/tmp/hash_test_script_outer_54321.sh"
#define SCRIPT_TEST_INNER "/tmp/hash_test_script_inner_54321.sh"

static void history_test_begin(void) {
    setenv("HISTFILE", HIST_TEST_FILE, 1);
    setenv("HISTSIZE", "100", 1);
    setenv("HISTFILESIZE", "200", 1);
    unsetenv("HISTCONTROL");
    history_init();
    is_interactive = true;
}

static void history_test_end(void) {
    is_interactive = false;
    shell_option_set_nolog(false);
    script_state.record_history = false;
    history_clear();
    unlink(HIST_TEST_FILE);
    unlink(SCRIPT_TEST_FILE);
    unlink(SCRIPT_TEST_INNER);
    unsetenv("HISTFILE");
    unsetenv("HISTSIZE");
    unsetenv("HISTFILESIZE");
}

static void write_test_script(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fputs(content, fp);
    fclose(fp);
}

// A file executed without the -i script flag (e.g. sourced) records nothing
void test_sourced_file_does_not_record_history(void) {
    history_test_begin();
    write_test_script(SCRIPT_TEST_FILE, "# comment in sourced file\ntrue\ntrue # second\n");
    script_execute_file(SCRIPT_TEST_FILE, 0, NULL);
    TEST_ASSERT_EQUAL_INT(0, history_count());
    history_test_end();
}

// hash-shell -i script records the script's own lines
void test_interactive_script_records_history(void) {
    history_test_begin();
    script_state.record_history = true;
    write_test_script(SCRIPT_TEST_FILE, "true\ntrue # second\n");
    script_execute_file(SCRIPT_TEST_FILE, 0, NULL);
    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING("true", history_get(0));
    TEST_ASSERT_EQUAL_STRING("true # second", history_get(1));
    TEST_ASSERT_TRUE(script_state.record_history);  // restored after the file
    history_test_end();
}

// ...but files sourced from that script do not
void test_interactive_script_nested_source_not_recorded(void) {
    history_test_begin();
    script_state.record_history = true;
    write_test_script(SCRIPT_TEST_INNER, "true\ntrue # inner\n");
    write_test_script(SCRIPT_TEST_FILE, ". " SCRIPT_TEST_INNER "\ntrue # outer\n");
    script_execute_file(SCRIPT_TEST_FILE, 0, NULL);
    TEST_ASSERT_EQUAL_INT(2, history_count());
    TEST_ASSERT_EQUAL_STRING(". " SCRIPT_TEST_INNER, history_get(0));
    TEST_ASSERT_EQUAL_STRING("true # outer", history_get(1));
    history_test_end();
}

// set -o nolog stops recording
void test_nolog_disables_script_history(void) {
    history_test_begin();
    script_state.record_history = true;
    shell_option_set_nolog(true);
    write_test_script(SCRIPT_TEST_FILE, "true\ntrue # second\n");
    script_execute_file(SCRIPT_TEST_FILE, 0, NULL);
    TEST_ASSERT_EQUAL_INT(0, history_count());
    history_test_end();
}

// ============================================================================
// Compound Commands Fed One Line at a Time (as the interactive prompt does)
// ============================================================================

// script_execute_string must not swallow empty lines (heredocs, bodies)
void test_execute_string_keeps_blank_lines(void) {
    script_execute_string("hash_blank_fn() {\ntrue\n\ntrue\n}");
    ShellFunction *fn = script_get_function("hash_blank_fn");
    TEST_ASSERT_NOT_NULL(fn);
    TEST_ASSERT_NOT_NULL(strstr(fn->body, "true\n\ntrue"));
}

int main(void) {
    UNITY_BEGIN();

    // Keyword detection

    // Line classification

    // Context stack
    RUN_TEST(test_pop_empty_context);
    RUN_TEST(test_count_loops_at_current_depth_empty);
    RUN_TEST(test_count_loops_at_current_depth_one_loop);
    RUN_TEST(test_count_loops_at_current_depth_multiple_loops);
    RUN_TEST(test_count_loops_at_current_depth_with_if);
    RUN_TEST(test_count_loops_with_case);

    // Break/continue pending (POSIX lexical scoping)
    RUN_TEST(test_break_pending_initial);
    RUN_TEST(test_continue_pending_initial);
    RUN_TEST(test_count_loops_lexical_scoping);

    // Function management
    RUN_TEST(test_define_function);
    RUN_TEST(test_get_undefined_function);
    RUN_TEST(test_redefine_function);

    // Script processing

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

    // History recording
    RUN_TEST(test_sourced_file_does_not_record_history);
    RUN_TEST(test_interactive_script_records_history);
    RUN_TEST(test_interactive_script_nested_source_not_recorded);
    RUN_TEST(test_nolog_disables_script_history);

    // Compound commands fed one line at a time
    RUN_TEST(test_execute_string_keeps_blank_lines);

    return UNITY_END();
}
