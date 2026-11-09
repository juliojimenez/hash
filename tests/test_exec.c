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
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
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

// read splits the line the way field splitting does (#385): IFS whitespace is
// trimmed at both ends and runs of it delimit, any other IFS byte delimits on
// its own so adjacent ones make empty fields, the last name takes the fields
// left over with their delimiters, an empty IFS keeps the whole line, and end
// of input with nothing read assigns empty values with status 1
void test_read_splits_on_ifs(void) {
    TEST_ASSERT_EQUAL_STRING("[ab  cd]\n", run("printf '  ab  cd  \\n' | { IFS=' ' read hrs_; printf '[%s]\\n' \"$hrs_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[ab][cd]\n", run("printf '  ab  cd  \\n' | { IFS=' ' read hrs_ hrt_; printf '[%s][%s]\\n' \"$hrs_\" \"$hrt_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a\tb\tc]\n", run("printf 'a\\tb\\tc\\n' | { read hrs_; printf '[%s]\\n' \"$hrs_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a][][c]\n", run("printf 'a::c\\n' | { IFS=: read hrs_ hrt_ hru_; printf '[%s][%s][%s]\\n' \"$hrs_\" \"$hrt_\" \"$hru_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[][a][]\n", run("printf ':a:\\n' | { IFS=: read hrs_ hrt_ hru_; printf '[%s][%s][%s]\\n' \"$hrs_\" \"$hrt_\" \"$hru_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a][b:c:]\n", run("printf 'a:b:c:\\n' | { IFS=: read hrs_ hrt_; printf '[%s][%s]\\n' \"$hrs_\" \"$hrt_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a][b]\n", run("printf 'a:b:\\n' | { IFS=: read hrs_ hrt_; printf '[%s][%s]\\n' \"$hrs_\" \"$hrt_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a][b][c  d]\n", run("printf 'a : b:c  d\\n' | { IFS=' :' read hrs_ hrt_ hru_; printf '[%s][%s][%s]\\n' \"$hrs_\" \"$hrt_\" \"$hru_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a]\n", run("printf 'a : \\n' | { IFS=': ' read hrs_; printf '[%s]\\n' \"$hrs_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[  ab  cd  ][]\n", run("printf '  ab  cd  \\n' | { IFS= read hrs_ hrt_; printf '[%s][%s]\\n' \"$hrs_\" \"$hrt_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a  b]\n", run("printf '  a  b  \\n' | { read -r hrs_; printf '[%s]\\n' \"$hrs_\"; }"));
    TEST_ASSERT_EQUAL_STRING("rc=1 [][]\n", run("hrs_=old hrt_=old; printf '' | { read hrs_ hrt_; printf 'rc=%s [%s][%s]\\n' \"$?\" \"$hrs_\" \"$hrt_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[v]\n", run("echo v | { read -r -- hrs_; printf '[%s]\\n' \"$hrs_\"; }"));
}

// Without -r a backslash escapes the next byte: \x is x, an escaped IFS byte
// neither splits nor trims, a backslash-newline joins the next line, and a
// backslash right before end of input is dropped with the status saying end
// of file. With -r every backslash is literal. REPLY gets the same treatment
void test_read_handles_backslashes(void) {
    TEST_ASSERT_EQUAL_STRING("[axb]\n", run("printf 'a\\\\xb\\n' | { read hrb_; printf '[%s]\\n' \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a\\xb]\n", run("printf 'a\\\\xb\\n' | { read -r hrb_; printf '[%s]\\n' \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a\\b]\n", run("printf 'a\\\\\\\\b\\n' | { read hrb_; printf '[%s]\\n' \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a b][c]\n", run("printf 'a\\\\ b c\\n' | { read hrb_ hrc_; printf '[%s][%s]\\n' \"$hrb_\" \"$hrc_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a ]\n", run("printf 'a\\\\ \\n' | { read hrb_; printf '[%s]\\n' \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a:b][c]\n", run("printf 'a\\\\:b:c\\n' | { IFS=: read hrb_ hrc_; printf '[%s][%s]\\n' \"$hrb_\" \"$hrc_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[ab][c]\n", run("printf 'a\\\\\\nb\\nc\\n' | { read hrb_; read hrc_; printf '[%s][%s]\\n' \"$hrb_\" \"$hrc_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a\\][b]\n", run("printf 'a\\\\\\nb\\nc\\n' | { read -r hrb_; read -r hrc_; printf '[%s][%s]\\n' \"$hrb_\" \"$hrc_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a  b]\n", run("printf 'a\\\\\\n  b\\n' | { read hrb_; printf '[%s]\\n' \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[a][b c]\n", run("printf 'a \\\\\\nb c\\n' | { read hrb_ hrc_; printf '[%s][%s]\\n' \"$hrb_\" \"$hrc_\"; }"));
    TEST_ASSERT_EQUAL_STRING("rc=1 [a]\n", run("printf 'a\\\\\\n' | { read hrb_; printf 'rc=%s [%s]\\n' \"$?\" \"$hrb_\"; }"));
    TEST_ASSERT_EQUAL_STRING("[axb][ab]\n", run("printf 'a\\\\xb\\na\\\\\\nb\\n' | { read; hrb_=$REPLY; read; printf '[%s][%s]\\n' \"$hrb_\" \"$REPLY\"; }"));
}

// Prefix assignments persist only after a POSIX special builtin (2.9.1): read
// and cd run in this process but restore theirs like any command. Each
// command restores only its own, so a special builtin run inside eval or a
// function body leaves the caller's in place, eval sees them throughout, and
// a name assigned twice goes back to its original value
void test_prefix_assignments_follow_the_special_builtin_rule(void) {
    TEST_ASSERT_EQUAL_STRING("[1]\n", run("hrp_=1; hrp_=2 read hrq_ </dev/null; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[3]\n", run("hrp_=3; hrp_=4 cd .; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[2]\n", run("hrp_=1; hrp_=2 :; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[3]\n", run("hrp_=3; hrp_=6 command eval :; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[1]\n", run("hrpf_() { :; }; hrp_=1; hrp_=2 hrpf_; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("66[6]\n", run("hrp_=3; hrp_=6 eval 'printf %s $hrp_; printf %s $hrp_'; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[0]\n", run("hrp_=0; hrp_=1 hrp_=2 true; echo \"[$hrp_]\""));
    TEST_ASSERT_EQUAL_STRING("[:]\n", run("echo l | { IFS=:; IFS=' ' read hrq_; printf '[%s]\\n' \"$IFS\"; }"));
}

// A $name, $N or $? inside $(( )) is substituted as text before evaluation
// (POSIX), so a parameter holding an expression is evaluated, not read as a
// number: modernish's let does `return "$((!($1)))"` with $1 = "0 == 0"
void test_arith_substitutes_parameters_as_text(void) {
    TEST_ASSERT_EQUAL_STRING("0\n", run("f() { echo $(( !($1) )); }; f '0 == 0'"));
    TEST_ASSERT_EQUAL_STRING("1\n", run("f() { echo $(( !($1) )); }; f '0 == 1'"));
    TEST_ASSERT_EQUAL_STRING("3 3\n", run("hx_='1+2'; echo $(( $hx_ )) $(( ${hx_} ))"));
    TEST_ASSERT_EQUAL_STRING("6 6 5\n", run("set -- 5; echo $(( $1 + 1 )) $(( ${1} + 1 )) $(( $1 ))"));
    TEST_ASSERT_EQUAL_STRING("4\n", run("echo $(( $(echo 3) + 1 ))"));
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

void test_alias_takes_effect_for_later_programs(void) {
    TEST_ASSERT_EQUAL_STRING("", run("alias hi='echo hi'; hi 2>/dev/null"));
    TEST_ASSERT_EQUAL_INT(127, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("hi\n", run("hi"));
    TEST_ASSERT_EQUAL_STRING("hi there\n", run("hi there"));
    run("alias not='! '");
    run("not false");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    run("not true");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("yes\n", run("if not false; then echo yes; fi"));
    TEST_ASSERT_EQUAL_STRING("", run("unalias hi; unalias not"));
}

// The stored body has the alias values in place: it must not be expanded
// again when the function runs (an alias may have changed or gone by then)
void test_alias_in_function_body_is_stored_expanded(void) {
    run("alias not='! '");
    run("f() { not false; }");
    ShellFunction *fn = script_get_function("f");
    TEST_ASSERT_NOT_NULL(fn);
    TEST_ASSERT_EQUAL_STRING("{ !  false; }", fn->body);
    run("f");
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    run("unalias not");
}

void test_kill_l_translates_names_and_numbers(void) {
    TEST_ASSERT_EQUAL_STRING("KILL\nTERM\n", run("kill -l 9 15"));
    TEST_ASSERT_EQUAL_STRING("1\n2\n", run("kill -l HUP SIGINT"));
    TEST_ASSERT_EQUAL_STRING("HUP\n", run("kill -l 129"));
    run("kill -l 999 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
}

void test_umask_shows_and_sets_the_mask(void) {
    mode_t saved = umask(022);
    TEST_ASSERT_EQUAL_STRING("0022\n", run("umask 022; umask"));
    TEST_ASSERT_EQUAL_STRING("u=rwx,g=rx,o=\n", run("umask 027; umask -S"));
    TEST_ASSERT_EQUAL_STRING("0027\n", run("umask 022; umask g-w,o=; umask"));
    TEST_ASSERT_EQUAL_STRING("0077\n", run("umask 0477; umask u+r; umask"));
    TEST_ASSERT_EQUAL_STRING("0022\n", run("umask a=rwx,g=rx,o=rx; umask"));
    run("umask 999 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    run("umask q+r 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("0077\n", run("umask 077; (umask 022); umask"));
    umask(saved);
}

void test_for_words_honour_noglob(void) {
    TEST_ASSERT_EQUAL_STRING("*\n", run("set -f; a='*'; for w in $a; do echo \"$w\"; done; set +f"));
}

void test_read_reports_end_of_file(void) {
    TEST_ASSERT_EQUAL_STRING("rc=1 x=abc\n", run("printf abc | { read -r x; echo rc=$? x=$x; }"));
    TEST_ASSERT_EQUAL_STRING("rc=0 x=abc\n", run("printf 'abc\\n' | { read -r x; echo rc=$? x=$x; }"));
}

// #390: unset -f removes a function (it was a TODO). A subshell removes its
// own copy and the parent keeps the function, a function may unset itself
// while it runs, a missing name is not an error, and unset without -f leaves
// functions alone (dash)
void test_unset_f_removes_the_function(void) {
    TEST_ASSERT_EQUAL_STRING("gone\nparent: F\n",
        run("f() { echo F; }; (unset -f f; command -v f >/dev/null || echo gone); echo \"parent: $(f)\""));
    TEST_ASSERT_EQUAL_STRING("in-h\nrc=127\n",
        run("h() { unset -f h; echo in-h; }; h; h 2>/dev/null; echo rc=$?"));
    TEST_ASSERT_EQUAL_STRING("0 P R 127\n",
        run("p() { echo P; }; q() { echo Q; }; r() { echo R; }; unset -f q nosuch q; s=$?; q 2>/dev/null; rc=$?; echo \"$s $(p) $(r) $rc\""));
    TEST_ASSERT_EQUAL_STRING("G [unset]\nN [unset]\n",
        run("g() { echo G; }; g=1; unset g; echo \"$(g) [${g-unset}]\"; n() { echo N; }; n=1; unset -v n; echo \"$(n) [${n-unset}]\""));
    TEST_ASSERT_EQUAL_STRING("rc=1 K\n",
        run("k() { echo K; }; unset -fv k 2>/dev/null; echo \"rc=$? $(k)\""));
}

void test_finished_background_jobs_make_room(void) {
    TEST_ASSERT_EQUAL_STRING("ok\n", run("i=0; while [ $i -lt 80 ]; do ( : ) & i=$((i+1)); done; wait; echo ok"));
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
}

// #393: `command` runs an external program with the arguments it was given,
// expanded once; they used to go through execute() a second time
void test_command_does_not_expand_its_arguments_twice(void) {
    TEST_ASSERT_EQUAL_STRING("unset\n", run("unset v; command /dev/null \\${v=x} 2>/dev/null; echo \"${v-unset}\""));
    TEST_ASSERT_EQUAL_STRING("y\nunset\n", run("unset v; command sh -c 'echo ${v=y}'; echo \"${v-unset}\""));
    TEST_ASSERT_EQUAL_STRING("$(echo sub) * ~ \\$v a  b\n", run("command /bin/echo '$(echo sub)' '*' '~' '\\$v' \"a  b\""));
    TEST_ASSERT_EQUAL_STRING("1\n", run("command expr 1 '<' 2"));
    TEST_ASSERT_EQUAL_STRING("rc=0\n", run("command /bin/echo a > /dev/null; echo rc=$?"));
    TEST_ASSERT_EQUAL_STRING("rc=127\n", run("f() { echo fn; }; command f 2>/dev/null; echo rc=$?"));
}

// A real 0x03 in the case word used to be read as a region marker, which
// shifted the protected pairs and left marker bytes in the word for fnmatch
void test_case_word_with_control_bytes(void) {
    TEST_ASSERT_EQUAL_STRING("ok\n", run("c3=$(printf '\\003'); c1=$(printf '\\001'); v=\"a${c3}b*c\"; "
                                        "case $v in ( *\"$c1\"* ) echo bug ;; ( * ) echo ok ;; esac"));
    TEST_ASSERT_EQUAL_STRING("match\n", run("c3=$(printf '\\003'); v=\"a${c3}b\"; "
                                           "case $v in ( a\"$c3\"b ) echo match ;; ( * ) echo miss ;; esac"));
    TEST_ASSERT_EQUAL_STRING("joined\n", run("set -- x y; case \"$@\" in ( 'x y' ) echo joined ;; ( * ) echo other ;; esac"));
}

// Expansions used to be cut at 8 KB (the result buffer was sized from the
// input), the ${var-word} word at 1 KB and ${var#pat} at 4 KB
void test_long_expansions_are_not_cut(void) {
    const char *grow = "v=x; i=0; while [ $i -lt 14 ]; do v=$v$v; i=$((i+1)); done; ";
    char src[1024];
    snprintf(src, sizeof(src), "%s printf '%%s' \"$v\" | wc -c | tr -d ' '", grow);
    TEST_ASSERT_EQUAL_STRING("16384\n", run(src));
    snprintf(src, sizeof(src), "%s set -- \"$v\" \"$v\"; printf '%%s' \"$*\" | wc -c | tr -d ' '; printf '%%s' \"$@\" | wc -c | tr -d ' '", grow);
    TEST_ASSERT_EQUAL_STRING("32769\n32768\n", run(src));
    snprintf(src, sizeof(src), "%s w=${v#x}; echo ${#w}; u=${nope:-$v}; echo ${#u}; s=\"$v,$v\"; IFS=,; set -- $s; echo $# ${#1}", grow);
    TEST_ASSERT_EQUAL_STRING("16383\n16384\n2 16384\n", run(src));
}

void test_bad_substitution_is_an_error(void) {
    run("echo ${.sh.version+s} 2>/dev/null");
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("", run("echo \"${foo*}\" 2>/dev/null"));
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("rc=1\n", run("(eval '[[ -n ${.sh.version+s} ]]') 2>/dev/null; echo rc=$?"));
    TEST_ASSERT_EQUAL_STRING("0 1 1 y ten p\n", run("x=1; echo \"${#} ${#x} ${x:-d} ${x#1}y ${10:-ten} ${x:+p}\""));
}

// #392: a redirection's target is saved before its source is opened. With fds
// 0-3 in use and the target closed, open() returns the target's own number;
// the copy used to be taken afterwards, of the new source, and the restore
// put /dev/null back on the fd (modernish BUG_SCLOSEDFD)
void test_redirect_saves_a_closed_fd_before_opening_the_source(void) {
    TEST_ASSERT_EQUAL_STRING("closed\n",
        run("exec 3>/dev/null; exec 4>&-; { :; } 4>/dev/null; { true >&4; } 2>/dev/null && echo open || echo closed"));
    TEST_ASSERT_EQUAL_STRING("closed\n",
        run("exec 3>/dev/null; exec 4>&-; : 4>/dev/null; { true >&4; } 2>/dev/null && echo open || echo closed"));
    TEST_ASSERT_EQUAL_STRING("closed\n",
        run("exec 3>/dev/null; exec 4>&-\n"
            "{ { while :; do { exec 4>/dev/null; } 4>&-; break; done 4>&-\n"
            "    { true >&4; } 2>/dev/null && echo leaked; } 4>&-; } 4>/dev/null\n"
            "{ true >&4; } 2>/dev/null && echo open || echo closed"));
    // a bare exec onto a closed descriptor still takes effect, and a later
    // block that redirects it restores it rather than closing it (inside the
    // block fd 4 is the block's /dev/null, so "hi" goes there, as in dash)
    TEST_ASSERT_EQUAL_STRING("still\n",
        run("exec 3>/dev/null; exec 4>&-; exec 4>&1; { echo hi >&4; } 4>/dev/null; echo still >&4"));
}

// `command` used to hand its arguments on without their quote markers, so
// expr X '<' X became `expr X X < X` (and '>' created a file), and it ran a
// function of that name instead of skipping it
void test_command_keeps_quoted_operators_and_skips_functions(void) {
    TEST_ASSERT_EQUAL_STRING("0\nnofile\n", run("cd /tmp && command expr X '<' X; test -e /tmp/X && echo FILE || echo nofile"));
    TEST_ASSERT_EQUAL_STRING("1\nnofile\n", run("cd /tmp && command expr Y '>' X; test -e /tmp/X && echo FILE || echo nofile"));
    TEST_ASSERT_EQUAL_STRING("a*b x<y\n", run("command echo 'a*b' 'x<y'"));
    TEST_ASSERT_EQUAL_STRING("rc=127\n", run("f() { echo function; }; command f 2>/dev/null; echo rc=$?"));
    TEST_ASSERT_EQUAL_STRING("f\n", run("f() { echo function; }; command -v f"));
    TEST_ASSERT_EQUAL_STRING("ext\n", run("echo() { builtin_echo_replaced; }; command echo ext; unset -f echo"));
}

// A ${...} nested in the word of another, with quotes of its own, is one
// unit of that word: the segmenters used to cut the outer word at the
// inner quotes (modernish's str isint: ${2#"${2%%[!" $CCt$CCn"]*}"})
void test_nested_expansion_with_quotes_in_a_word(void) {
    TEST_ASSERT_EQUAL_STRING("[ 42 ]\n", run("x=' 42 '; echo \"[${x#\"${x%%\" \"*}\"}]\""));
    TEST_ASSERT_EQUAL_STRING("[42 ]\n", run("x=' 42 '; echo \"[${x#${x%%[!\" ab\"]*}}]\""));
    TEST_ASSERT_EQUAL_STRING("[ ]\n", run("x=' 42 '; echo \"[${nope:-\"${x%%[!\" ab\"]*}\"}]\""));
    TEST_ASSERT_EQUAL_STRING("[42 ]\n", run("t=$(printf '\\t'); set -- a ' 42 '; echo \"[${2#\"${2%%[!\" $t\"]*}\"}]\""));
    TEST_ASSERT_EQUAL_STRING("[ 42 ]\n", run("x=' 42 '; echo \"[${x#\"${x%% *}\"}]\""));
}

// A signal ignored when the shell started stays ignored: trap - does not
// make it default again and a trap on it is dropped (GitHub Actions runs
// every step with SIGPIPE ignored; modernish probes exactly this)
void test_trap_keeps_signals_ignored_on_entry(void) {
    signal(SIGPIPE, SIG_IGN);
    trap_init();
    TEST_ASSERT_EQUAL_STRING("alive\n", run("trap - PIPE; kill -s PIPE $$; echo alive"));
    TEST_ASSERT_EQUAL_STRING("alive too\n", run("trap 'echo trapped' PIPE; kill -s PIPE $$; echo alive too"));
    signal(SIGPIPE, SIG_DFL);
    trap_init();
    // (A trap action that fires from a real signal runs inside the handler,
    // which the thread sanitizer flags as signal-unsafe; test.sh covers that.)
}

// POSIX 2.5.2: unquoted $* and $@ give one field per parameter, each split
// again on IFS; where no splitting happens (an assignment, a here-document,
// a case word, the value of ${var=$*}) $* joins with the first IFS character
void test_unquoted_star_and_at_are_fields(void) {
    TEST_ASSERT_EQUAL_STRING("[a][b][c]\n", run("set -- 'a b' c; printf '[%s]' $*; echo"));
    TEST_ASSERT_EQUAL_STRING("[a][b][c]\n", run("set -- 'a b' c; printf '[%s]' $@; echo"));
    TEST_ASSERT_EQUAL_STRING("[one][two]\n", run("set -- one two; IFS=:; printf '[%s]' $*; echo; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[one][two]\n", run("set -- one two; IFS=; printf '[%s]' $*; echo; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[HIone][twoBYE]\n", run("set -- one two; IFS=; printf '[%s]' HI$*BYE; echo; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[a][b]\n", run("set -- a '' b; printf '[%s]' $@; echo"));
    TEST_ASSERT_EQUAL_STRING("[xa][b][cy]\n", run("set -- 'a b' c; printf '[%s]' x$@y; echo"));
    TEST_ASSERT_EQUAL_STRING("[a][b]\n", run("set -- ' a ' b; printf '[%s]' $*; echo"));
}

void test_star_joins_where_nothing_splits(void) {
    TEST_ASSERT_EQUAL_STRING("[one:two]\n", run("set -- one two; IFS=:; v=$*; printf '[%s]\\n' \"$v\"; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[one:two]\n", run("set -- one two; IFS=:; printf '[%s]\\n' \"$*\"; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[one][two] var=[one:two]\n", run("unset var; set -- one two; IFS=:; printf '[%s]' ${var=$*}; echo \" var=[$var]\"; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("one:two\n", run("set -- one two; IFS=:; cat <<X\n$*\nX\nunset IFS"));
    TEST_ASSERT_EQUAL_STRING("joined\n", run("set -- one two; IFS=:; case $* in (one:two) echo joined ;; (*) echo split ;; esac; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[one two]\n", run("set -- one two; IFS=:; v=$@; printf '[%s]\\n' \"$v\"; unset IFS"));
    TEST_ASSERT_EQUAL_STRING("[a b] [a b]\n", run("unset x; echo \"[${x=a b}] [$x]\""));
    TEST_ASSERT_EQUAL_STRING("[a][b] y=[a b]\n", run("unset y; printf '[%s]' ${y=a b}; echo \" y=[$y]\""));
}

void test_quoted_at_fields(void) {
    TEST_ASSERT_EQUAL_STRING("[0] x\n", run("f() { /bin/echo \"[$#]\" \"$@\" x; }; f"));
    TEST_ASSERT_EQUAL_STRING("[1]  x\n", run("set -- ''; /bin/echo \"[$#]\" \"$@\" x"));
    TEST_ASSERT_EQUAL_STRING("[a][][b]\n", run("set -- a '' b; printf '[%s]' \"$@\"; echo"));
    TEST_ASSERT_EQUAL_STRING("[][]\n", run("set -- '' ''; printf '[%s]' \"$@\"; echo"));
    TEST_ASSERT_EQUAL_STRING("done\n", run("set --; for w in \"$@\"; do echo iterated; done; echo done"));
    TEST_ASSERT_EQUAL_STRING("[xy]\n", run("set --; printf '[%s]' \"x$@y\"; echo"));
    TEST_ASSERT_EQUAL_STRING("n=3 second=to\n", run("f() { echo n=$# second=$2; }; g() { f \"$@\"; }; g n=1 to three"));
    TEST_ASSERT_EQUAL_STRING("[a b][a bz]\n", run("set -- a b; x=\"$@\"; y=\"$@\"z; printf '[%s]' \"$x\" \"$y\"; echo"));
}

void test_escaped_backslash_and_dollar(void) {
    TEST_ASSERT_EQUAL_STRING("\\star\n", run("V=star; W=\\\\${V}; printf '%s\\n' \"$W\""));
    TEST_ASSERT_EQUAL_STRING("\\\\star\n", run("V=star; printf '%s\\n' \\\\\\\\$V"));
    TEST_ASSERT_EQUAL_STRING("$x\n", run("x=VAL; printf '%s\\n' \\$x"));
    TEST_ASSERT_EQUAL_STRING("\\$x\n", run("x=VAL; printf '%s\\n' \\\\\\$x"));
}

// #384: a here-document with a quoted delimiter is literal, and the text of
// a $( ) is copied as written at parse time, so the body keeps every
// backslash and backslash-newline (dash: abc \<nl>def \\<nl>ghi \\\)
void test_heredoc_in_cmdsub_keeps_backslash_newline(void) {
    TEST_ASSERT_EQUAL_STRING("[abc \\\ndef \\\\\nghi \\\\\\]\n",
        run("v=$(cat <<-\\EOT\n\tabc \\\n\tdef \\\\\n\tghi \\\\\\\n\tEOT\n); printf '[%s]\\n' \"$v\""));
    // the same text reaching the parser through eval
    TEST_ASSERT_EQUAL_STRING("[abc \\\ndef \\\\]\n",
        run("nl='\n'; eval 'v=$(cat <<-\\EOT'\"$nl\"'abc \\'\"$nl\"'def \\\\'\"$nl\"'EOT'\"$nl\"')'; printf '[%s]\\n' \"$v\""));
}

// #384: with an unquoted delimiter a backslash before the newline continues
// the line: the next line is part of this one, so <<- does not strip its
// tabs and it is not tested as the delimiter (modernish string.t 010)
void test_heredoc_continued_line_keeps_tab_and_is_not_the_delimiter(void) {
    TEST_ASSERT_EQUAL_STRING("def \tghi\njkl\tFIN\n",
        run("cat <<-FIN\n\tdef \\\n\tghi\n\tjkl\\\n\tFIN\n\tFIN\n"));
    // a quoted delimiter: no continuation, the backslash stays, the next line
    // can end the body
    TEST_ASSERT_EQUAL_STRING("abc \\\n", run("cat <<-'X'\n\tabc \\\n\tX\n"));
    // an escaped backslash before the newline is not a continuation
    TEST_ASSERT_EQUAL_STRING("abc \\\nX2\n", run("cat <<X\nabc \\\\\nX2\nX\n"));
}

// A backslash inside single quotes is a literal byte: it does not escape the
// closing quote, nor the expansion, command substitution or case pattern
// that follows the quote ('a\'$x gave a$x; 'a\'`echo q` left the command)
void test_single_quoted_backslash_escapes_nothing_after_the_quote(void) {
    TEST_ASSERT_EQUAL_STRING("[a\\1][a\\q][a\\q][\\q][a\\1b\\1]\n",
        run("x=1; printf '[%s]' 'a\\'$x 'a\\'$(echo q) 'a\\'`echo q` \\\\`echo q` 'a\\'$x'b\\'$x; echo"));
    TEST_ASSERT_EQUAL_STRING("[a\\\n\tb]\n", run("x=1; printf '[%s]\\n' 'a\\'\"$(printf '\\n\\t')\"b"));
    TEST_ASSERT_EQUAL_STRING("m1 m2 m3\n",
        run("nl='\n'; x=1; v='a \\'$nl'b'\n"
            "case $v in ('a \\'$nl'b') printf 'm1 ';; (*) printf 'n1 ';; esac\n"
            "case 'a\\1' in ('a\\'$x) printf 'm2 ';; (*) printf 'n2 ';; esac\n"
            "case 'a\\\\' in ('a\\\\') echo m3;; (*) echo n3;; esac"));
    TEST_ASSERT_EQUAL_STRING("[abc def]\n",
        run("nl='\n'; eval 'printf \"[%s]\\n\" \"abc \\'\"$nl\"'def\"'"));
}

// #386: bracket expressions over the whole ASCII set. Three things had to
// hold: a run of escaped backslashes in double quotes keeps its length, an
// escaped marker-range byte in a case pattern travels protected, and the
// fnmatch pattern puts a ] member first (macOS fnmatch loses a backslash
// member that is followed by an escaped ]: [\\\]a] had no \ in it there)
void test_bracket_expression_members(void) {
    TEST_ASSERT_EQUAL_STRING("[\\\\\\] 3 [\\\\1] [\\\\q]\n",
        run("x=1; v=\"\\\\\\\\\\\\\"; printf '[%s] %s [%s] [%s]\\n' \"$v\" \"${#v}\" \"\\\\\\\\$x\" \"\\\\\\\\`echo q`\""));
    TEST_ASSERT_EQUAL_STRING("bs rb a\n",
        run("case '\\' in ( *[\\\\\\]a]* ) printf 'bs ';; ( * ) printf 'no ';; esac\n"
            "case ']' in ( *[\\\\\\]a]* ) printf 'rb ';; ( * ) printf 'no ';; esac\n"
            "case 'a' in ( *[\\\\\\]a]* ) echo a;; ( * ) echo no;; esac"));
    // an escaped control byte and an escaped $ in one pattern (the $ used to be expanded)
    TEST_ASSERT_EQUAL_STRING("ok\n",
        run("c1=$(printf '\\001'); eval \"case 'a' in ( *[\\\\${c1}\\\\\\$a]* ) echo ok;; ( * ) echo no;; esac\""));
    // ] first and - last are kept as members; a range still works
    TEST_ASSERT_EQUAL_STRING("rb dash m no\n",
        run("case ']' in ( []a] ) printf 'rb ';; ( * ) printf 'no ';; esac\n"
            "case '-' in ( [a\\\\-] ) printf 'dash ';; ( * ) printf 'no ';; esac\n"
            "case 'm' in ( [a-z] ) printf 'm ';; ( * ) printf 'no ';; esac\n"
            "case '-' in ( [a-z] ) echo yes;; ( * ) echo no;; esac"));
}

// An escaped backtick inside double quotes is a literal backtick; a bare one
// started a command substitution later ("a\`b\`c" ran b)
void test_escaped_backtick_in_double_quotes_is_literal(void) {
    TEST_ASSERT_EQUAL_STRING("[a`b`c] 5 [x`y`z] [\\`t`]\n",
        run("v=\"q\\`w\\`e\"; printf '[%s] %s ' \"a\\`b\\`c\" \"${#v}\"; eval 'printf \"[%s] \" \"x\\`y\\`z\"'; printf '[%s]\\n' \"\\\\\\`t\\`\""));
    // escaped quotes are marked too: a bare one flipped the quote tracking of
    // the command-substitution pass, which then ran what single quotes held
    TEST_ASSERT_EQUAL_STRING("[''`t`] [a\"b`c\"] ['p `q` r']\n",
        run("x=\\'\\''`t`'; printf '[%s] ' \"$x\" \"a\\\"b\\`c\\\"\"; printf '[%s]\\n' \\''p `q` r'\\'"));
}

// #388: $'...' is dollar-single-quotes (POSIX.1-2024, bash, ksh, zsh): C-style
// escapes decoded, the result quoted text. Inside double quotes it is a dollar
// sign and a quote. $$'bar' is the pid followed by bar ($$ is consumed as one
// parameter; the second $ used to be read again as a new expansion).
void test_dollar_single_quotes(void) {
    TEST_ASSERT_EQUAL_STRING("[bar][a\tb][AA\xc3\xa9][it's][\\z][][$'x']\n",
        run("printf '[%s]' $'bar' $'a\\tb' $'\\x41\\101\\u00e9' $'it\\'s' $'\\z' $'' \"$'x'\"; echo"));
    TEST_ASSERT_EQUAL_STRING("3 ok1 ok2 ok3\n",
        run("v=$'\\e\\cA\\n'; printf '%s ' \"${#v}\"; a=$$'bar'; [ \"$a\" = \"$$bar\" ] && printf 'ok1 '; "
            "b=$$$'bar'; [ \"$b\" = \"$$bar\" ] && printf 'ok2 '; [ \"$$x\" = \"$$\"x ] && echo ok3"));
    TEST_ASSERT_EQUAL_STRING("m lit\n",
        run("case 'a b' in ( $'a\\40b' ) printf 'm ';; ( * ) printf 'no ';; esac; case 'xyz' in ( $'*' ) echo bug;; ( * ) echo lit;; esac"));
    // a $'...' inside a command substitution and a function body (the lexer skips it as a unit, \' included)
    TEST_ASSERT_EQUAL_STRING("[q\tr] [it's]\n",
        run("v=$(printf '%s' $'q\\tr'); f() { printf '[%s]\\n' $'it\\'s'; }; printf '[%s] ' \"$v\"; f"));
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
    RUN_TEST(test_arith_substitutes_parameters_as_text);
    RUN_TEST(test_read_assigns_shell_variables);
    RUN_TEST(test_read_splits_on_ifs);
    RUN_TEST(test_read_handles_backslashes);
    RUN_TEST(test_prefix_assignments_follow_the_special_builtin_rule);
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
    RUN_TEST(test_heredoc_in_cmdsub_keeps_backslash_newline);
    RUN_TEST(test_heredoc_continued_line_keeps_tab_and_is_not_the_delimiter);
    RUN_TEST(test_single_quoted_backslash_escapes_nothing_after_the_quote);

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
    RUN_TEST(test_alias_takes_effect_for_later_programs);
    RUN_TEST(test_alias_in_function_body_is_stored_expanded);
    RUN_TEST(test_kill_l_translates_names_and_numbers);
    RUN_TEST(test_umask_shows_and_sets_the_mask);
    RUN_TEST(test_for_words_honour_noglob);
    RUN_TEST(test_read_reports_end_of_file);
    RUN_TEST(test_finished_background_jobs_make_room);
    RUN_TEST(test_unset_f_removes_the_function);
    RUN_TEST(test_case_word_with_control_bytes);
    RUN_TEST(test_command_does_not_expand_its_arguments_twice);
    RUN_TEST(test_command_keeps_quoted_operators_and_skips_functions);
    RUN_TEST(test_redirect_saves_a_closed_fd_before_opening_the_source);
    RUN_TEST(test_nested_expansion_with_quotes_in_a_word);
    RUN_TEST(test_trap_keeps_signals_ignored_on_entry);
    RUN_TEST(test_long_expansions_are_not_cut);
    RUN_TEST(test_bad_substitution_is_an_error);
    RUN_TEST(test_unquoted_star_and_at_are_fields);
    RUN_TEST(test_star_joins_where_nothing_splits);
    RUN_TEST(test_quoted_at_fields);
    RUN_TEST(test_escaped_backslash_and_dollar);
    RUN_TEST(test_bracket_expression_members);
    RUN_TEST(test_escaped_backtick_in_double_quotes_is_literal);
    RUN_TEST(test_dollar_single_quotes);
    return UNITY_END();
}
