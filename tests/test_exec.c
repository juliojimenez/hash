#include <limits.h>
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
    exec_abort_clear();  // a nesting test may have left the abort raised
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

// `command break|continue|return|exit` keep their control flow (#350); command
// only strips the "a special builtin's error exits the shell" property
void test_command_keeps_control_flow(void) {
    TEST_ASSERT_EQUAL_STRING("ok\n", run("for i in 1 2; do command continue; echo BUG; done; echo ok"));
    TEST_ASSERT_EQUAL_STRING("after\n", run("for i in 1 2; do command break; echo BUG; done; echo after"));
    TEST_ASSERT_EQUAL_STRING("1a\n2a\n", run("for i in 1 2; do for j in a b; do echo $i$j; command continue 2; done; done"));
    TEST_ASSERT_EQUAL_STRING("st=3\n", run("f() { command return 3; echo BUG; }; f; echo st=$?"));
    TEST_ASSERT_EQUAL_STRING("eval-ok\n", run("for i in 1 2; do eval \"command continue\"; echo BUG; done; echo eval-ok"));
    TEST_ASSERT_EQUAL_STRING("", run("command exit 4; echo BUG"));
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, status);
    TEST_ASSERT_EQUAL_INT(4, last_command_exit_code);
    // plain `command` is unchanged
    TEST_ASSERT_EQUAL_STRING("plain\n", run("command echo plain"));
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

    // A matched clause with no commands completes with status 0 (#351); a clause
    // with commands still sees the previous status in $?
    TEST_ASSERT_EQUAL_STRING("", run("false; case x in x) ;; esac"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("inside=1\n", run("false; case x in x) echo inside=$?;; esac"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("1\n", run("false || ! case x in x) ;; esac; echo $?"));
    // modernish's FTL_ORNOT check: the group must fail, so && does not fire
    TEST_ASSERT_EQUAL_STRING("", run("{ ! : || ! case x in x) ;; esac; } && echo FATAL"));
}

// Backslashes and quoted pattern characters in case words and patterns
// (#352, #353): an unquoted \\ is one literal backslash on both sides, a
// backslash in double quotes is literal unless it escapes $ ` " \, single quotes
// keep everything, and a quoted * ? [ is not a pattern character
void test_case_backslashes_and_quoted_patterns(void) {
    // modernish FTL_CASEBKSL and FTL_CASEBKSL2
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case \\\\z in ( \"\\z\" ) echo ok;; ( * ) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("set -- \\\\; case ab\\\\cd in ( *\"$1\"* ) echo ok;; ( * ) echo BUG;; esac"));
    // the word side: an unquoted \\ is one backslash
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case a\\\\b in (a\\\\b) echo ok;; (*) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case \\\\z in ('\\z') echo ok;; (*) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case '\\\\' in ('\\\\') echo ok;; (*) echo BUG;; esac"));
    // single quotes keep $ and \\$ literal
    TEST_ASSERT_EQUAL_STRING("ok\n", run("x=5; case '$x' in (5) echo BUG;; (*) echo ok;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case '\\$' in (\\\\\\$) echo ok;; (*) echo BUG;; esac"));
    // quoted pattern characters are literal
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case ab in (\"a*\") echo BUG;; (*) echo ok;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case 'a*' in (\"a*\") echo ok;; (*) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case 'a[b' in ('a[b') echo ok;; (*) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case fxo in (f\"*\"o) echo BUG;; (*) echo ok;; esac"));
    // unquoted pattern characters still match
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case abc in (\"a\"*) echo ok;; (*) echo BUG;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case abc in (a[bx]c) echo ok;; (*) echo BUG;; esac"));
}

// ${var:-word} words honour their quotes end to end, and function arguments
// arrive without quote markers (#375)
void test_default_words_and_function_args(void) {
    TEST_ASSERT_EQUAL_STRING("<a b><c>\n", run("f() { printf '<%s>' ${1+\"$@\"}; echo; }; f 'a b' c"));
    TEST_ASSERT_EQUAL_STRING("<>\n", run("f() { printf '<%s>' ${1+\"$@\"}; echo; }; f"));
    TEST_ASSERT_EQUAL_STRING("[a b]\n", run("printf '[%s]' ${hw_:-\"a b\"}; echo"));
    TEST_ASSERT_EQUAL_STRING("[a][b]\n", run("printf '[%s]' ${hw_:-a b}; echo"));
    TEST_ASSERT_EQUAL_STRING("[a][b]\n", run("printf '[%s]' ${hz_:-$(echo a b)}; echo"));
    TEST_ASSERT_EQUAL_STRING("[a b]\n", run("printf '[%s]' \"${hy_:-$(echo a b)}\"; echo"));
    TEST_ASSERT_EQUAL_STRING("[p q]\n", run("hv_='p q'; printf '[%s]' ${hw_:-\"$hv_\"}; echo"));
    TEST_ASSERT_EQUAL_STRING("[x y][z]\n", run("hu_=set; printf '[%s]' ${hu_:+\"x y\" z}; echo"));
    TEST_ASSERT_EQUAL_STRING("hi there\n", run(": ${hx_:=$(echo hi there)}; echo \"$hx_\""));
    TEST_ASSERT_EQUAL_STRING("3 3\n", run("f() { echo \"${#1} ${#2}\"; }; f 'a;b' 'a*b'"));
    TEST_ASSERT_EQUAL_STRING("m: a; b\nm: c d\n", run("f() { printf 'm: %s\\n' \"$@\"; }; f 'a; b' 'c d'"));
}

// Bugs found while running modernish's fatal-bug battery (#355 PR), all in v51
void test_modernish_battery_findings(void) {
    // arithmetic inside a double-quoted case word is expanded
    TEST_ASSERT_EQUAL_STRING("ok\n", run("case \"$((1+1))\" in (2) echo ok;; (*) echo BUG;; esac"));
    // a command substitution runs what follows an external command
    TEST_ASSERT_EQUAL_STRING("a\nb\n", run("x=$(/bin/echo a; echo b); echo \"$x\""));
    // set -- inside a function owns its parameters (was a double free)
    TEST_ASSERT_EQUAL_STRING("2 x y\nafter 0\n", run("f() { set -- x y; echo \"$# $1 $2\"; }; f a b; echo \"after $#\""));
    // exec with a command applies its redirections instead of passing them as words
    TEST_ASSERT_EQUAL_STRING("[a]\n", run("x=$(exec echo a 2>/dev/null); echo \"[$x]\""));
    // parentheses and comments inside quotes in a command substitution
    TEST_ASSERT_EQUAL_STRING("[(]\n", run("x=$(echo \"(\"); echo \"[$x]\""));
    TEST_ASSERT_EQUAL_STRING("[(]\n", run("x=\"$(echo \"(\")\"; echo \"[$x]\""));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("eval ': || $( : # \"\n)' && echo ok"));
    // an escaped brace in a default word; double-quote rules when the ${...} is quoted
    TEST_ASSERT_EQUAL_STRING("[a}b]\n", run("u=1; x=${u:+a\\}b}; echo \"[$x]\""));
    TEST_ASSERT_EQUAL_STRING("$x a\\}b\n", run("echo \"${hq_:-\\$x} ${hq_:-a\\}b}\""));
    // an unquoted expansion in a case pattern is a pattern; a quote character from one is literal
    TEST_ASSERT_EQUAL_STRING("y\n", run("p='[+-]*'; case --test in ($p) echo y;; (*) echo n;; esac"));
    TEST_ASSERT_EQUAL_STRING("ok\n", run("foo=\\\"; case \"$foo\" in $foo ) echo ok;; *) echo BUG;; esac"));
    // a variable name built from an expansion inside arithmetic; assignment under set -u
    TEST_ASSERT_EQUAL_STRING("8\n", run("v=b; _a_b=7; echo $((_a_${v} + 1))"));
    TEST_ASSERT_EQUAL_STRING("[1]\n", run("set -u; unset hu_; : $((hu_ = 1)); echo \"[$hu_]\"; set +u"));
}

