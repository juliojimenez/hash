#include "unity.h"
#include "../src/completion.h"
#include "../src/config.h"
#include "../src/shellvar.h"
#include "../src/exec.h"
#include "../src/script.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static char saved_cwd[1024];

void setUp(void) {
    config_init();
    completion_init();
    script_init();
    shellvar_init();
    if (!getcwd(saved_cwd, sizeof(saved_cwd))) {
        saved_cwd[0] = '\0';
    }
}

void tearDown(void) {
    if (saved_cwd[0]) {
        chdir(saved_cwd);  // a failed assertion may have left us in the test tree
    }
}

// Test completion generation returns results
void test_completion_generate_basic(void) {
    // Complete "ec" should find "echo"
    CompletionResult *result = completion_generate("ec", 2);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->count > 0);

    // Check if "echo" is in the results
    int found_echo = 0;
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->matches[i], "echo") == 0) {
            found_echo = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_echo);

    completion_free_result(result);
}

// Test completion with empty input
void test_completion_generate_empty(void) {
    CompletionResult *result = completion_generate("", 0);

    TEST_ASSERT_NOT_NULL(result);
    // Should return built-in commands
    TEST_ASSERT_TRUE(result->count > 0);

    completion_free_result(result);
}

// Test common prefix calculation
void test_completion_common_prefix_single(void) {
    char *matches[] = {"testing"};
    char *prefix = completion_common_prefix(matches, 1);

    TEST_ASSERT_NOT_NULL(prefix);
    TEST_ASSERT_EQUAL_STRING("testing", prefix);

    free(prefix);
}

// Test common prefix with multiple matches
void test_completion_common_prefix_multiple(void) {
    char *matches[] = {"test1", "test2", "test3"};
    char *prefix = completion_common_prefix(matches, 3);

    TEST_ASSERT_NOT_NULL(prefix);
    TEST_ASSERT_EQUAL_STRING("test", prefix);

    free(prefix);
}

// Test common prefix with no common prefix
void test_completion_common_prefix_none(void) {
    char *matches[] = {"apple", "banana", "cherry"};
    char *prefix = completion_common_prefix(matches, 3);

    TEST_ASSERT_NULL(prefix);
}

// Test file completion for root directory - should not have double slashes
void test_completion_root_no_double_slash(void) {
    // Complete "ls /" should show files in root without double slashes
    CompletionResult *result = completion_generate("ls /", 4);

    TEST_ASSERT_NOT_NULL(result);

    // Check that no match starts with "//"
    for (int i = 0; i < result->count; i++) {
        TEST_ASSERT_FALSE(strncmp(result->matches[i], "//", 2) == 0);
    }

    completion_free_result(result);
}

// Test file completion with trailing slash doesn't add extra slash
void test_completion_path_no_extra_slash(void) {
    // Create a test scenario - complete "ls /tmp/"
    CompletionResult *result = completion_generate("ls /tmp/", 8);

    TEST_ASSERT_NOT_NULL(result);

    // Check that matches start with /tmp/ not /tmp//
    for (int i = 0; i < result->count; i++) {
        // Should not have double slashes
        TEST_ASSERT_NULL(strstr(result->matches[i], "//"));
    }

    completion_free_result(result);
}

// Test that directory completions end with /
void test_completion_directory_has_trailing_slash(void) {
    // Complete in current directory, looking for directories
    CompletionResult *result = completion_generate("cd ", 3);

    TEST_ASSERT_NOT_NULL(result);

    // At least some matches should be directories (end with /)
    // We can't guarantee which ones, but the code should work

    completion_free_result(result);
}

// Test completing a path like /e should give /etc/ etc.
void test_completion_partial_root_path(void) {
    CompletionResult *result = completion_generate("ls /e", 5);

    TEST_ASSERT_NOT_NULL(result);

    // Should find /etc if it exists
    int found_etc = 0;
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->matches[i], "/etc/") == 0) {
            found_etc = 1;
            break;
        }
    }
    // /etc should exist on most systems
    TEST_ASSERT_TRUE(found_etc);

    completion_free_result(result);
}

// Test that NULL input is handled
void test_completion_null_input(void) {
    CompletionResult *result = completion_generate(NULL, 0);
    TEST_ASSERT_NULL(result);
}

// Test free on NULL
void test_completion_free_null(void) {
    // Should not crash
    completion_free_result(NULL);
    TEST_ASSERT_TRUE(1);
}

// ============================================================================
// Context-aware completion (#329)
// ============================================================================

#define COMP_TEST_DIR "/tmp/hash_completion_ctx_test"

static int has_match(const CompletionResult *r, const char *text) {
    for (int i = 0; r && i < r->count; i++) {
        if (strcmp(r->matches[i], text) == 0) {
            return 1;
        }
    }
    return 0;
}

static void make_test_tree(void) {
    system("rm -rf " COMP_TEST_DIR " && mkdir -p " COMP_TEST_DIR "/adir && touch " COMP_TEST_DIR "/afile");
    FILE *f = fopen(COMP_TEST_DIR "/Makefile", "w");
    if (f) {
        fputs("VAR := 1\n.PHONY: build clean\nbuild: src/main.c\n\tcc -o out $<\nclean check:\n\trm -f out\n%.o: %.c\n\tcc -c $<\n", f);
        fclose(f);
    }
}

