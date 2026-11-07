#include "unity.h"
#include "../src/config.h"
#include "../src/execute.h"
#include "../src/color_config.h"
#include "../src/script.h"
#include "../src/shellvar.h"
#include "../src/trap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define RC_HOME "/tmp/hash_config_test_home"
#define RC_FILE RC_HOME "/.hashrc"

void setUp(void) {
    script_init();
    shellvar_init();
    config_init();
    trap_init();
}

void tearDown(void) {
    script_cleanup();
    shellvar_cleanup();
    unlink(RC_FILE);
    rmdir(RC_HOME);
}

// Test config initialization
void test_config_init(void) {
    config_init();
    TEST_ASSERT_EQUAL_INT(0, shell_config.alias_count);
    TEST_ASSERT_TRUE(shell_config.use_colors);
    TEST_ASSERT_TRUE(shell_config.show_welcome);
}

// Test adding an alias
void test_add_alias(void) {
    int result = config_add_alias("ll", "ls -lah");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);
}

// Test getting an alias
void test_get_alias(void) {
    config_add_alias("ll", "ls -lah");
    const char *value = config_get_alias("ll");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING("ls -lah", value);
}

// Test getting nonexistent alias
void test_get_nonexistent_alias(void) {
    const char *value = config_get_alias("doesnotexist");
    TEST_ASSERT_NULL(value);
}

// Test updating an alias
void test_update_alias(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("ll", "ls -la");

    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);
    const char *value = config_get_alias("ll");
    TEST_ASSERT_EQUAL_STRING("ls -la", value);
}

// Test removing an alias
void test_remove_alias(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("la", "ls -A");

    int result = config_remove_alias("ll");
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, shell_config.alias_count);

    const char *value = config_get_alias("ll");
    TEST_ASSERT_NULL(value);
}

// Test removing nonexistent alias
void test_remove_nonexistent_alias(void) {
    int result = config_remove_alias("doesnotexist");
    TEST_ASSERT_EQUAL_INT(-1, result);
}

// unalias -a: config_clear_aliases empties the table
void test_clear_aliases(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("la", "ls -A");
    config_clear_aliases();
    TEST_ASSERT_EQUAL_INT(0, shell_config.alias_count);
    TEST_ASSERT_NULL(config_get_alias("ll"));
    TEST_ASSERT_NULL(config_get_alias("la"));
    // The table is usable again afterwards
    TEST_ASSERT_EQUAL_INT(0, config_add_alias("x", "echo x"));
    TEST_ASSERT_EQUAL_STRING("echo x", config_get_alias("x"));
}

// Test multiple aliases
void test_multiple_aliases(void) {
    config_add_alias("ll", "ls -lah");
    config_add_alias("la", "ls -A");
    config_add_alias("l", "ls -CF");

    TEST_ASSERT_EQUAL_INT(3, shell_config.alias_count);
    TEST_ASSERT_EQUAL_STRING("ls -lah", config_get_alias("ll"));
    TEST_ASSERT_EQUAL_STRING("ls -A", config_get_alias("la"));
    TEST_ASSERT_EQUAL_STRING("ls -CF", config_get_alias("l"));
}

// Test config_load_logout_files runs without crashing
// (actual file execution would require integration testing)
void test_config_load_logout_files(void) {
    // Just verify the function doesn't crash when called
    // This will try to load ~/.hash_logout which may or may not exist
    config_load_logout_files();
    // If we get here, the function completed without error
    TEST_ASSERT_TRUE(1);
}

// set option=value: the hash-specific options, with the quotes already removed by the shell
void test_set_option_known(void) {
    TEST_ASSERT_EQUAL_INT(0, config_set_option("welcome=off"));
    TEST_ASSERT_FALSE(shell_config.show_welcome);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("welcome=on"));
    TEST_ASSERT_TRUE(shell_config.show_welcome);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("colors=on"));
    TEST_ASSERT_TRUE(shell_config.use_colors);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("autosuggest=off"));
    TEST_ASSERT_FALSE(color_config.autosuggestion_enabled);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("autosuggest=on"));
    TEST_ASSERT_TRUE(color_config.autosuggestion_enabled);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("syntax_highlight=off"));
    TEST_ASSERT_FALSE(color_config.syntax_highlight_enabled);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("danger_highlight=off"));
    TEST_ASSERT_FALSE(color_config.danger_highlight_enabled);
    TEST_ASSERT_EQUAL_INT(0, config_set_option("color.prompt=bold,blue"));
    TEST_ASSERT_EQUAL_INT(0, config_set_option("PS1=> "));
}

void test_set_option_unknown(void) {
    TEST_ASSERT_EQUAL_INT(-1, config_set_option("history=on"));
    TEST_ASSERT_EQUAL_INT(-1, config_set_option("autosuggest=maybe"));
    TEST_ASSERT_EQUAL_INT(-1, config_set_option("color.=red"));
    TEST_ASSERT_EQUAL_INT(-1, config_set_option("color.prompt"));
    TEST_ASSERT_EQUAL_INT(-1, config_set_option(NULL));
}