// A builtin that prints a listing fails with status 1 when the write fails
// (#364); jobs with no jobs prints nothing. /dev/full exists on Linux only.
void test_listing_write_errors(void) {
    TEST_ASSERT_EQUAL_STRING("", run("jobs"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    if (access("/dev/full", W_OK) != 0) {
        TEST_IGNORE_MESSAGE("no /dev/full on this system");
    }
    TEST_ASSERT_EQUAL_STRING("1\n", run("export >/dev/full 2>/dev/null; echo $?"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("set >/dev/full 2>/dev/null; echo $?"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("help >/dev/full 2>/dev/null; echo $?"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("kill -l >/dev/full 2>/dev/null; echo $?"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("alias hx_=y; alias >/dev/full 2>/dev/null; echo $?; unalias hx_"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("type echo >/dev/full 2>/dev/null; echo $?"));
    TEST_ASSERT_EQUAL_STRING("0\n", run("export >/dev/null; echo $?"));
}

// read assigns shell variables, not environment entries (#365): a name a for
// loop used takes the new value, nothing is exported, an exported variable
// stays exported with the new value, REPLY works, a readonly name fails
void test_read_assigns_shell_variables(void) {
    TEST_ASSERT_EQUAL_STRING("fromread\n", run("for hr_ in a b; do :; done; echo fromread | { read hr_; echo \"$hr_\"; }"));
    TEST_ASSERT_EQUAL_STRING("0\n", run("echo v | { read hr2_; env | grep -c '^hr2_='; }"));
    TEST_ASSERT_EQUAL_STRING("hr3_=new\n", run("export hr3_=old; echo new | { read hr3_; env | grep '^hr3_='; }"));
    TEST_ASSERT_EQUAL_STRING("[a][b c]\n", run("echo 'a b c' | { read hp_ hq_; echo \"[$hp_][$hq_]\"; }"));
    TEST_ASSERT_EQUAL_STRING("[line] 0\n", run("echo line | { read; echo \"[$REPLY] $(env | grep -c '^REPLY=')\"; }"));
    TEST_ASSERT_EQUAL_STRING("st=1 [keep]\n", run("readonly hro_=keep; echo x | { read hro_ 2>/dev/null; echo \"st=$? [$hro_]\"; }"));
}

// eval with no arguments, null arguments or blank text succeeds (POSIX)
void test_eval_empty_is_success(void) {
    TEST_ASSERT_EQUAL_STRING("0\n", run("false; eval; echo $?"));
    TEST_ASSERT_EQUAL_STRING("0\n", run("false; eval \"\"; echo $?"));
    TEST_ASSERT_EQUAL_STRING("0\n", run("false; eval \"\" \"\"; echo $?"));
    TEST_ASSERT_EQUAL_STRING("0\n", run("false; eval \" \"; echo $?"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("eval false; echo $?"));
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

// ============================================================================
// Line feeder (prompt, stdin and script files feed one line at a time)
// ============================================================================

static ExecFeeder feeder;

static FeedResult feed(const char *line, ExecStatus *st) {
    return exec_feed_line(&feeder, line, st);
}

void test_feeder_runs_complete_lines_immediately(void) {
    exec_feeder_init(&feeder);
    ExecStatus st;
    const char *out = NULL;
    fflush(stdout);
    int saved = fcntl(STDOUT_FILENO, F_DUPFD, 20);
    int low = open(OUT_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    int fd = fcntl(low, F_DUPFD, 21);
    close(low);
    dup2(fd, STDOUT_FILENO);

    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("echo one", &st));
    TEST_ASSERT_EQUAL_INT(EXEC_OK, st);
    TEST_ASSERT_FALSE(exec_feeder_pending(&feeder));

    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("for i in 1 2", &st));
    TEST_ASSERT_TRUE(exec_feeder_pending(&feeder));
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("do", &st));
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("echo loop$i", &st));
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("done", &st));
    TEST_ASSERT_FALSE(exec_feeder_pending(&feeder));

    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("cat <<EOF", &st));
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("", &st));
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("body $((2+3))", &st));
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("EOF", &st));

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, captured, sizeof(captured) - 1);
    captured[n > 0 ? n : 0] = '\0';
    close(fd);
    unlink(OUT_FILE);
    out = captured;
    TEST_ASSERT_EQUAL_STRING("one\nloop1\nloop2\n\nbody 5\n", out);
    exec_feeder_free(&feeder);
}