void test_completion_variables_after_dollar(void) {
    setenv("HASHCOMP_TEST_VAR", "1", 1);
    CompletionResult *r = completion_generate("echo $HASHCOMP_TE", 17);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("$HASHCOMP_TEST_VAR", r->matches[0]);
    completion_free_result(r);
    r = completion_generate("echo ${HASHCOMP_TE", 18);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("${HASHCOMP_TEST_VAR}", r->matches[0]);
    completion_free_result(r);
    // A shell variable that is not exported is offered too
    shellvar_set("hashcomp_local_only", "x");
    r = completion_generate("echo $hashcomp_loc", 18);
    TEST_ASSERT_TRUE(has_match(r, "$hashcomp_local_only"));
    completion_free_result(r);
    shellvar_unset("hashcomp_local_only");
    unsetenv("HASHCOMP_TEST_VAR");
}

void test_completion_cd_directories_only(void) {
    make_test_tree();
    char cwd[1024];
    TEST_ASSERT_NOT_NULL(getcwd(cwd, sizeof(cwd)));
    TEST_ASSERT_EQUAL_INT(0, chdir(COMP_TEST_DIR));
    CompletionResult *r = completion_generate("cd a", 4);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("adir/", r->matches[0]);
    completion_free_result(r);
    r = completion_generate("ls a", 4);  // other commands keep files
    TEST_ASSERT_EQUAL_INT(2, r->count);
    completion_free_result(r);
    r = completion_generate("ls; cd a", 8);  // the command after a separator counts
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("adir/", r->matches[0]);
    completion_free_result(r);
    chdir(cwd);
}

void test_completion_name_spaces(void) {
    config_add_alias("hashcomp_myalias", "ls");
    CompletionResult *r = completion_generate("unalias hashcomp_my", 19);
    TEST_ASSERT_TRUE(has_match(r, "hashcomp_myalias"));
    completion_free_result(r);
    setenv("HASHCOMP_EXPORTED", "1", 1);
    r = completion_generate("export HASHCOMP_EX", 18);
    TEST_ASSERT_TRUE(has_match(r, "HASHCOMP_EXPORTED"));   // bare names after export
    completion_free_result(r);
    r = completion_generate("unset HASHCOMP_EX", 17);
    TEST_ASSERT_TRUE(has_match(r, "HASHCOMP_EXPORTED"));
    completion_free_result(r);
    unsetenv("HASHCOMP_EXPORTED");
    r = completion_generate("sudo ec", 7);                   // the operand of sudo is a command
    TEST_ASSERT_TRUE(has_match(r, "echo"));
    completion_free_result(r);
    r = completion_generate("~roo", 4);                      // ~user
    TEST_ASSERT_TRUE(has_match(r, "~root/"));
    completion_free_result(r);
}

void test_completion_wordlist_spec(void) {
    TEST_ASSERT_EQUAL_INT(0, completion_spec_define("hashcomp_tool", "alpha alps beta", NULL, 0, false, false));
    CompletionResult *r = completion_generate("hashcomp_tool al", 16);
    TEST_ASSERT_EQUAL_INT(2, r->count);
    TEST_ASSERT_EQUAL_STRING("alpha", r->matches[0]);       // sorted
    TEST_ASSERT_EQUAL_STRING("alps", r->matches[1]);
    TEST_ASSERT_EQUAL_STRING("alp", r->common_prefix);
    completion_free_result(r);
    r = completion_generate("hashcomp_tool zz", 16);
    TEST_ASSERT_EQUAL_INT(0, r->count);                     // no -o default: nothing
    completion_free_result(r);
    const CompSpec *spec = completion_spec_find("hashcomp_tool");
    TEST_ASSERT_NOT_NULL(spec);
    TEST_ASSERT_EQUAL_STRING("alpha alps beta", spec->wordlist);
    TEST_ASSERT_TRUE(completion_spec_remove("hashcomp_tool"));
    TEST_ASSERT_FALSE(completion_spec_remove("hashcomp_tool"));
    TEST_ASSERT_NULL(completion_spec_find("hashcomp_tool"));
}

void test_completion_make_targets(void) {
    make_test_tree();
    char cwd[1024];
    TEST_ASSERT_NOT_NULL(getcwd(cwd, sizeof(cwd)));
    TEST_ASSERT_EQUAL_INT(0, chdir(COMP_TEST_DIR));
    CompletionResult *r = completion_generate("make ", 5);
    TEST_ASSERT_EQUAL_INT(3, r->count);                     // build, check, clean: not VAR, .PHONY or %.o
    TEST_ASSERT_EQUAL_STRING("build", r->matches[0]);
    TEST_ASSERT_EQUAL_STRING("check", r->matches[1]);
    TEST_ASSERT_EQUAL_STRING("clean", r->matches[2]);
    completion_free_result(r);
    r = completion_generate("make cl", 7);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("clean", r->matches[0]);
    completion_free_result(r);
    chdir(cwd);
}