// ~/.hashrc is a shell script: aliases, functions, conditionals and the hash
// options all take effect when the shell runs it at startup
void test_hashrc_runs_as_script(void) {
    mkdir(RC_HOME, 0700);
    FILE *f = fopen(RC_FILE, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("alias hash_rc_alias='echo aliased'\n"
          "hash_rc_fn() {\n  echo \"fn $1\"\n}\n"
          "if true; then\n  set welcome=off\nfi\n", f);
    fclose(f);

    TEST_ASSERT_EQUAL_INT(0, config_load_silent(RC_FILE));
    TEST_ASSERT_NOT_NULL(script_get_function("hash_rc_fn"));
    TEST_ASSERT_EQUAL_STRING("echo aliased", config_get_alias("hash_rc_alias"));
    TEST_ASSERT_FALSE(shell_config.show_welcome);
}

// An exit in a startup file stops the file and asks the shell to exit with
// that status; the lines after it do not run
void test_hashrc_exit_requests_shell_exit(void) {
    mkdir(RC_HOME, 0700);
    FILE *f = fopen(RC_FILE, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("set welcome=off\nexit 3\nset welcome=on\n", f);
    fclose(f);

    config_load_silent(RC_FILE);
    TEST_ASSERT_TRUE(script_state.exit_requested);
    TEST_ASSERT_EQUAL_INT(3, last_command_exit_code);
    TEST_ASSERT_FALSE(shell_config.show_welcome);
}

// A nested source does not end the outer file: a return after it still works
void test_hashrc_return_after_nested_source(void) {
    mkdir(RC_HOME, 0700);
    FILE *f = fopen(RC_HOME "/settings", "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("set welcome=off\n", f);
    fclose(f);
    f = fopen(RC_FILE, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("source " RC_HOME "/settings\nreturn\nset welcome=on\n", f);
    fclose(f);

    config_load_silent(RC_FILE);
    TEST_ASSERT_FALSE(shell_config.show_welcome);
    TEST_ASSERT_FALSE(script_state.exit_requested);
    TEST_ASSERT_FALSE(script_state.in_script);
    unlink(RC_HOME "/settings");
}

// The set options: by letter, by name, and the $- summary
void test_set_options_by_letter_and_name(void) {
    shell_options_init();
    TEST_ASSERT_TRUE(shell_option_apply_letter('x', true));
    TEST_ASSERT_TRUE(shell_option_xtrace());
    TEST_ASSERT_TRUE(shell_option_apply_letter('x', false));
    TEST_ASSERT_FALSE(shell_option_xtrace());
    TEST_ASSERT_TRUE(shell_option_apply_name("noclobber", true));
    TEST_ASSERT_TRUE(shell_option_noclobber());
    TEST_ASSERT_TRUE(shell_option_apply_letter('C', false));
    TEST_ASSERT_FALSE(shell_option_noclobber());
    TEST_ASSERT_TRUE(shell_option_apply_letter('h', true));   // accepted, nothing to do
    TEST_ASSERT_FALSE(shell_option_apply_letter('z', true));
    TEST_ASSERT_FALSE(shell_option_apply_name("braceexpand", true));  // vi is an option since #330
    TEST_ASSERT_FALSE(shell_option_apply_name(NULL, true));
}

void test_set_options_flags_string(void) {
    shell_options_init();
    char buf[32];
    shell_options_flags(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
    shell_option_apply_letter('e', true);
    shell_option_apply_letter('x', true);
    shell_option_apply_name("noglob", true);
    shell_options_flags(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("efx", buf);  // table order, like bash's letter order
    shell_options_init();
}

// A missing startup file is not an error
void test_missing_startup_file_is_fine(void) {
    TEST_ASSERT_EQUAL_INT(0, config_load_silent("/tmp/hash_config_test_missing/.hashrc"));
}

void test_vi_and_emacs_are_one_setting(void) {
    shell_options_init();
    TEST_ASSERT_FALSE(shell_option_vi());
    TEST_ASSERT_TRUE(shell_option_apply_name("vi", true));
    TEST_ASSERT_TRUE(shell_option_vi());
    TEST_ASSERT_TRUE(shell_option_apply_name("vi", false));
    TEST_ASSERT_FALSE(shell_option_vi());
    TEST_ASSERT_TRUE(shell_option_apply_name("emacs", false));
    TEST_ASSERT_TRUE(shell_option_vi());
    TEST_ASSERT_TRUE(shell_option_apply_name("emacs", true));
    TEST_ASSERT_FALSE(shell_option_vi());
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_config_init);
    RUN_TEST(test_add_alias);
    RUN_TEST(test_get_alias);
    RUN_TEST(test_get_nonexistent_alias);
    RUN_TEST(test_update_alias);
    RUN_TEST(test_remove_alias);
    RUN_TEST(test_remove_nonexistent_alias);
    RUN_TEST(test_clear_aliases);
    RUN_TEST(test_multiple_aliases);
    RUN_TEST(test_config_load_logout_files);
    RUN_TEST(test_set_option_known);
    RUN_TEST(test_set_option_unknown);
    RUN_TEST(test_hashrc_runs_as_script);
    RUN_TEST(test_hashrc_exit_requests_shell_exit);
    RUN_TEST(test_hashrc_return_after_nested_source);
    RUN_TEST(test_set_options_by_letter_and_name);
    RUN_TEST(test_set_options_flags_string);
    RUN_TEST(test_missing_startup_file_is_fine);

    RUN_TEST(test_vi_and_emacs_are_one_setting);
    return UNITY_END();
}
