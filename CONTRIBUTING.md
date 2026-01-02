# Contributing to Hash Shell

Thank you for your interest in contributing to hash! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Project Structure](#project-structure)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Issue Guidelines](#issue-guidelines)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to julio@julioj.com.

## Getting Started

### Prerequisites

- GCC or Clang compiler
- Make build system
- Git
- Linux environment (native or WSL)

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork locally:

```bash
git clone https://github.com/YOUR_USERNAME/hash.git
cd hash
```

3. Add the upstream remote:

```bash
git remote add upstream https://github.com/juliojimenez/hash.git
```

## Development Setup

### Building

```bash
# Clean build
make clean
make

# Debug build (with symbols)
make debug

# Run the shell
./hash-shell
```

### Running Tests

```bash
# Setup test framework (first time only)
make test-setup

# Run unit tests
make test

# Run integration tests
./test.sh
```

### Cleaning Up

```bash
# Remove build artifacts
make clean

# Remove test artifacts
make test-clean
```

## Project Structure

```
hash/
├── src/                    # Source files
│   ├── main.c              # Entry point
│   ├── hash.h              # Main header with constants
│   ├── parser.c/h          # Command line parsing
│   ├── execute.c/h         # Command execution
│   ├── builtins.c/h        # Built-in commands (cd, exit, etc.)
│   ├── config.c/h          # Configuration and aliases
│   ├── history.c/h         # Command history
│   ├── completion.c/h      # Tab completion
│   ├── lineedit.c/h        # Line editing (readline-like)
│   ├── prompt.c/h          # Prompt generation
│   ├── pipeline.c/h        # Pipe handling
│   ├── chain.c/h           # Command chaining (&&, ||, ;)
│   ├── redirect.c/h        # I/O redirection
│   ├── expand.c/h          # Tilde expansion
│   ├── varexpand.c/h       # Variable expansion
│   ├── colors.c/h          # ANSI color support
│   └── safe_string.c/h     # Safe string utilities
├── tests/                  # Unit tests
│   ├── test_*.c            # Test files
│   ├── setup_unity.sh      # Unity framework setup
│   └── unity/              # Unity test framework (auto-downloaded)
├── docs/                   # Documentation
├── examples/               # Example configurations
├── .github/                # GitHub Actions workflows
├── Makefile                # Build configuration
├── test.sh                 # Integration test script
└── README.md               # Project overview
```

## Coding Standards

### C Style Guidelines

**Indentation and Formatting:**
- Use 4 spaces for indentation (no tabs)
- Opening braces on the same line for functions and control structures
- Maximum line length: 100 characters
- No trailing whitespace

**Example:**
```c
int example_function(char *arg) {
    if (arg == NULL) {
        return -1;
    }

    for (int i = 0; i < 10; i++) {
        // Process
    }

    return 0;
}
```

**Naming Conventions:**
- Functions: `snake_case` with module prefix (e.g., `history_add`, `config_load`)
- Variables: `snake_case`
- Constants/Macros: `UPPER_SNAKE_CASE`
- Type definitions: `PascalCase`

**Comments:**
- Use `//` for single-line comments
- Use `/* */` for multi-line comments and file headers
- Document all public functions in header files

### Security Practices

**Always use safe string functions:**
```c
// Good
safe_strcpy(dst, src, sizeof(dst));
safe_strcat(dst, src, sizeof(dst));
size_t len = safe_strlen(str, MAX_LENGTH);

// Avoid
strcpy(dst, src);   // No bounds checking
strcat(dst, src);   // No bounds checking
strlen(str);        // May read past buffer
```

**Use reentrant functions for thread safety:**
```c
// Good
struct passwd pw;
struct passwd *result = NULL;
char buf[1024];
getpwuid_r(uid, &pw, buf, sizeof(buf), &result);

// Avoid
struct passwd *pw = getpwuid(uid);  // Not thread-safe
```

**Always null-terminate strings:**
```c
char buffer[SIZE];
// After any operation that may not null-terminate
buffer[SIZE - 1] = '\0';
```

### Static Analysis

All code must pass SonarQube analysis. Common issues to avoid:
- Buffer overflows
- Unbounded string operations
- Memory leaks
- Use of deprecated/unsafe functions
- Uninitialized variables

### Compiler Warnings

Code must compile cleanly with:
```bash
gcc -Wall -Wextra -Werror -std=gnu99
```

## Testing

### Unit Tests

Unit tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) framework.

