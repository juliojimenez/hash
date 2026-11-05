#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

HASH_BIN="./hash-shell"
TEST_DIR="test_output"
PASSED=0
FAILED=0

# Portable timeout function
# Usage: run_with_timeout <seconds> <command> [args...]
run_with_timeout() {
    local timeout_duration="$1"
    shift

    # Check if GNU timeout is available (Linux, or macOS with coreutils)
    if command -v timeout &> /dev/null; then
        timeout "$timeout_duration" "$@"
        return $?
    # Check for gtimeout (macOS with Homebrew coreutils)
    elif command -v gtimeout &> /dev/null; then
        gtimeout "$timeout_duration" "$@"
        return $?
    else
        # Fallback: use perl (available on macOS by default)
        perl -e '
            use strict;
            use warnings;
            my $timeout = shift @ARGV;
            my $pid = fork();
            if ($pid == 0) {
                exec(@ARGV) or exit(127);
            }
            local $SIG{ALRM} = sub { kill("TERM", $pid); exit(124); };
            alarm($timeout);
            waitpid($pid, 0);
            alarm(0);
            exit($? >> 8);
        ' "$timeout_duration" "$@"
        return $?
    fi
}

setup() {
    echo "Setting up test environment..."
    mkdir -p "$TEST_DIR"

    # Build hash
    echo "Building hash..."
    make clean > /dev/null 2>&1
    make > /dev/null 2>&1

    if [[ ! -f "$HASH_BIN" ]]; then
        echo -e "${RED}FATAL: hash binary not found${NC}"
        exit 1
    fi

    echo -e "${GREEN}Setup complete${NC}\n"
    return 0
}

cleanup() {
    rm -rf "$TEST_DIR"
    return 0
}

run_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"

    echo -n "Testing: $test_name... "

    # Run command in hash shell and capture all output
    local output=$(printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" 2>&1)

    # Use grep -F for literal (fixed) string matching to avoid regex issues
    if echo "$output" | grep -qF "$expected"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected to find: $expected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    fi
}

run_command_test() {
    local test_name="$1"
    local command="$2"

    echo -n "Testing: $test_name... "

    # Run command, check exit code
    if printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Command failed or timed out"
        ((FAILED++))
        return 1
    fi
}

# Like run_test, but through the interactive prompt loop (-i), the path a real
# terminal session takes. HOME and HISTFILE point at the test directory so the
# run neither loads ~/.hashrc nor touches the real history file.
run_interactive_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"

    echo -n "Testing: $test_name... "

    local output=$(printf "%s\nexit\n" "$command" | \
        HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
        HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)

    if echo "$output" | grep -qF "$expected"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected to find: $expected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    fi
}

# Interactive variant that passes when the string is ABSENT from the output
run_interactive_test_absent() {
    local test_name="$1"
    local command="$2"
    local unexpected="$3"

    echo -n "Testing: $test_name... "

    local output=$(printf "%s\nexit\n" "$command" | \
        HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
        HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)

    if echo "$output" | grep -qF "$unexpected"; then
        echo -e "${RED}FAIL${NC}"
        echo "  Did not expect to find: $unexpected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    else
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    fi
}

run_file_test() {
    local test_name="$1"
    local command="$2"
    local output_file="$3"
    local expected_content="$4"

    echo -n "Testing: $test_name... "

    # Run command
    printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" > /dev/null 2>&1

    if [[ -f "$output_file" ]] && grep -q "$expected_content" "$output_file"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        rm -f "$output_file"
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  File not found or content mismatch"
        if [[ -f "$output_file" ]]; then
            echo "  File exists, content:"
            cat "$output_file"
        else
            echo "  File does not exist: $output_file"
        fi
        ((FAILED++))
        return 1
    fi
}

echo -e "${YELLOW}=== Hash Shell Test Suite ===${NC}\n"

setup

echo -e "${YELLOW}Basic Commands:${NC}"
run_test "echo command" "echo hello" "hello"
run_test "pwd command" "pwd" "/"
run_command_test "ls command" "ls"
run_command_test "date command" "date"

echo -e "\n${YELLOW}Built-in Commands:${NC}"
run_command_test "cd to /tmp" "cd /tmp"
run_command_test "cd to home (with tilde)" "cd ~"
run_command_test "cd to home (no args)" "cd"
run_command_test "exit command" "exit"

echo -e "\n${YELLOW}Error Handling:${NC}"
run_test "invalid command" "this_command_does_not_exist_12345" "No such file or directory"

