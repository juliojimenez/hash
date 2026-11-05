# Testing

Hash uses the Unity test framework for unit tests, a shell script for integration tests, and a small Python driver that types at the shell in a pseudo-terminal for interactive tests.

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

### Interactive Tests

```bash
python3 tests/interactive/run.py
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

## Interactive Tests

`tests/interactive/run.py` runs the shell in a pseudo-terminal and types at it, so the line editor, the continuation prompt, Ctrl-C, Ctrl-R, the startup files and the history file are tested the way a user meets them. Piping into `hash-shell -i` cannot reach those paths.

Each case is a text file in `tests/interactive/cases/`, one directive per line:

```
# Ctrl-C at the continuation prompt abandons the unfinished compound command
send if true; then
expect ...
send echo DIS''CARDED
raw \x03
expect ^C
send echo AF''TER
expect AFTER
absent DISCARDED
send exit
status 0
```

| Directive | Meaning |
|-----------|---------|
| `send text` | Wait for a prompt, type `text`, press Enter |
| `raw text` | Send keystrokes as typed (`\x03` is Ctrl-C, `\r` is Enter, `\x1b[A` is Up) |
| `expect text` | Wait until `text` appears, after the previous match |
| `absent text` | `text` must not have appeared so far |
| `wait seconds` | Keep reading output for that long |
| `rc text` / `write FILE text` | Add a line to `~/.hashrc` or another file under the session's HOME before the shell starts |
| `env NAME=value` | Set a variable for the shell |
| `login` | Start the shell as a login shell |
| `status N` | The shell must exit with status `N` |
| `file FILE has text` / `lacks text` | Check a file under HOME after the session |

Every case gets a fresh HOME, `TERM=dumb`, `PS1="hash> "` and `PS2="... "`, so `expect ...` is "the continuation prompt showed". The shell echoes what is typed, so to check that a command did not run, type something whose output differs from its spelling: `echo DIS''CARDED` prints `DISCARDED` only if it ran.

```bash
python3 tests/interactive/run.py            # every case
python3 tests/interactive/run.py ctrl-c -v  # cases whose name contains ctrl-c, with their transcripts
python3 tests/interactive/run.py --keep     # keep each case's HOME for a look
python3 tests/interactive/run.py --help     # the full directive reference
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
- Add an interactive case for anything a user sees or types at the prompt
- Test edge cases (NULL, empty strings, boundaries)
- Test error handling paths
