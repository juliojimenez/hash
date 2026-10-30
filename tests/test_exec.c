#include "unity.h"
#include "../src/exec.h"
#include "../src/execute.h"
#include "../src/script.h"
#include "../src/shellvar.h"
#include "../src/config.h"
#include "../src/trap.h"
#include "../src/hash.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OUT_FILE "/tmp/hash_exec_test_out.txt"
#define TMP_FILE "/tmp/hash_exec_test_tmp.txt"

static char captured[8192];
static ExecStatus status;
static bool incomplete;

void setUp(void) {
    script_init();
    shellvar_init();
    config_init();
    trap_init();
    is_interactive = false;
    last_command_exit_code = 0;
    unlink(TMP_FILE);
}

void tearDown(void) {
    shell_option_set_errexit(false);
    script_set_in_condition(false);
    script_cleanup();
    shellvar_cleanup();
    unlink(TMP_FILE);
}

// Run a program with stdout captured; exit status stays in last_command_exit_code
static const char *run(const char *src) {
    fflush(stdout);
    // Keep the harness descriptors above the range scripts under test may use (exec 3>, <&8)
    int saved = fcntl(STDOUT_FILENO, F_DUPFD, 20);
    int low = open(OUT_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    int fd = fcntl(low, F_DUPFD, 21);
    close(low);
    dup2(fd, STDOUT_FILENO);

    incomplete = false;
    status = exec_string(src, &incomplete);

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, captured, sizeof(captured) - 1);
    captured[n > 0 ? n : 0] = '\0';
    close(fd);
    unlink(OUT_FILE);
    return captured;
}

