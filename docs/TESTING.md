# Testing Hash Shell

## Overview

Hash shell has two types of tests:

1. **Integration tests** (`test.sh`) - Test the complete shell end-to-end
2. **Unit tests** (`tests/test_*.c`) - Test individual functions in isolation

## Unit Tests

### Setup

Unit tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) test framework. Set it up once:

```bash
make test-setup
```

This downloads Unity v2.6.0 into `tests/unity/`.

### Running Unit Tests

Run all unit tests:

```bash
make test
```

This will:
1. Build all test files
2. Run each test suite
3. Report pass/fail results
4. Exit with code 0 on success, 1 on failure

### Test Structure

Unit tests are organized by module:

```
tests/
├── test_parser.c     # Tests for parser.c
├── test_builtins.c   # Tests for builtins.c
├── test_execute.c    # Tests for execute.c
├── setup_unity.sh    # Script to download Unity
└── unity/            # Unity test framework (auto-downloaded)
```

### What's Tested

**Parser Tests** (`test_parser.c`):
- ✅ Simple command parsing
- ✅ Multiple arguments
- ✅ Extra whitespace handling
- ✅ Empty strings
- ✅ Tab characters
- ✅ Newline handling

**Builtins Tests** (`test_builtins.c`):
- ✅ `cd` to valid directory
- ✅ `cd` with no arguments
- ✅ `cd` to invalid directory
- ✅ `exit` command
- ✅ `try_builtin()` with builtin commands
- ✅ `try_builtin()` with non-builtin commands
- ✅ NULL argument handling

**Execute Tests** (`test_execute.c`):
- ✅ NULL args handling
- ✅ Builtin command execution
- ✅ External command execution
- ✅ Commands with arguments
- ✅ Invalid command handling

### Adding New Unit Tests

1. Create a new test file: `tests/test_module.c`
2. Include Unity and your module:

```c
#include "unity.h"
#include "../src/module.h"

void setUp(void) { }
void tearDown(void) { }

void test_something(void) {
    TEST_ASSERT_EQUAL_INT(expected, actual);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}
```

3. Run `make test` - new tests are auto-discovered!

### Unity Assertions

Common assertions used:
- `TEST_ASSERT_EQUAL_INT(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_NOT_NULL(pointer)`
- `TEST_ASSERT_NULL(pointer)`
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`

See [Unity documentation](https://github.com/ThrowTheSwitch/Unity) for more.

### Cleaning Up

Remove test build artifacts:

```bash
make test-clean
```

Remove everything including Unity framework:

```bash
make test-clean
rm -rf tests/unity
```

## Integration Tests

### Running Tests Locally

Make the test script executable and run it:

```bash
chmod +x test.sh
./test.sh
```

The test script will:
1. Build the hash shell
2. Run a series of functional tests
3. Report pass/fail results
4. Exit with code 0 on success, 1 on failure

### Manual Testing

Build and run hash interactively:

```bash
make
./hash
```

Test basic commands:
```
#> echo hello world
#> ls -la
#> pwd
#> cd /tmp
#> pwd
#> exit
```

## GitHub Actions

### Workflow Features

The GitHub Actions workflow runs **both** integration and unit tests:

**Build and Test Job:**
- Tests on multiple Ubuntu versions (20.04, 22.04, latest)
- Tests with both GCC and Clang compilers
- Runs integration tests (`./test.sh`)
- Runs unit tests (`make test`)
- Tests different optimization levels (-O0 with debug, -O2)

**Build Validation Job:**
- Treats all warnings as errors (`-Werror`)
- Checks for trailing whitespace
- Verifies clean rebuild works

**Security Scan Job:**
- Builds with security hardening flags
- Checks for unsafe C functions (gets, strcpy, etc.)
- Uses stack protection and fortify source

### Viewing Results

1. Go to GitHub repository
2. Click the "Actions" tab
3. See test results for each push/PR
4. Click on a specific run to see detailed output for each test

### Workflow Triggers

Tests run automatically on:
- Push to `main` branch
- Pull requests to `main`
- Manual trigger (workflow_dispatch)

## Complete Test Workflow

Here's a typical development workflow with tests:

```bash
# 1. Make changes to code
vim src/parser.c

# 2. Run unit tests (fast, isolated)
make test

# 3. Run integration tests (slower, end-to-end)
./test.sh

# 4. If all pass, commit
git add src/parser.c
git commit -m "Update parser"
git push

# 5. GitHub Actions runs both test suites automatically
```

## Adding New Tests

Edit `test.sh` and add new test cases:

```bash
# Add to the appropriate section
run_test "my new test" "command" "expected_output"
```

Test helper functions:
- `run_test` - Check command output contains expected string
- `run_command_test` - Check command runs without error
- `run_file_test` - Check file creation/content

## Test Coverage

### Unit Test Coverage
**Parser Module:**
- ✅ Simple command parsing
- ✅ Multiple arguments
- ✅ Whitespace handling (spaces, tabs, newlines)
- ✅ Empty input
- ✅ Double quotes
- ✅ Single quotes
- ✅ Mixed quotes
- ✅ Escaped double quotes (`\"`)
- ✅ Escaped single quotes (`\'`)
- ✅ Escaped backslashes (`\\`)
- ✅ Escape sequences (`\n`, `\t`, `\r`)
- ✅ Literal backslashes in single quotes
- ✅ Empty quoted strings
- ✅ Edge cases

**Builtins Module:**
- ✅ `cd` command (valid/invalid directories)
- ✅ `exit` command
- ✅ Error handling
- ✅ NULL argument handling

**Execute Module:**
- ✅ Builtin command execution
- ✅ External command execution
- ✅ Invalid command handling
- ✅ NULL argument handling

### Integration Test Coverage
- ✅ Basic command execution (echo, pwd, ls, date)
- ✅ Built-in commands (cd, exit)
- ✅ Error handling (invalid commands)
- ✅ Edge cases (empty commands, whitespace)
- ✅ Quote handling (double, single, mixed)
- ✅ Escape sequences in quotes
- ✅ File operations (redirection)

### Future Test Ideas

**Unit Tests:**
- Memory leak detection with valgrind
- Stress testing with many arguments
- Unicode/special character handling
- Signal handling tests

**Integration Tests:**
- Pipes (`ls | grep txt`)
- Background processes (`sleep 10 &`)
- Signal handling (Ctrl+C)
- Environment variables
- Command history

## Best Practices

1. **Run unit tests first** - They're fast and catch issues early
2. **Run integration tests before committing** - Ensure end-to-end functionality
3. **Write tests for new features** - Both unit and integration tests
4. **Keep tests independent** - Each test should run in isolation
5. **Use descriptive test names** - `test_parse_line_with_tabs` not `test1`

Back to [README](../README.md)
