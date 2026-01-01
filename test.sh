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