void test_feeder_keeps_program_text_while_it_runs(void) {
    exec_feeder_init(&feeder);
    ExecStatus st;
    fflush(stdout);
    int saved = fcntl(STDOUT_FILENO, F_DUPFD, 20);
    int low = open(OUT_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    int fd = fcntl(low, F_DUPFD, 21);
    close(low);
    dup2(fd, STDOUT_FILENO);

    // A background job records its command from the text the program was
    // parsed from, which must outlive the feeder's buffer
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("sleep 5 & jobs; kill $!", &st));
    TEST_ASSERT_FALSE(exec_feeder_pending(&feeder));

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, captured, sizeof(captured) - 1);
    captured[n > 0 ? n : 0] = '\0';
    close(fd);
    unlink(OUT_FILE);
    TEST_ASSERT_NOT_NULL(strstr(captured, "sleep 5 &"));
    exec_feeder_free(&feeder);
}

void test_feeder_syntax_error_drops_pending_text(void) {
    exec_feeder_init(&feeder);
    ExecStatus st;
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("if true", &st));
    TEST_ASSERT_EQUAL_INT(FEED_SYNTAX_ERROR, feed("then done", &st));
    TEST_ASSERT_FALSE(exec_feeder_pending(&feeder));
    TEST_ASSERT_EQUAL_INT(2, last_command_exit_code);
    // Usable again afterwards
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("true", &st));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    exec_feeder_free(&feeder);
}

