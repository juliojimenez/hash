#ifndef ARITH_H
#define ARITH_H

/**
 * Evaluate an arithmetic expression
 *
 * Supports:
 * - Integer arithmetic: +, -, *, /, %
 * - Comparisons: <, >, <=, >=, ==, !=
 * - Logical: &&, ||, !
 * - Bitwise: &, |, ^, ~, <<, >>
 * - Ternary: expr ? expr : expr
 * - Parentheses for grouping
 * - Variable references (without $)
 * - Assignment: =, +=, -=, *=, /=, %=
 * - Pre/post increment/decrement: ++, --
 *
 * @param expr The arithmetic expression to evaluate
 * @param result Pointer to store the result
 * @return 0 on success, -1 on error
 */
int arith_evaluate(const char *expr, long *result);

/**
 * Expand arithmetic substitutions in a string
 *
 * Replaces all $((...)) with their evaluated results
 *
 * Returns newly allocated string (caller must free)
 * Returns NULL if no expansion needed or on error
 *
 * @param str String containing arithmetic expressions
 * @return Expanded string or NULL
 */
char *arith_expand(const char *str);

/**
 * Expand arithmetic substitutions in all arguments
 * Modifies args array in place (replaces with expanded versions)
 *
 * @param args Null-terminated argument array
 * @return 0 on success, -1 on error
 */
int arith_args(char **args);

/**
 * Check if a string contains arithmetic expansion
 *
 * @param str String to check
 * @return 1 if contains $((...)), 0 otherwise
 */
int has_arith(const char *str);

#endif // ARITH_H
