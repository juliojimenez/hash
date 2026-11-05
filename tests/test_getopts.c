#include "unity.h"
#include "../src/builtins.h"
#include "../src/shellvar.h"
#include "../src/script.h"
#include "../src/execute.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
    shellvar_init();
    script_init();
    shellvar_set("OPTIND", "1");
}

void tearDown(void) {
    script_cleanup();
    shellvar_cleanup();
}

// Run getopts once over explicit operands; returns the builtin's status
static int opts(const char *optstring, const char *a1, const char *a2, const char *a3) {
    char *args[] = {"getopts", (char *)optstring, "opt", (char *)a1, (char *)a2, (char *)a3, NULL};
    if (!a3) args[5] = NULL;
    if (!a2) args[4] = NULL;
    if (!a1) args[3] = NULL;
    shell_getopts(args);
    return last_command_exit_code;
}

void test_flags_and_argument_in_sequence(void) {
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-a", "-b", "val"));
    TEST_ASSERT_EQUAL_STRING("a", shellvar_get("opt"));
    TEST_ASSERT_NULL(shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));

    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-a", "-b", "val"));
    TEST_ASSERT_EQUAL_STRING("b", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("val", shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("4", shellvar_get("OPTIND"));

    TEST_ASSERT_EQUAL_INT(1, opts("ab:", "-a", "-b", "val"));
    TEST_ASSERT_EQUAL_STRING("?", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("4", shellvar_get("OPTIND"));
}

void test_cluster_and_attached_argument(void) {
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-ab", "val", NULL));
    TEST_ASSERT_EQUAL_STRING("a", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("1", shellvar_get("OPTIND"));  // still inside -ab
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-ab", "val", NULL));
    TEST_ASSERT_EQUAL_STRING("b", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("val", shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("3", shellvar_get("OPTIND"));

    shellvar_set("OPTIND", "1");
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-bval", "x", NULL));
    TEST_ASSERT_EQUAL_STRING("b", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("val", shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));
}

void test_operand_and_double_dash_end_the_options(void) {
    TEST_ASSERT_EQUAL_INT(1, opts("a", "x", "-a", NULL));
    TEST_ASSERT_EQUAL_STRING("1", shellvar_get("OPTIND"));  // x is the first operand

    shellvar_set("OPTIND", "1");
    TEST_ASSERT_EQUAL_INT(1, opts("a", "--", "-a", NULL));
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));  // past the --
}

void test_unknown_option_and_missing_argument(void) {
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-z", NULL, NULL));  // message on stderr
    TEST_ASSERT_EQUAL_STRING("?", shellvar_get("opt"));
    TEST_ASSERT_NULL(shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));

    shellvar_set("OPTIND", "1");
    TEST_ASSERT_EQUAL_INT(0, opts("ab:", "-b", NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("?", shellvar_get("opt"));
    TEST_ASSERT_NULL(shellvar_get("OPTARG"));
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));
}

void test_silent_mode_reports_through_optarg(void) {
    TEST_ASSERT_EQUAL_INT(0, opts(":ab:", "-z", NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("?", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("z", shellvar_get("OPTARG"));

    shellvar_set("OPTIND", "1");
    TEST_ASSERT_EQUAL_INT(0, opts(":ab:", "-b", NULL, NULL));
    TEST_ASSERT_EQUAL_STRING(":", shellvar_get("opt"));
    TEST_ASSERT_EQUAL_STRING("b", shellvar_get("OPTARG"));
}

void test_positional_parameters_by_default(void) {
    char *params[] = {"-a", "x", "y"};
    script_set_positional_params(3, params);
    char *args[] = {"getopts", "a", "opt", NULL};
    shell_getopts(args);
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("a", shellvar_get("opt"));
    shell_getopts(args);
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_STRING("2", shellvar_get("OPTIND"));
}

void test_usage_error(void) {
    char *args[] = {"getopts", "a", NULL};
    shell_getopts(args);
    TEST_ASSERT_EQUAL_INT(2, last_command_exit_code);
}

void test_shift_drops_positional_parameters(void) {
    char *params[] = {"a", "b", "c"};
    script_set_positional_params(3, params);
    char *one[] = {"shift", NULL};
    shell_shift(one);
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(3, script_state.positional_count);  // $0 b c
    TEST_ASSERT_EQUAL_STRING("b", script_get_positional_param(1));

    char *two[] = {"shift", "2", NULL};
    shell_shift(two);
    TEST_ASSERT_EQUAL_INT(0, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(1, script_state.positional_count);

    shell_shift(one);  // nothing left to shift
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
    TEST_ASSERT_EQUAL_INT(1, script_state.positional_count);

    char *bad[] = {"shift", "x", NULL};
    shell_shift(bad);
    TEST_ASSERT_EQUAL_INT(1, last_command_exit_code);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flags_and_argument_in_sequence);
    RUN_TEST(test_cluster_and_attached_argument);
    RUN_TEST(test_operand_and_double_dash_end_the_options);
    RUN_TEST(test_unknown_option_and_missing_argument);
    RUN_TEST(test_silent_mode_reports_through_optarg);
    RUN_TEST(test_positional_parameters_by_default);
    RUN_TEST(test_usage_error);
    RUN_TEST(test_shift_drops_positional_parameters);
    return UNITY_END();
}
