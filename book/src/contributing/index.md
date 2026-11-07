# Contributing Guide

Thank you for your interest in contributing to hash!

## Getting Started

### Prerequisites

- GCC or Clang compiler
- Make build system
- Git

### Fork and Clone

```bash
git clone https://github.com/YOUR_USERNAME/hash.git
cd hash
git remote add upstream https://github.com/juliojimenez/hash.git
```

### Build

```bash
make clean
make
./hash-shell
```

### Debug Build

```bash
make debug
```

## Project Structure

```
hash/
├── src/                    # Source files
│   ├── main.c              # Entry point
│   ├── hash.h              # Main header
│   ├── parser.c/h          # Command parsing
│   ├── execute.c/h         # Execution
│   ├── builtins.c/h        # Built-in commands
│   ├── history.c/h         # Command history
│   ├── completion.c/h      # Tab completion
│   ├── lineedit.c/h        # Line editing
│   ├── prompt.c/h          # Prompt generation
│   └── ...
├── tests/                  # Unit tests
├── docs/                   # Documentation
├── book/                   # Book of Hash source
└── Makefile
```

## Coding Standards

### Style

- 4 spaces indentation (no tabs)
- Functions: `snake_case` with module prefix
- Constants: `UPPER_SNAKE_CASE`
- Max line length: 100 characters

```c
int example_function(char *arg) {
    if (arg == NULL) {
        return -1;
    }
    return 0;
}
```

### Safe String Functions

Always use safe string functions:

```c
// Good
safe_strcpy(dst, src, sizeof(dst));
safe_strcat(dst, src, sizeof(dst));

// Avoid
strcpy(dst, src);   // No bounds checking
strcat(dst, src);   // No bounds checking
```

### Compiler Warnings

Code must compile cleanly:

```bash
gcc -Wall -Wextra -Werror -std=gnu99
```

## Testing

### Setup Test Framework

```bash
make test-setup
```

### Run Tests

```bash
make test        # Unit tests
./test.sh        # Integration tests
```

### Writing Tests

Tests use Unity framework:

```c
#include "unity.h"

void test_feature_works(void) {
    int result = function("input");
    TEST_ASSERT_EQUAL_INT(0, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_feature_works);
    return UNITY_END();
}
```

## Submitting Changes

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation

### Commit Messages

```
type(scope): description

Types: feat, fix, docs, style, refactor, test, chore
```

Examples:
```
feat(completion): add environment variable completion
fix(history): prevent duplicate entries
docs(readme): update installation instructions
```

### Before Submitting

```bash
# Sync with upstream
git fetch upstream
git rebase upstream/main

# Run tests
make clean && make && make test
./test.sh

# Check warnings
make CFLAGS="-Wall -Wextra -Werror -std=gnu99"
```

### Pull Request

1. Push branch to your fork
2. Open PR against `juliojimenez/hash:main`
3. Fill out PR template
4. Address review feedback

## Getting Help

- **Questions**: [GitHub Discussions](https://github.com/juliojimenez/hash/discussions)
- **Bugs**: [GitHub Issues](https://github.com/juliojimenez/hash/issues)
- **Email**: julio@julioj.com