echo -e "\n${YELLOW}Edge Cases:${NC}"
run_command_test "empty command" ""
run_command_test "whitespace only" "   "

echo -e "\n${YELLOW}Command Substitution:${NC}"
run_test "basic command substitution" 'echo $(echo hello)' "hello"
run_test "command substitution with pwd" 'echo $(pwd)' "/"
run_test "command substitution in string" 'echo "prefix-$(echo middle)-suffix"' "prefix-middle-suffix"
run_test "backtick substitution" 'echo `echo backtick`' "backtick"

echo -e "\n${YELLOW}Quote Handling:${NC}"
run_test "double quotes" "echo \"hello world\"" "hello world"
run_test "single quotes" "echo 'hello world'" "hello world"
run_test "escaped double quote" "echo \"He said \\\"hello\\\"\"" 'He said "hello"'
run_test "escaped backslash" "echo \"path\\\\to\\\\file\"" "path\\to\\file"
run_test "mixed quotes" "echo \"double\" 'single'" "double"

echo -e "\n${YELLOW}Alias Functionality:${NC}"
run_command_test "create alias" "alias testcmd='echo test works'"
run_test "use alias" "alias testcmd='echo aliasworks' && testcmd" "aliasworks"
run_test "alias with args" "alias ll='ls -la' && ll ." "total"
run_command_test "list aliases" "alias"
run_command_test "remove alias" "unalias testcmd"

echo -e "\n${YELLOW}File Operations:${NC}"
run_file_test "create file with echo" "echo test_content > $TEST_DIR/testfile.txt" "$TEST_DIR/testfile.txt" "test_content"

echo -e "\n${YELLOW}Interactive Prompt (-i):${NC}"
run_interactive_test "simple command" 'echo plain-ok' "plain-ok"
run_interactive_test "one-line for loop" 'for i in 1 2; do echo loop$i; done' "loop2"
run_interactive_test "one-line if" 'if true; then echo branch-yes; fi' "branch-yes"
run_interactive_test "multi-line for loop" $'for i in 1 2\ndo\necho multi$i\ndone' "multi2"
run_interactive_test "while loop" 'n=0; while [ $n -lt 2 ]; do n=$((n+1)); done; echo while$n' "while2"
run_interactive_test "case statement" 'case x in x) echo case-x;; esac' "case-x"
run_interactive_test "function definition and call" 'hi() { echo fn-called; }; hi' "fn-called"
run_interactive_test "brace group" '{ echo brace-a; echo brace-b; }' "brace-b"
run_interactive_test "exit code after loop" 'for i in 1; do false; done; echo ec=$?' "ec=1"
run_interactive_test "loop inside skipped branch keeps shell alive" $'if false\nthen\nfor i in 1 2\ndo\necho skipped\ndone\nfi\necho still-alive' "still-alive"
run_interactive_test "stray semicolon keeps shell alive" $';\necho still-alive' "still-alive"
run_interactive_test "one-line loop inside skipped branch" 'if false; then for i in 1 2; do echo skipped; done; fi; echo still-alive' "still-alive"
run_interactive_test "one-line loop inside taken branch" 'if true; then for i in 1 2; do echo taken$i; done; fi' "taken2"
run_interactive_test "incomplete eval keeps shell alive" $'eval \'if true\'\necho still-alive' "still-alive"
run_interactive_test_absent "semicolon inside comment is not a separator" 'echo visible # comment; echo should-not-run' "should-not-run"
run_interactive_test_absent "comment right after a semicolon" 'echo visible;# comment; echo should-not-run' "should-not-run"
run_interactive_test "syntax error inside if does not stick the prompt" $'if false; then done; fi\necho still-alive' "still-alive"
run_interactive_test_absent "function in skipped branch is not defined" 'f() { echo original; }; if false; then f() { echo replaced; }; fi; f' "replaced"
run_interactive_test "function in taken branch is defined" 'if true; then g() { echo defined; }; fi; g' "defined"
run_interactive_test_absent "command after function in skipped branch does not run" 'if false; then f() { :; } && echo leaked; fi; echo after' "leaked"
run_interactive_test "escaped semicolon before # is not a comment" 'echo first; echo foo\;#bar; echo after' "after"
run_interactive_test "escaped semicolon is literal" 'echo foo\;#bar' "foo;#bar"

# The legacy engine does not join continued lines at the prompt (and hangs on
# them in stdin mode), so this case pins the AST engine
HASH_ENGINE=ast run_interactive_test "line continuation across prompt lines (ast engine)" $'printf \'%s|\' a \\\n  b c; echo' "a|b|c|"