void test_feeder_reset_discards_pending_text(void) {
    exec_feeder_init(&feeder);
    ExecStatus st;
    TEST_ASSERT_EQUAL_INT(FEED_NEED_MORE, feed("while true", &st));
    exec_feeder_reset(&feeder);
    TEST_ASSERT_FALSE(exec_feeder_pending(&feeder));
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("false", &st));
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    exec_feeder_free(&feeder);
}

void test_feeder_reports_exit(void) {
    exec_feeder_init(&feeder);
    ExecStatus st;
    TEST_ASSERT_EQUAL_INT(FEED_RAN, feed("exit 5", &st));
    TEST_ASSERT_EQUAL_INT(EXEC_EXIT, st);
    TEST_ASSERT_EQUAL_INT(5, last_command_exit_code);
    script_state.exit_requested = false;
    exec_feeder_free(&feeder);
}

void test_exec_string_status_contract(void) {
    TEST_ASSERT_EQUAL_INT(0, exec_string_status("true"));
    TEST_ASSERT_EQUAL_INT(3, exec_string_status("(exit 3)"));
    TEST_ASSERT_EQUAL_INT(2, exec_string_status("for i in 1 2 2>/dev/null"));
}

// Blank lines inside a function body survive: the body is the brace group's
// source text, so a heredoc or an empty line in it is kept as written
void test_function_body_keeps_blank_lines(void) {
    run("hash_blank_fn() {\ntrue\n\ntrue\n}");
    ShellFunction *fn = script_get_function("hash_blank_fn");
    TEST_ASSERT_NOT_NULL(fn);
    TEST_ASSERT_NOT_NULL(strstr(fn->body, "true\n\ntrue"));
}

// $LINENO is the line of the command within the text being run
void test_lineno_counts_lines_of_the_text(void) {
    TEST_ASSERT_EQUAL_STRING("1 1\n", run("echo $LINENO $((LINENO))"));
    TEST_ASSERT_EQUAL_STRING("1\n2\n", run("echo $LINENO\necho $LINENO"));
    TEST_ASSERT_EQUAL_STRING("2\n", run("f() {\n  echo $LINENO\n}\nf"));  // defined on line 1, echo on line 2
    TEST_ASSERT_EQUAL_STRING("5\n", run("true\ntrue\ntrue\nf() {\n  echo $LINENO\n}\nf"));
}

// local: a function's variables go back to what they were when it returns
void test_local_shadows_and_restores(void) {
    TEST_ASSERT_EQUAL_STRING("inner\nglobal\n", run("x=global; f() { local x=inner; echo $x; }; f; echo $x"));
    TEST_ASSERT_EQUAL_STRING("unset\n", run("f() { local y=1; }; f; echo \"${y-unset}\""));
    TEST_ASSERT_EQUAL_STRING("in=unset\nout=g\n", run("x=g; f() { local x; echo \"in=${x-unset}\"; }; f; echo \"out=$x\""));
    TEST_ASSERT_EQUAL_STRING("in=unset\nout=g\n", run("x=g; f() { local x=l; unset x; echo \"in=${x-unset}\"; }; f; echo \"out=$x\""));
}