static const char *read_file(const char *path) {
    static char buf[4096];
    FILE *f = fopen(path, "r");
    if (!f) {
        return "(missing)";
    }
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// ============================================================================
// Simple commands, lists, and-or, pipelines
// ============================================================================

void test_simple_command_and_status(void) {
    TEST_ASSERT_EQUAL_STRING("hi\n", run("echo hi"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
    run("false");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
}

void test_list_and_and_or(void) {
    TEST_ASSERT_EQUAL_STRING("a\nb\n", run("echo a; echo b"));
    TEST_ASSERT_EQUAL_STRING("a\n", run("true && echo a || echo b"));
    TEST_ASSERT_EQUAL_STRING("b\n", run("false && echo a || echo b"));
    TEST_ASSERT_EQUAL_STRING("", run("false && echo a"));
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
}

void test_pipeline_and_negation(void) {
    TEST_ASSERT_EQUAL_STRING("HELLO\n", run("echo hello | tr a-z A-Z"));
    TEST_ASSERT_EQUAL_STRING("2\n", run("printf 'a\\nb\\n' | wc -l | tr -d ' '"));
    run("false | true");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    run("true | false");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    run("! false");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    run("! true");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
}

void test_variables_and_prefix_assignment(void) {
    TEST_ASSERT_EQUAL_STRING("1\n", run("X=1; echo $X"));
    TEST_ASSERT_EQUAL_STRING("inner=5\nouter=[]\n", run("Y=5 sh -c 'echo inner=$Y'; echo \"outer=[$Y]\""));
    TEST_ASSERT_EQUAL_STRING("inner=5\nouter=[1]\n", run("X=1; X=5 sh -c 'echo inner=$X'; echo \"outer=[$X]\""));
}

void test_command_substitution_in_words(void) {
    TEST_ASSERT_EQUAL_STRING("nested\n", run("echo $(echo nested)"));
    TEST_ASSERT_EQUAL_STRING("3\n", run("echo $((1+2))"));
}

void test_background_and_wait(void) {
    TEST_ASSERT_EQUAL_STRING("done\n", run("sleep 0 & wait; echo done"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
}

// ============================================================================
// Redirections
// ============================================================================

void test_output_input_redirection(void) {
    run("echo filed > " TMP_FILE);
    TEST_ASSERT_EQUAL_STRING("filed\n", read_file(TMP_FILE));
    TEST_ASSERT_EQUAL_STRING("filed\n", run("cat < " TMP_FILE));
    run("echo more >> " TMP_FILE);
    TEST_ASSERT_EQUAL_STRING("filed\nmore\n", read_file(TMP_FILE));
}

void test_redirection_is_undone_after_command(void) {
    TEST_ASSERT_EQUAL_STRING("visible\n", run("echo hidden > " TMP_FILE "; echo visible"));
}

void test_stderr_redirection_and_dup(void) {
    TEST_ASSERT_EQUAL_STRING("", run("echo to-err >&2 2>/dev/null"));
    TEST_ASSERT_EQUAL_STRING("both\n", run("echo both 2>&1"));
    run("sh -c 'echo e >&2' 2> " TMP_FILE);
    TEST_ASSERT_EQUAL_STRING("e\n", read_file(TMP_FILE));
}

void test_redirection_failure_sets_status(void) {
    run("echo x > /nonexistent-dir/file 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
}

void test_heredoc(void) {
    TEST_ASSERT_EQUAL_STRING("line one\n\nline $three\n", run("cat <<'EOF'\nline one\n\nline $three\nEOF\n"));
    TEST_ASSERT_EQUAL_STRING("value=42\n", run("v=42; cat <<EOF\nvalue=$v\nEOF\n"));
    TEST_ASSERT_EQUAL_STRING("a\n  b\n", run("cat <<-EOF\n\ta\n\t  b\n\tEOF\n"));
}

void test_compound_redirections(void) {
    run("for i in 1 2; do echo $i; done > " TMP_FILE);
    TEST_ASSERT_EQUAL_STRING("1\n2\n", read_file(TMP_FILE));
    TEST_ASSERT_EQUAL_STRING("[1]\n[2]\n", run("while read l; do echo \"[$l]\"; done < " TMP_FILE));
    run("{ echo a; echo b; } > " TMP_FILE);
    TEST_ASSERT_EQUAL_STRING("a\nb\n", read_file(TMP_FILE));
    TEST_ASSERT_EQUAL_STRING("yes\n", run("if true; then echo yes; fi 2>/dev/null"));
}

// ============================================================================
// Conditionals and loops
// ============================================================================

void test_if_elif_else(void) {
    TEST_ASSERT_EQUAL_STRING("one\n", run("if true; then echo one; elif true; then echo two; else echo three; fi"));
    TEST_ASSERT_EQUAL_STRING("two\n", run("if false; then echo one; elif true; then echo two; else echo three; fi"));
    TEST_ASSERT_EQUAL_STRING("three\n", run("if false; then echo one; elif false; then echo two; else echo three; fi"));
}

void test_if_with_no_branch_taken_is_success(void) {
    run("if false; then :; fi");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    run("false; if false; then :; fi");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
}

void test_for_loop(void) {
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", run("for i in 1 2 3; do echo $i; done"));
    TEST_ASSERT_EQUAL_STRING("a b\nc\n", run("for w in 'a b' c; do echo $w; done"));
    TEST_ASSERT_EQUAL_STRING("x\ny\n", run("list='x y'; for v in $list; do echo $v; done"));
    run("for i in; do echo never; done");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
}

void test_while_until(void) {
    TEST_ASSERT_EQUAL_STRING("3\n", run("n=0; while [ $n -lt 3 ]; do n=$((n+1)); done; echo $n"));
    TEST_ASSERT_EQUAL_STRING("5\n", run("n=0; until [ $n -ge 5 ]; do n=$((n+1)); done; echo $n"));
    run("while false; do :; done");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
}

void test_break_and_continue(void) {
    TEST_ASSERT_EQUAL_STRING("1\n3\n", run("for i in 1 2 3; do if [ $i = 2 ]; then continue; fi; echo $i; done"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("for i in 1 2 3; do echo $i; break; done"));
    TEST_ASSERT_EQUAL_STRING("1a\n", run("for i in 1 2; do for j in a b; do echo $i$j; break 2; done; done"));
    TEST_ASSERT_EQUAL_STRING("1a\n2a\n", run("for i in 1 2; do for j in a b; do echo $i$j; continue 2; done; done"));
    TEST_ASSERT_EQUAL_STRING("after\n", run("n=0; while true; do n=$((n+1)); if [ $n = 3 ]; then break; fi; done; echo after"));
}

void test_case(void) {
    TEST_ASSERT_EQUAL_STRING("match\n", run("case abc in a*) echo match;; *) echo no;; esac"));
    TEST_ASSERT_EQUAL_STRING("yes\n", run("x=hello; case $x in h?llo|bye) echo yes;; esac"));
    TEST_ASSERT_EQUAL_STRING("q\n", run("case \"a b\" in \"a b\") echo q;; esac"));
    TEST_ASSERT_EQUAL_STRING("", run("case zzz in a) echo a;; b) echo b;; esac"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("second\n", run("case 2 in 1) echo first;; 2) echo second;; esac"));
}

// ============================================================================
// Groups, subshells, functions
// ============================================================================

void test_brace_group_runs_in_current_shell(void) {
    TEST_ASSERT_EQUAL_STRING("1\n", run("{ x=1; }; echo $x"));
}

void test_subshell_isolates_state_and_reports_status(void) {
    TEST_ASSERT_EQUAL_STRING("1\n", run("x=1; (x=2); echo $x"));
    run("(exit 3)");
    TEST_ASSERT_EQUAL_INT(3, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
    TEST_ASSERT_EQUAL_STRING("sub\n", run("(echo sub) | cat"));
}

void test_function_definition_and_call(void) {
    TEST_ASSERT_EQUAL_STRING("in-f arg 2\n", run("f() { echo in-f $1 $#; }; f arg two"));
    run("g() { return 4; }; g");
    TEST_ASSERT_EQUAL_INT(4, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
    TEST_ASSERT_EQUAL_STRING("in-f x 1\n", run("f() { echo in-f $1 $#; }; h() { f x; }; h"));
    TEST_ASSERT_EQUAL_STRING("one\n", run("f() { echo one; return; echo two; }; f"));
}

void test_function_with_loop_and_break(void) {
    TEST_ASSERT_EQUAL_STRING("1\n", run("f() { for i in 1 2 3; do echo $i; break; done; }; f"));
    TEST_ASSERT_EQUAL_STRING("sub\n", run("f() ( echo sub ); f"));
}

void test_function_redefinition(void) {
    TEST_ASSERT_EQUAL_STRING("new\n", run("f() { echo old; }; f() { echo new; }; f"));
}

// ============================================================================
// exit, errexit, errors
// ============================================================================

void test_exit_stops_everything(void) {
    TEST_ASSERT_EQUAL_STRING("a\n", run("echo a; exit 7; echo b"));
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, status);
    TEST_ASSERT_EQUAL_INT(7, last_command_exit_code);
    script_state.exit_requested = false;
    TEST_ASSERT_EQUAL_STRING("", run("f() { exit 9; }; f; echo not-reached"));
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, status);
    TEST_ASSERT_EQUAL_INT(9, last_command_exit_code);
    script_state.exit_requested = false;
}

void test_errexit(void) {
    TEST_ASSERT_EQUAL_STRING("", run("set -e; false; echo not-reached"));
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, status);
    script_state.exit_requested = false;
    shell_option_set_errexit(false);
    TEST_ASSERT_EQUAL_STRING("alive\n", run("set -e; false && true; echo alive"));
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
    shell_option_set_errexit(false);
    TEST_ASSERT_EQUAL_STRING("alive\n", run("set -e; if false; then :; fi; echo alive"));
    shell_option_set_errexit(false);
    TEST_ASSERT_EQUAL_STRING("alive\n", run("set -e; ! true; echo alive"));
    shell_option_set_errexit(false);
}

void test_syntax_error_reports_status_2(void) {
    run("if false; then done; fi 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(2, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
}

void test_incomplete_input_runs_nothing(void) {
    TEST_ASSERT_EQUAL_STRING("", run("for i in 1 2"));
    TEST_ASSERT_TRUE(incomplete);
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
}

void test_nested_control_flow(void) {
    TEST_ASSERT_EQUAL_STRING("1\n", run("if true; then for i in 1; do { echo $i; }; done; fi"));
    TEST_ASSERT_EQUAL_STRING("q\n", run("printf 'x\\nq\\n' | while read l; do case $l in q) echo q; break;; esac; done"));
}

// ============================================================================
// Regressions found by running the smoosh suite through the executor
// ============================================================================

void test_long_pipeline_through_eval(void) {
    // eleven stages: the old pipeline array used to stop growing at eight
    TEST_ASSERT_EQUAL_STRING("works\n", run("c='echo works'; for n in 1 2 3 4 5 6 7 8 9 10; do c=\"$c | { read x; echo \\$x; }\"; done; eval $c"));
}

void test_special_builtin_redirect_error_exits(void) {
    run(": 2>&9; echo oh no");
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, status);
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("", captured);
    TEST_ASSERT_EQUAL_STRING("after\n", run("true 2>&9; echo after"));
}

void test_exec_redirections_persist(void) {
    run("exec 3> " TMP_FILE "; echo via-three >&3; exec 3>&-");
    TEST_ASSERT_EQUAL_STRING("via-three\n", read_file(TMP_FILE));
    run("echo hi > " TMP_FILE "; command exec 8< " TMP_FILE "; read msg <&8; echo $msg; exec 8<&-");
    TEST_ASSERT_EQUAL_STRING("hi\n", captured);
}

void test_for_readonly_checked_every_iteration(void) {
    TEST_ASSERT_EQUAL_STRING("a\n1\n", run("(for x in a b c; do echo $x; readonly x; done); echo $?"));
}

void test_heredoc_backslash_newline_joins_lines(void) {
    TEST_ASSERT_EQUAL_STRING("ab\n$P.\\'q\n", run("cat <<EOF\na\\\nb\n\\$P.\\\\'q\nEOF\n"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_simple_command_and_status);
    RUN_TEST(test_list_and_and_or);
    RUN_TEST(test_pipeline_and_negation);
    RUN_TEST(test_variables_and_prefix_assignment);
    RUN_TEST(test_command_substitution_in_words);
    RUN_TEST(test_background_and_wait);

    RUN_TEST(test_output_input_redirection);
    RUN_TEST(test_redirection_is_undone_after_command);
    RUN_TEST(test_stderr_redirection_and_dup);
    RUN_TEST(test_redirection_failure_sets_status);
    RUN_TEST(test_heredoc);
    RUN_TEST(test_compound_redirections);

    RUN_TEST(test_if_elif_else);
    RUN_TEST(test_if_with_no_branch_taken_is_success);
    RUN_TEST(test_for_loop);
    RUN_TEST(test_while_until);
    RUN_TEST(test_break_and_continue);
    RUN_TEST(test_case);

    RUN_TEST(test_brace_group_runs_in_current_shell);
    RUN_TEST(test_subshell_isolates_state_and_reports_status);
    RUN_TEST(test_function_definition_and_call);
    RUN_TEST(test_function_with_loop_and_break);
    RUN_TEST(test_function_redefinition);

    RUN_TEST(test_exit_stops_everything);
    RUN_TEST(test_errexit);
    RUN_TEST(test_syntax_error_reports_status_2);
    RUN_TEST(test_incomplete_input_runs_nothing);
    RUN_TEST(test_nested_control_flow);

    RUN_TEST(test_long_pipeline_through_eval);
    RUN_TEST(test_special_builtin_redirect_error_exits);
    RUN_TEST(test_exec_redirections_persist);
    RUN_TEST(test_for_readonly_checked_every_iteration);
    RUN_TEST(test_heredoc_backslash_newline_joins_lines);

    return UNITY_END();
}