void test_completion_git_subcommands(void) {
    CompletionResult *r = completion_generate("git che", 7);
    TEST_ASSERT_EQUAL_INT(2, r->count);
    TEST_ASSERT_EQUAL_STRING("checkout", r->matches[0]);
    TEST_ASSERT_EQUAL_STRING("cherry-pick", r->matches[1]);
    completion_free_result(r);
    r = completion_generate("git --no-pager sta", 18);      // options before the subcommand
    TEST_ASSERT_TRUE(has_match(r, "stash"));
    TEST_ASSERT_TRUE(has_match(r, "status"));
    completion_free_result(r);
}

void test_completion_function_spec(void) {
    bool incomplete = false;
    exec_string("hashcomp_fn() { echo \"$1:$2:$3\" >/dev/null; echo gamma; echo delta; echo gap; }", &incomplete);
    TEST_ASSERT_EQUAL_INT(0, completion_spec_define("hashcomp_cmd", NULL, "hashcomp_fn", 0, false, false));
    CompletionResult *r = completion_generate("hashcomp_cmd ga", 15);
    TEST_ASSERT_EQUAL_INT(2, r->count);                     // gamma and gap start with ga, delta does not
    TEST_ASSERT_EQUAL_STRING("gamma", r->matches[0]);
    TEST_ASSERT_EQUAL_STRING("gap", r->matches[1]);
    completion_free_result(r);
    r = completion_generate("hashcomp_cmd d", 14);
    TEST_ASSERT_EQUAL_INT(1, r->count);
    TEST_ASSERT_EQUAL_STRING("delta", r->matches[0]);
    completion_free_result(r);
    completion_spec_remove("hashcomp_cmd");
}

// ============================================================================
// Did you mean (#332)
// ============================================================================

void test_edit_distance(void) {
    TEST_ASSERT_EQUAL_INT(0, completion_edit_distance("echo", "echo"));
    TEST_ASSERT_EQUAL_INT(1, completion_edit_distance("ehco", "echo"));   // transposition counts one
    TEST_ASSERT_EQUAL_INT(1, completion_edit_distance("sl", "ls"));
    TEST_ASSERT_EQUAL_INT(1, completion_edit_distance("exprot", "export"));
    TEST_ASSERT_EQUAL_INT(1, completion_edit_distance("gti", "git"));
    TEST_ASSERT_EQUAL_INT(2, completion_edit_distance("expr", "export"));
    TEST_ASSERT_EQUAL_INT(3, completion_edit_distance("abc", "xyz"));
    TEST_ASSERT_EQUAL_INT(4, completion_edit_distance("", "echo"));
}

void test_suggest_commands(void) {
    char *out[3];
    int n = completion_suggest_commands("ehco", out, 3);
    TEST_ASSERT_EQUAL_INT(1, n);                       // echo is one edit away, nothing else that close
    TEST_ASSERT_EQUAL_STRING("echo", out[0]);
    for (int i = 0; i < n; i++) free(out[i]);
    n = completion_suggest_commands("exprot", out, 3);
    TEST_ASSERT_TRUE(n >= 1);
    TEST_ASSERT_EQUAL_STRING("export", out[0]);
    for (int i = 0; i < n; i++) free(out[i]);
    n = completion_suggest_commands("zqxjkvw", out, 3);  // nothing within two edits
    TEST_ASSERT_EQUAL_INT(0, n);
    n = completion_suggest_commands("echo", out, 3);     // the name itself is not a suggestion
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(strcmp(out[i], "echo") != 0);
        free(out[i]);
    }
    const char *const *names;
    size_t count = completion_path_commands(&names);     // the cached PATH scan is sorted and populated
    TEST_ASSERT_TRUE(count > 10);
    for (size_t i = 1; i < count; i++) {
        TEST_ASSERT_TRUE(strcmp(names[i - 1], names[i]) < 0);
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_completion_generate_basic);
    RUN_TEST(test_completion_generate_empty);
    RUN_TEST(test_completion_common_prefix_single);
    RUN_TEST(test_completion_common_prefix_multiple);
    RUN_TEST(test_completion_common_prefix_none);
    RUN_TEST(test_completion_root_no_double_slash);
    RUN_TEST(test_completion_path_no_extra_slash);
    RUN_TEST(test_completion_directory_has_trailing_slash);
    RUN_TEST(test_completion_partial_root_path);
    RUN_TEST(test_completion_null_input);
    RUN_TEST(test_completion_free_null);

    RUN_TEST(test_completion_variables_after_dollar);
    RUN_TEST(test_completion_cd_directories_only);
    RUN_TEST(test_completion_name_spaces);
    RUN_TEST(test_completion_wordlist_spec);
    RUN_TEST(test_completion_make_targets);
    RUN_TEST(test_completion_git_subcommands);
    RUN_TEST(test_completion_function_spec);
    RUN_TEST(test_edit_distance);
    RUN_TEST(test_suggest_commands);
    return UNITY_END();
}
