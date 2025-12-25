#include "unity.h"
#include "../src/chain.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// Test parsing single command (no chaining)
void test_parse_single_command(void) {
    char line[] = "echo hello";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(1, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo hello", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain->commands[0].next_op);

    chain_free(chain);
}

// Test parsing semicolon chain
void test_parse_semicolon_chain(void) {
    char line[] = "echo first ; echo second";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(2, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo first", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_ALWAYS, chain->commands[0].next_op);
    TEST_ASSERT_EQUAL_STRING("echo second", chain->commands[1].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_NONE, chain->commands[1].next_op);

    chain_free(chain);
}

// Test parsing AND chain
void test_parse_and_chain(void) {
    char line[] = "true && echo success";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(2, chain->count);
    TEST_ASSERT_EQUAL_STRING("true", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_AND, chain->commands[0].next_op);
    TEST_ASSERT_EQUAL_STRING("echo success", chain->commands[1].cmd_line);

    chain_free(chain);
}

// Test parsing OR chain
void test_parse_or_chain(void) {
    char line[] = "false || echo failure";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(2, chain->count);
    TEST_ASSERT_EQUAL_STRING("false", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_OR, chain->commands[0].next_op);
    TEST_ASSERT_EQUAL_STRING("echo failure", chain->commands[1].cmd_line);

    chain_free(chain);
}

// Test parsing mixed operators
void test_parse_mixed_operators(void) {
    char line[] = "echo a ; echo b && echo c";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(3, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo a", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_ALWAYS, chain->commands[0].next_op);
    TEST_ASSERT_EQUAL_STRING("echo b", chain->commands[1].cmd_line);
    TEST_ASSERT_EQUAL_INT(CHAIN_AND, chain->commands[1].next_op);
    TEST_ASSERT_EQUAL_STRING("echo c", chain->commands[2].cmd_line);

    chain_free(chain);
}

// Test operators inside quotes are ignored
void test_parse_quoted_operators(void) {
    char line[] = "echo \"test && test\"";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(1, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo \"test && test\"", chain->commands[0].cmd_line);

    chain_free(chain);
}

// Test empty line
void test_parse_empty_line(void) {
    char line[] = "";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NULL(chain);
}

// Test whitespace handling
void test_parse_whitespace(void) {
    char line[] = "  echo hello  ;  echo world  ";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(2, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo hello", chain->commands[0].cmd_line);
    TEST_ASSERT_EQUAL_STRING("echo world", chain->commands[1].cmd_line);

    chain_free(chain);
}

// Test trailing semicolon
void test_parse_trailing_semicolon(void) {
    char line[] = "echo hello ;";
    CommandChain *chain = chain_parse(line);

    TEST_ASSERT_NOT_NULL(chain);
    TEST_ASSERT_EQUAL_INT(1, chain->count);
    TEST_ASSERT_EQUAL_STRING("echo hello", chain->commands[0].cmd_line);

    chain_free(chain);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_single_command);
    RUN_TEST(test_parse_semicolon_chain);
    RUN_TEST(test_parse_and_chain);
    RUN_TEST(test_parse_or_chain);
    RUN_TEST(test_parse_mixed_operators);
    RUN_TEST(test_parse_quoted_operators);
    RUN_TEST(test_parse_empty_line);
    RUN_TEST(test_parse_whitespace);
    RUN_TEST(test_parse_trailing_semicolon);

    return UNITY_END();
}
