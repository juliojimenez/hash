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
