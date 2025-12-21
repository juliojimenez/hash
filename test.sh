RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

HASH_BIN="./hash"
TEST_DIR="test_output"
PASSED=0
FAILED=0

setup() {
    echo "Setting up test environment..."
    mkdir -p "$TEST_DIR"

    # Build hash
    echo "Building hash..."
    make clean > /dev/null 2>&1
    make > /dev/null 2>&1

    if [ ! -f "$HASH_BIN" ]; then
        echo -e "${RED}FATAL: hash binary not found${NC}"
        exit 1
    fi

    echo -e "${GREEN}Setup complete${NC}\n"
}

cleanup() {
    rm -rf "$TEST_DIR"
}

run_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"

    echo -n "Testing: $test_name... "

    # Run command in hash shell
    local output=$(echo "$command\nexit" | timeout 2 "$HASH_BIN" 2>&1)

    if echo "$output" | grep -q "$expected"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected: $expected"
        echo "  Got: $output"
        ((FAILED++))
        return 1
    fi
}

run_command_test() {
    local test_name="$1"
    local command="$2"

    echo -n "Testing: $test_name... "

    # Run command, check exit code
    if echo -e "$command\nexit" | timeout 2 "$HASH_BIN" > /dev/null 2>&1; then
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
    echo -e "$command\nexit" | timeout 2 "$HASH_BIN" > /dev/null 2>&1

    if [ -f "$output_file" ] && grep -q "$expected_content" "$output_file"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        rm -f "$output_file"
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  File not found or content mismatch"
        ((FAILED++))
        return 1
    fi
}

echo -e "${YELLOW}=== Hash Shell Test Suite ===${NC}\n"

setup

echo -e "${YELLOW}Basic Commands:${NC}"
run_test "echo command" "echo hello"
# TODO: command substitution - $(pwd)
run_test "pwd command" "pwd"
run_command_test "ls command" "ls"
run_command_test "date command" "date"

echo -e "\n${YELLOW}Built-in Commands:${NC}"
run_command_test "cd to /tmp" "cd /tmp"
# TODO: recognize ~ as home
run_command_test "cd to home" "cd ~"
run_command_test "exit command" "exit"

echo -e "\n${YELLOW}Error Handling:${NC}"
run_test "invalid command" "this_command_does_not_exist_12345" "No such file or directory"
run_test "cd with no args" "cd" "expected argument"

echo -e "\n${YELLOW}Edge Cases:${NC}"
run_command_test "empty command" ""
run_command_test "whitespace only" "   "

echo -e "\n${YELLOW}File Operations:${NC}"
#run_file_test "create file with echo" "echo test_content > $TEST_DIR/testfile.txt" "$TEST_DIR/testfile.txt" "test_content"

cleanup

echo -e "\n${YELLOW}=== Test Summary ===${NC}"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo -e "Total:  $((PASSED + FAILED))"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
