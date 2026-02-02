# Testing

Hash uses the Unity test framework for unit tests and shell scripts for integration tests.

## Setup

Download the Unity framework (first time only):

```bash
make test-setup
```

## Running Tests

### Unit Tests

```bash
make test
```

### Integration Tests

```bash
./test.sh
```

### Clean Test Artifacts

```bash
make test-clean
```

## Writing Unit Tests

### Test File Structure

Create `tests/test_module.c`:

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

### Common Assertions

```c
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_STRING(expected, actual)
TEST_ASSERT_NOT_NULL(pointer)
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
```

## Integration Tests

Add tests to `test.sh`:

```bash
run_test "description" "command" "expected_output"
run_command_test "description" "command"
run_file_test "description" "command" "file" "expected"
```

## Sanitizer Tests

### AddressSanitizer

```bash
make clean
make CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer -std=gnu99"
make test-clean
make test CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer -std=gnu99"
```

### UndefinedBehaviorSanitizer

```bash
make clean
make CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=undefined -fno-omit-frame-pointer -std=gnu99"
make test-clean
make test CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=undefined -fno-omit-frame-pointer -std=gnu99"
```

### Combined (Most Thorough)

```bash
make clean
make CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=gnu99"
make test-clean
make test CC=clang CFLAGS="-Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=gnu99"
```

## Test Coverage Guidelines

When adding features:

- Write unit tests for all public functions
- Write integration tests for user-facing features
- Test edge cases (NULL, empty strings, boundaries)
- Test error handling paths