**Adding a new test file:**

1. Create `tests/test_module.c`:

```c
#include "unity.h"
#include "../src/module.h"

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

void test_feature_works(void) {
    int result = module_function("input");
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_handles_null(void) {
    int result = module_function(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_feature_works);
    RUN_TEST(test_handles_null);
    return UNITY_END();
}
```

2. Run tests:
```bash
make test
```

**Common assertions:**
- `TEST_ASSERT_EQUAL_INT(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_NOT_NULL(pointer)`
- `TEST_ASSERT_NULL(pointer)`
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`

### Integration Tests

Add integration tests to `test.sh`:

```bash
run_test "description" "command" "expected_output"
run_command_test "description" "command"
run_file_test "description" "command" "output_file" "expected_content"
```

### Test Coverage

When adding new features:
- Write unit tests for all public functions
- Write integration tests for user-facing functionality
- Test edge cases (NULL inputs, empty strings, boundary conditions)
- Test error handling paths

## Submitting Changes

### Branch Naming

Use descriptive branch names:
- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation changes
- `refactor/description` - Code refactoring

### Commit Messages

Follow conventional commit format:

```
type(scope): short description

Longer description if needed.

Fixes #123
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation
- `style` - Formatting (no code change)
- `refactor` - Code restructuring
- `test` - Adding tests
- `chore` - Maintenance tasks

**Examples:**
```
feat(completion): add environment variable completion

fix(history): prevent duplicate entries when using erasedups

docs(readme): update installation instructions

test(parser): add tests for escaped quotes
```

### Before Submitting

1. **Sync with upstream:**
```bash
git fetch upstream
git rebase upstream/main
```

2. **Run all tests:**
```bash
make clean
make
make test
./test.sh
```

3. **Check for warnings:**
```bash
make clean
make CFLAGS="-Wall -Wextra -Werror -std=gnu99"
```

4. **Check for trailing whitespace:**
```bash
git grep -n '[[:space:]]$' -- '*.c' '*.h'
```

## Issue Guidelines

### Bug Reports

Use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) and include:

- Clear description of the bug
- Steps to reproduce
- Expected behavior
- Actual behavior
- Hash version and OS information
- Relevant screenshots or logs

### Feature Requests

When proposing new features:

- Describe the use case
- Explain how it would work
- Consider compatibility with existing features
- Reference similar functionality in bash/zsh if applicable

## Pull Request Process

### Creating a Pull Request

1. Push your branch to your fork
2. Open a PR against `juliojimenez/hash:main`
3. Fill out the PR template with:
   - Description of changes
   - Related issue numbers
   - Testing performed
   - Screenshots (if UI changes)

### Review Process

- All PRs require review before merging
- Address reviewer feedback promptly
- Keep PRs focused and reasonably sized
- Large changes should be discussed in an issue first

### Merge Requirements

Before merging, PRs must:
- Pass all CI checks (build, tests, security scan)
- Have no unresolved conversations
- Be up-to-date with the main branch
- Follow coding standards

## Documentation

When adding features, update relevant documentation:

- Add/update docs in `docs/` directory
- Update `README.md` if adding major features
- Add examples in `examples/` if helpful
- Include inline comments for complex logic

### Documentation Style

- Use Markdown formatting
- Include code examples
- Show both usage and expected output
- Link to related documentation

## Getting Help

- **Questions:** Open a [GitHub Discussion](https://github.com/juliojimenez/hash/discussions)
- **Bugs:** Open an [issue](https://github.com/juliojimenez/hash/issues)
- **Email:** julio@julioj.com

## Recognition

Contributors are recognized in release notes and the project's contributor list. Thank you for helping make hash better!

---

Back to [README](README.md)
