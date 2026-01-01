#ifndef TEST_BUILTIN_H
#define TEST_BUILTIN_H

// ============================================================================
// NOTE: This file implements the POSIX 'test' and '[' shell builtins,
// as well as the bash-style '[[' extended test.
// It is NOT a unit test file. The name "test" refers to the shell command.
// Unit tests are located in the tests/ directory.
// ============================================================================
//
// ============================================================================
// POSIX test / [ ] / [[ ]] BUILTIN
// ============================================================================
//
// Implements the POSIX test command, [ ] syntax, and bash-style [[ ]]
// for conditional expressions.
//
// File tests:
//   -e FILE     True if file exists
//   -f FILE     True if file exists and is a regular file
//   -d FILE     True if file exists and is a directory
//   -r FILE     True if file exists and is readable
//   -w FILE     True if file exists and is writable
//   -x FILE     True if file exists and is executable
//   -s FILE     True if file exists and has size > 0
//   -L FILE     True if file exists and is a symbolic link
//   -h FILE     Same as -L
//
// String tests:
//   -z STRING   True if string is empty
//   -n STRING   True if string is not empty
//   STRING      True if string is not empty
//   S1 = S2     True if strings are equal
//   S1 != S2    True if strings are not equal
//
// Integer comparisons:
//   N1 -eq N2   True if N1 equals N2
//   N1 -ne N2   True if N1 not equals N2
//   N1 -lt N2   True if N1 < N2
//   N1 -le N2   True if N1 <= N2
//   N1 -gt N2   True if N1 > N2
//   N1 -ge N2   True if N1 >= N2
//
// Logical operators:
//   ! EXPR      True if EXPR is false
//   EXPR -a EXPR  True if both expressions are true (AND) - [ ] only
//   EXPR -o EXPR  True if either expression is true (OR) - [ ] only
//   EXPR && EXPR  True if both expressions are true (AND) - [[ ]] only
//   EXPR || EXPR  True if either expression is true (OR) - [[ ]] only
//   ( EXPR )    Grouping
//
// [[ ]] specific features:
//   PATTERN matching with == and != (supports * and ? wildcards)
//   =~            Regex matching (returns 0 if match, 1 otherwise)
//   <             String less than (lexicographic)
//   >             String greater than (lexicographic)
//
// ============================================================================

/**
 * Execute the test builtin command
 *
 * @param args Arguments (test arg1 arg2 ... or [ arg1 arg2 ... ])
 * @return 0 if true, 1 if false, 2 on error
 */
int builtin_test(char **args);

/**
 * Execute the [ builtin (test with syntax checking for ])
 *
 * @param args Arguments ([ arg1 arg2 ... ])
 * @return 0 if true, 1 if false, 2 on error
 */
int builtin_bracket(char **args);

/**
 * Execute the [[ builtin (bash-style extended test)
 *
 * Differences from [:
 * - Uses && and || for logical operators (not -a and -o)
 * - Supports pattern matching with == and !=
 * - Supports regex matching with =~
 * - Supports string comparison with < and >
 * - No word splitting or pathname expansion
 *
 * @param args Arguments ([[ arg1 arg2 ... ]])
 * @return 0 if true, 1 if false, 2 on error
 */
int builtin_double_bracket(char **args);

/**
 * Evaluate a test expression
 * This is the core evaluation function used by both test and [
 *
 * @param args Array of test arguments
 * @param argc Number of arguments
 * @return 0 if true, 1 if false, 2 on error
 */
int test_evaluate(char **args, int argc);

#endif // TEST_BUILTIN_H