# exit N at the prompt, and end of input, leave the shell with the last status
echo -n "Testing: exit status at the prompt... "
printf 'exit 3\n' | HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i >/dev/null 2>&1; ec_exit=$?
printf 'false\n' | HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i >/dev/null 2>&1; ec_eof=$?
if [[ "$ec_exit" == "3" && "$ec_eof" == "1" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  exit 3 gave $ec_exit (want 3); end of input after false gave $ec_eof (want 1)"; ((FAILED++))
fi

# A script on standard input shares that input with the commands it runs: a
# redirection of descriptor 0 must not eat the lines after it, and a command
# can read what follows it in the script
echo -n "Testing: piped script keeps running after a stdin redirect... "
echo one > "$TEST_DIR/stdin_redirect.txt"
redirect_out=$(printf 'cat < %s\necho after-redirect\nexit 4\n' "$TEST_DIR/stdin_redirect.txt" | run_with_timeout 2 "$HASH_BIN" 2>&1); redirect_ec=$?
if [[ "$redirect_out" == $'one\nafter-redirect' && "$redirect_ec" == "4" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected: one, after-redirect, exit 4"; echo "  Got (exit $redirect_ec):"; echo "$redirect_out" | head -5; ((FAILED++))
fi

echo -n "Testing: command reads the rest of the piped script... "
share_out=$(printf 'cat\nhello from the input\n' | run_with_timeout 2 "$HASH_BIN" 2>&1)
if [[ "$share_out" == "hello from the input" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected: hello from the input"; echo "  Got:"; echo "$share_out" | head -5; ((FAILED++))
fi

# Blank lines typed inside an open compound command must survive into the body
echo -n "Testing: blank line inside loop heredoc is kept... "
heredoc_lines=$(printf 'for i in 1\ndo\ncat <<EOF\nline1\n\nline3\nEOF\ndone\nexit\n' | \
    HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
    HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>/dev/null | wc -l | tr -d ' ')
if [[ "$heredoc_lines" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected 3 output lines, got $heredoc_lines"; ((FAILED++))
fi

# ~/.hashrc is a shell script: functions, conditionals and the hash options all
# take effect at startup, and none of its lines land in the history
echo -n "Testing: .hashrc functions and conditionals take effect... "
mkdir -p "$TEST_DIR/rchome"
printf '%s\n' "alias rc_alias='echo aliased'" 'rc_fn() {' '  echo "fn $1"' '}' 'if true; then' '  RC_IF=yes' 'fi' 'set welcome=off' > "$TEST_DIR/rchome/.hashrc"
rc_out=$(printf 'rc_fn one\necho "if=$RC_IF"\nrc_alias\nexit\n' | HOME="$PWD/$TEST_DIR/rchome" HISTFILE="$PWD/$TEST_DIR/rchome/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rc_out" | grep -qF "fn one" && echo "$rc_out" | grep -qF "if=yes" && echo "$rc_out" | grep -qF "aliased" && ! grep -qs 'rc_fn() {' "$TEST_DIR/rchome/hash_history"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rc_out" | head -10; echo "  History:"; cat "$TEST_DIR/rchome/hash_history" 2>/dev/null | head -5; ((FAILED++))
fi

# exit in .hashrc ends the shell before the prompt, with that status
echo -n "Testing: exit in .hashrc ends the shell with its status... "
mkdir -p "$TEST_DIR/rcexit"
printf 'exit 7\n' > "$TEST_DIR/rcexit/.hashrc"
rcx_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rcexit" HISTFILE="$PWD/$TEST_DIR/rcexit/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1); rcx_ec=$?
if [[ "$rcx_ec" == "7" ]] && ! echo "$rcx_out" | grep -qF "alive"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 7 and no prompt; got exit $rcx_ec:"; echo "$rcx_out" | head -5; ((FAILED++))
fi

# exit in a sourced file exits the shell
echo -n "Testing: exit in a sourced file exits the shell... "
printf 'exit 5\necho after-exit\n' > "$TEST_DIR/exit5.sh"
src_out=$(run_with_timeout 2 "$HASH_BIN" -c "source $TEST_DIR/exit5.sh; echo after-source" 2>&1); src_ec=$?
if [[ "$src_ec" == "5" && -z "$src_out" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 5 and no output; got exit $src_ec: $src_out"; ((FAILED++))
fi

# The shell removes the quotes around an alias value; quotes inside it stay
run_test "alias keeps the quotes inside its value" $'alias greet="\'printf\' \'%s\\n\' \'hello world\'"; greet' "hello world"

# return in .hashrc ends the file, and only the file
echo -n "Testing: return in .hashrc ends only the file... "
mkdir -p "$TEST_DIR/rcreturn"
printf 'RC=1\nreturn\nRC=2\n' > "$TEST_DIR/rcreturn/.hashrc"
rcr_out=$(printf 'echo "rc=$RC"\nif true; then echo if-ran; fi\nexit\n' | HOME="$PWD/$TEST_DIR/rcreturn" HISTFILE="$PWD/$TEST_DIR/rcreturn/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rcr_out" | grep -qF "rc=1" && echo "$rcr_out" | grep -qF "if-ran"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rcr_out" | head -5; ((FAILED++))
fi

# source inside an EXIT trap does not cut the trap short
echo -n "Testing: source inside an EXIT trap runs the rest of the trap... "
printf 'true\n' > "$TEST_DIR/true.sh"
printf "trap 'source %s; echo finished' EXIT\nexit 3\n" "$TEST_DIR/true.sh" > "$TEST_DIR/trap_source.sh"
trap_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/trap_source.sh" 2>&1)
if echo "$trap_out" | grep -qF "finished"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected 'finished'; got: $trap_out"; ((FAILED++))
fi

# A login shell keeps the requested exit status across its logout file, unless
# the logout file exits itself
echo -n "Testing: logout file keeps the exit status unless it exits... "
mkdir -p "$TEST_DIR/rclogin"
printf 'exit 7\n' > "$TEST_DIR/rclogin/.hashrc"
printf 'true\n' > "$TEST_DIR/rclogin/.hash_logout"
printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogin" HISTFILE="$PWD/$TEST_DIR/rclogin/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i >/dev/null 2>&1; lo_keep=$?
printf 'exit 3\n' > "$TEST_DIR/rclogin/.hash_logout"
printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogin" HISTFILE="$PWD/$TEST_DIR/rclogin/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i >/dev/null 2>&1; lo_exit=$?
if [[ "$lo_keep" == "7" && "$lo_exit" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  logout 'true' gave $lo_keep (want 7); logout 'exit 3' gave $lo_exit (want 3)"; ((FAILED++))
fi

# A file sourced from .hashrc does not end .hashrc: a return after it still works
echo -n "Testing: return after a nested source in .hashrc... "
mkdir -p "$TEST_DIR/rcnested"
printf 'S=1\n' > "$TEST_DIR/rcnested/settings.sh"
printf 'source %s\nreturn\necho not-reached\n' "$TEST_DIR/rcnested/settings.sh" > "$TEST_DIR/rcnested/.hashrc"
rcn_out=$(printf 'echo "s=$S"\nexit\n' | HOME="$PWD/$TEST_DIR/rcnested" HISTFILE="$PWD/$TEST_DIR/rcnested/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rcn_out" | grep -qF "s=1" && ! echo "$rcn_out" | grep -qF "not-reached" && ! echo "$rcn_out" | grep -qF "can only"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rcn_out" | head -5; ((FAILED++))
fi

# logout in a login shell's .hashrc ends the shell before the prompt
echo -n "Testing: logout in .hashrc of a login shell skips the prompt... "
mkdir -p "$TEST_DIR/rclogout"
printf 'logout\necho rc-after\n' > "$TEST_DIR/rclogout/.hashrc"
lo_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogout" HISTFILE="$PWD/$TEST_DIR/rclogout/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i 2>&1); lo_ec=$?
if [[ "$lo_ec" == "0" ]] && ! echo "$lo_out" | grep -qE "alive|rc-after"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 0 with no prompt; got exit $lo_ec: $lo_out"; ((FAILED++))
fi

# exit inside a condition in .hashrc still ends the shell with its status
echo -n "Testing: exit inside an if condition in .hashrc ends the shell... "
mkdir -p "$TEST_DIR/rcifexit"
printf 'if exit 7; then\n:\nfi\necho after\n' > "$TEST_DIR/rcifexit/.hashrc"
ife_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rcifexit" HISTFILE="$PWD/$TEST_DIR/rcifexit/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1); ife_ec=$?
if [[ "$ife_ec" == "7" ]] && ! echo "$ife_out" | grep -qE "alive|after"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 7 and no output; got exit $ife_ec: $ife_out"; ((FAILED++))
fi

cleanup

echo -e "\n${YELLOW}=== Test Summary ===${NC}"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo -e "Total:  $((PASSED + FAILED))"

if [[ $FAILED -eq 0 ]]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