void test_local_is_dynamically_scoped_and_survives_return(void) {
    TEST_ASSERT_EQUAL_STRING("g sees f\nafter=top\n", run("g() { echo \"g sees $v\"; }; f() { local v=f; g; }; v=top; f; echo \"after=$v\""));
    TEST_ASSERT_EQUAL_STRING("st=3 v=0\n", run("f() { local v=1; return 3; }; v=0; f; echo \"st=$? v=$v\""));
    TEST_ASSERT_EQUAL_STRING("a=b=c\n", run("f() { local a=b=c; echo \"a=$a\"; }; f"));
}

void test_local_keeps_export_and_restores_environment(void) {
    TEST_ASSERT_EQUAL_STRING("E=2 env=2\nafter=1 env=1\n",
        run("export E=1; f() { local E=2; echo \"E=$E env=$(printenv E)\"; }; f; echo \"after=$E env=$(printenv E)\""));
}

void test_local_errors(void) {
    TEST_ASSERT_EQUAL_STRING("st=1\n", run("local z=1 2>/dev/null; echo \"st=$?\""));
    TEST_ASSERT_EQUAL_STRING("st=1 r=1\n", run("readonly r=1; f() { local r=2 2>/dev/null; echo \"st=$? r=$r\"; }; f"));
}

// ============================================================================
// Function nesting limit
// ============================================================================

void test_function_nesting_limit_from_funcnest(void) {
    shellvar_unset("FUNCNEST");
    TEST_ASSERT_EQUAL_INT(1000, exec_nesting_limit());
    shellvar_set("FUNCNEST", "50");
    TEST_ASSERT_EQUAL_INT(50, exec_nesting_limit());
    shellvar_set("FUNCNEST", "0");       // no depth limit, the stack guard remains
    TEST_ASSERT_EQUAL_INT(INT_MAX, exec_nesting_limit());
    shellvar_set("FUNCNEST", "abc");
    TEST_ASSERT_EQUAL_INT(INT_MAX, exec_nesting_limit());
    shellvar_unset("FUNCNEST");
    TEST_ASSERT_EQUAL_INT(0, exec_nesting_depth());
}

void test_function_recursion_aborts_with_status_1(void) {
    shellvar_set("FUNCNEST", "25");
    // The innermost call is refused, every level unwinds, nothing after the call runs
    TEST_ASSERT_EQUAL_STRING("", run("f() { f; echo never; }; f; echo never"));
    TEST_ASSERT_EQUAL_INT(EXEC_ABORT, status);
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_TRUE(exec_abort_pending());
    TEST_ASSERT_EQUAL_INT(0, exec_nesting_depth());
    // The top level clears the abort and the next command runs normally
    exec_abort_clear();
    TEST_ASSERT_FALSE(exec_abort_pending());
    TEST_ASSERT_EQUAL_STRING("ok\n", run("echo ok"));
    TEST_ASSERT_EQUAL_INT(EXEC_OK, status);
    // eval and . are levels too
    TEST_ASSERT_EQUAL_STRING("", run("e() { eval e; }; e; echo never"));
    TEST_ASSERT_EQUAL_INT(EXEC_ABORT, status);
    exec_abort_clear();
    shellvar_unset("FUNCNEST");
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
    RUN_TEST(test_command_keeps_control_flow);
    RUN_TEST(test_case);
    RUN_TEST(test_eval_empty_is_success);
    RUN_TEST(test_read_assigns_shell_variables);
    RUN_TEST(test_listing_write_errors);
    RUN_TEST(test_modernish_battery_findings);
    RUN_TEST(test_default_words_and_function_args);
    RUN_TEST(test_case_backslashes_and_quoted_patterns);

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

    RUN_TEST(test_feeder_runs_complete_lines_immediately);
    RUN_TEST(test_feeder_keeps_program_text_while_it_runs);
    RUN_TEST(test_feeder_syntax_error_drops_pending_text);
    RUN_TEST(test_feeder_reset_discards_pending_text);
    RUN_TEST(test_feeder_reports_exit);
    RUN_TEST(test_exec_string_status_contract);
    RUN_TEST(test_function_body_keeps_blank_lines);
    RUN_TEST(test_lineno_counts_lines_of_the_text);
    RUN_TEST(test_local_shadows_and_restores);
    RUN_TEST(test_local_is_dynamically_scoped_and_survives_return);
    RUN_TEST(test_local_keeps_export_and_restores_environment);
    RUN_TEST(test_local_errors);

    RUN_TEST(test_function_nesting_limit_from_funcnest);
    RUN_TEST(test_function_recursion_aborts_with_status_1);
    return UNITY_END();
}
