#include "unity.h"
#include "../src/ast_parser.h"
#include <stdio.h>
#include <string.h>

static Arena arena;

void setUp(void) {
    arena_init(&arena, 0);
}

void tearDown(void) {
    arena_free(&arena);
}

// Parse and render; failures come back as INCOMPLETE or ERROR:line:col: message
static const char *sx(const char *src) {
    static char failure[256];
    AstParseResult r = parse_program(src, &arena);
    if (r.status == PARSE_INCOMPLETE) {
        return "INCOMPLETE";
    }
    if (r.status == PARSE_ERROR) {
        snprintf(failure, sizeof(failure), "ERROR:%zu:%zu: %s", r.line, r.col, r.message);
        return failure;
    }
    return ast_sexpr(r.root, &arena);
}

// ============================================================================
// Simple commands and lists
// ============================================================================

void test_empty_and_comment_only(void) {
    TEST_ASSERT_EQUAL_STRING("(list)", sx(""));
    TEST_ASSERT_EQUAL_STRING("(list)", sx("   \n\n"));
    TEST_ASSERT_EQUAL_STRING("(list)", sx("# just a comment\n"));
}

void test_simple_command(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"hello\" \"world\"))", sx("echo hello world"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"hello\"))", sx("echo hello\n"));
}

void test_words_keep_quotes_and_expansions_raw(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"'a b'\" \"\\\"$x\\\"\" \"$(date)\" \"${y:-d}\"))",
                             sx("echo 'a b' \"$x\" $(date) ${y:-d}"));
}

void test_separators(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"a\") (bg (cmd \"b\")) (cmd \"c\"))", sx("a; b & c"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"a\") (cmd \"b\"))", sx("a\nb"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"a\") (cmd \"b\"))", sx("a;\n\nb;"));
    TEST_ASSERT_EQUAL_STRING("(list (bg (cmd \"a\")))", sx("a &"));
}

void test_and_or_left_associative(void) {
    TEST_ASSERT_EQUAL_STRING("(list (or (and (cmd \"a\") (cmd \"b\")) (cmd \"c\")))", sx("a && b || c"));
    TEST_ASSERT_EQUAL_STRING("(list (and (cmd \"a\") (cmd \"b\")))", sx("a &&\n  b"));
}

void test_pipeline_and_negation(void) {
    TEST_ASSERT_EQUAL_STRING("(list (pipe (cmd \"a\") (cmd \"b\") (cmd \"c\")))", sx("a | b | c"));
    TEST_ASSERT_EQUAL_STRING("(list (not (pipe (cmd \"a\") (cmd \"b\"))))", sx("! a | b"));
    TEST_ASSERT_EQUAL_STRING("(list (not (cmd \"a\")))", sx("! a"));
    TEST_ASSERT_EQUAL_STRING("(list (pipe (cmd \"a\") (cmd \"b\")))", sx("a |\n b"));
}

void test_assignments_before_command_only(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd (assign \"X=1\") (assign \"Y=two\") \"cmd\" \"arg\" \"Z=3\"))",
                             sx("X=1 Y=two cmd arg Z=3"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd (assign \"X=1\")))", sx("X=1"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"=x\" \"1x=2\"))", sx("=x 1x=2"));
}

void test_reserved_words_as_arguments(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"if\" \"then\" \"fi\" \"{\" \"}\" \"!\"))",
                             sx("echo if then fi { } !"));
}

// ============================================================================
// Redirections and here-documents
// ============================================================================

void test_redirections(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (cmd \"cmd\" (redir 1 > \"out\") (redir 2 >& \"1\") (redir 0 < \"in\") (redir 1 >> \"log\")))",
        sx("cmd >out 2>&1 <in >>log"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"cmd\" (redir 3 <> \"f\") (redir 0 <& \"3\")))", sx("cmd 3<>f <&3"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd (redir 1 > \"out\")))", sx(">out"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"a\" \"b\" (redir 1 > \"x\")))", sx("a >x b"));
}

void test_heredoc(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (cmd \"cat\" (heredoc 0 << \"EOF\" \"line1\\n\\nline3\\n\")) (cmd \"echo\" \"after\"))",
        sx("cat <<EOF\nline1\n\nline3\nEOF\necho after"));
}

void test_heredoc_quoted_delimiter_and_tab_strip(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"cat\" (heredoc 0 << quoted \"EOF\" \"$x\\n\")))",
                             sx("cat <<'EOF'\n$x\nEOF\n"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"cat\" (heredoc 0 << quoted \"EOF\" \"a\\n\")))",
                             sx("cat <<\"EOF\"\na\nEOF\n"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"cat\" (heredoc 0 <<- \"EOF\" \"a\\n  b\\n\")))",
                             sx("cat <<-EOF\n\ta\n\t  b\n\tEOF\n"));
}

void test_two_heredocs_on_one_line_and_fd(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (cmd \"cmd\" (heredoc 0 << \"A\" \"one\\n\") (heredoc 3 << \"B\" \"two\\n\")))",
        sx("cmd <<A 3<<B\none\nA\ntwo\nB\n"));
}

void test_heredoc_in_compound_and_pipeline(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (if (list (cmd \"true\")) (list (pipe (cmd \"cat\" (heredoc 0 << \"EOF\" \"x\\n\")) (cmd \"wc\")))))",
        sx("if true; then cat <<EOF | wc\nx\nEOF\nfi"));
}

void test_unterminated_heredoc_is_incomplete(void) {
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("cat <<EOF\nline1\n"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("cat <<EOF"));
}

// ============================================================================
// Compound commands
// ============================================================================

void test_if_one_line_and_multi_line(void) {
    const char *want = "(list (if (list (cmd \"a\")) (list (cmd \"b\")) (elif (list (cmd \"c\")) (list (cmd \"d\"))) (else (list (cmd \"e\")))))";
    TEST_ASSERT_EQUAL_STRING(want, sx("if a; then b; elif c; then d; else e; fi"));
    TEST_ASSERT_EQUAL_STRING(want, sx("if a\nthen\n  b\nelif c\nthen\n  d\nelse\n  e\nfi\n"));
    TEST_ASSERT_EQUAL_STRING("(list (if (list (cmd \"a\")) (list (cmd \"b\"))))", sx("if a; then b; fi"));
}

void test_for_loop(void) {
    TEST_ASSERT_EQUAL_STRING("(list (for \"i\" (words \"1\" \"2\") (list (cmd \"echo\" \"$i\"))))",
                             sx("for i in 1 2; do echo $i; done"));
    TEST_ASSERT_EQUAL_STRING("(list (for \"i\" (words \"a\" \"b\") (list (cmd \":\"))))",
                             sx("for i\nin a b\ndo\n:\ndone"));
    TEST_ASSERT_EQUAL_STRING("(list (for \"i\" (list (cmd \":\"))))", sx("for i; do :; done"));
    TEST_ASSERT_EQUAL_STRING("(list (for \"i\" (list (cmd \":\"))))", sx("for i\ndo :; done"));
    TEST_ASSERT_EQUAL_STRING("(list (for \"i\" (words) (list (cmd \":\"))))", sx("for i in; do :; done"));
}

void test_while_until(void) {
    TEST_ASSERT_EQUAL_STRING("(list (while (list (cmd \"a\")) (list (cmd \"b\"))))", sx("while a; do b; done"));
    TEST_ASSERT_EQUAL_STRING("(list (until (list (cmd \"a\")) (list (cmd \"b\") (cmd \"c\"))))",
                             sx("until a\ndo\nb\nc\ndone"));
}

void test_case(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (case \"$x\" (item (pat \"a\" \"b\") (list (cmd \"echo\" \"ab\"))) (item (pat \"c\") (list (cmd \"echo\" \"c\"))) (item (pat \"*\") (list))))",
        sx("case $x in a|b) echo ab;; (c) echo c;; *) ;; esac"));
    TEST_ASSERT_EQUAL_STRING(
        "(list (case \"x\" (item (pat \"x\") (list (cmd \"echo\" \"last\")))))",
        sx("case x in\n  x)\n    echo last\nesac"));
    TEST_ASSERT_EQUAL_STRING("(list (case \"x\" (item (pat \"x\") (list))))", sx("case x in x) esac"));
}

void test_brace_group_and_subshell(void) {
    TEST_ASSERT_EQUAL_STRING("(list (brace (list (cmd \"a\") (cmd \"b\")) (redir 1 > \"out\")))", sx("{ a; b; } >out"));
    TEST_ASSERT_EQUAL_STRING("(list (brace (list (cmd \"a\") (cmd \"b\"))))", sx("{\na\nb\n}"));
    TEST_ASSERT_EQUAL_STRING("(list (subshell (list (cmd \"a\") (cmd \"b\"))))", sx("(a; b)"));
    TEST_ASSERT_EQUAL_STRING("(list (subshell (list (cmd \"a\"))))", sx("(a)"));
    TEST_ASSERT_EQUAL_STRING("(list (pipe (subshell (list (cmd \"a\"))) (cmd \"b\")))", sx("(a) | b"));
}

void test_function_definition(void) {
    TEST_ASSERT_EQUAL_STRING("(list (function \"f\" (brace (list (cmd \"echo\" \"hi\")))))", sx("f() { echo hi; }"));
    TEST_ASSERT_EQUAL_STRING("(list (function \"f\" (brace (list (cmd \"echo\" \"hi\")))))", sx("f ()\n{\n  echo hi\n}\n"));
    TEST_ASSERT_EQUAL_STRING("(list (function \"f\" (subshell (list (cmd \":\")))))", sx("f() ( : )"));
    TEST_ASSERT_EQUAL_STRING("(list (function \"f\" (brace (list (cmd \":\")) (redir 1 > \"log\"))) (cmd \"f\"))",
                             sx("f() { :; } >log; f"));
}

void test_nested_compounds(void) {
    TEST_ASSERT_EQUAL_STRING(
        "(list (if (list (cmd \"true\")) (list (for \"i\" (words \"1\") (list (brace (list (cmd \"echo\" \"$i\"))))))))",
        sx("if true; then for i in 1; do { echo $i; }; done; fi"));
    TEST_ASSERT_EQUAL_STRING(
        "(list (while (list (cmd \"read\" \"l\")) (list (case \"$l\" (item (pat \"q\") (list (cmd \"break\")))))))",
        sx("while read l; do case $l in q) break;; esac; done"));
}

void test_compound_in_pipeline_and_and_or(void) {
    TEST_ASSERT_EQUAL_STRING("(list (and (brace (list (cmd \"a\"))) (subshell (list (cmd \"b\")))))", sx("{ a; } && (b)"));
    TEST_ASSERT_EQUAL_STRING("(list (pipe (cmd \"x\") (while (list (cmd \"read\" \"l\")) (list (cmd \"echo\" \"$l\")))))",
                             sx("x | while read l; do echo $l; done"));
}

// ============================================================================
// Comments and line continuations
// ============================================================================

void test_comments(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a\"))", sx("echo a # comment; echo b"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a\"))", sx("echo a;# comment; echo b"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a#b\"))", sx("echo a#b"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a\") (cmd \"echo\" \"b\"))", sx("echo a # c\necho b"));
}

void test_line_continuations(void) {
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a\" \"b\"))", sx("echo a \\\nb"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"ab\"))", sx("echo a\\\nb"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"a\" \"b\"))", sx("echo a \\\n  b"));
    TEST_ASSERT_EQUAL_STRING("(list (if (list (cmd \"true\")) (list (cmd \"b\"))))", sx("if true; then \\\nb; fi"));
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"'a\\\\\\nb'\"))", sx("echo 'a\\\nb'"));
}

void test_escaped_separator_in_word(void) {
    // Source text: echo foo\;#bar; echo after  (one backslash before the semicolon)
    TEST_ASSERT_EQUAL_STRING("(list (cmd \"echo\" \"foo\\\\;#bar\") (cmd \"echo\" \"after\"))",
                             sx("echo foo\\;#bar; echo after"));
}

// ============================================================================
// Incomplete input (what the prompt uses to show PS2)
// ============================================================================

void test_incomplete_compounds(void) {
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("for i in 1 2"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("for i in 1 2; do"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("for i in 1 2; do echo $i"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("if true"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("if true; then"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("while true; do :"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("case x in"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("case x in a)"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("case x in a) echo;;"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("{ a"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("( a"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("f()"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("f() {"));
}

void test_incomplete_operators_and_quotes(void) {
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("a |"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("a &&"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("a ||\n"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("!"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("echo \"unterminated"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("echo 'unterminated\nmore"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("echo $(cat"));
    TEST_ASSERT_EQUAL_STRING("INCOMPLETE", sx("cmd >"));
}

// ============================================================================
// Syntax errors
// ============================================================================

void test_syntax_errors(void) {
    TEST_ASSERT_EQUAL_STRING("ERROR:0:0: syntax error near unexpected token `done'", sx("done"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:0: syntax error near unexpected token `fi'", sx("fi"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:0: syntax error near unexpected token `)'", sx(")"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:0: syntax error near unexpected token `;'", sx(";"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:3: syntax error near unexpected token `;'", sx("a |;"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:3: syntax error near unexpected token `;'", sx("if ; then :; fi"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:6: syntax error near unexpected token `fi'", sx("if a; fi"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:15: syntax error near unexpected token `done'", sx("if false; then done; fi"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:2: syntax error near unexpected token `;;'", sx("a ;; b"));
    TEST_ASSERT_EQUAL_STRING("ERROR:0:4: syntax error near unexpected token `echo'", sx("f() echo"));
    TEST_ASSERT_EQUAL_STRING("ERROR:1:0: syntax error near unexpected token `)'", sx("a\n)"));
}

void test_error_position_tracks_lines(void) {
    AstParseResult r = parse_program("echo ok\necho ok\n  done\n", &arena);
    TEST_ASSERT_EQUAL_INT(PARSE_ERROR, r.status);
    TEST_ASSERT_EQUAL_UINT(2, r.line);
    TEST_ASSERT_EQUAL_UINT(2, r.col);
    TEST_ASSERT_NULL(r.root);
}

void test_node_positions(void) {
    AstParseResult r = parse_program("a\n  if b; then c; fi", &arena);
    TEST_ASSERT_EQUAL_INT(PARSE_OK, r.status);
    TEST_ASSERT_EQUAL_INT(NODE_LIST, r.root->type);
    TEST_ASSERT_EQUAL_UINT(2, r.root->u.list.items.count);
    Node *second = r.root->u.list.items.items[1].node;
    TEST_ASSERT_EQUAL_INT(NODE_PIPELINE, second->type);
    Node *if_node = second->u.pipeline.commands.items[0];
    TEST_ASSERT_EQUAL_INT(NODE_IF, if_node->type);
    TEST_ASSERT_EQUAL_UINT(1, if_node->line);
    TEST_ASSERT_EQUAL_UINT(2, if_node->col);
}

void test_redirect_struct_fields(void) {
    AstParseResult r = parse_program("cat <<-'X' 2>err\n\tbody\n\tX\n", &arena);
    TEST_ASSERT_EQUAL_INT(PARSE_OK, r.status);
    Node *cmd = r.root->u.list.items.items[0].node->u.pipeline.commands.items[0];
    TEST_ASSERT_EQUAL_UINT(2, cmd->redirs.count);
    TEST_ASSERT_EQUAL_INT(TOKEN_DLESSDASH, cmd->redirs.items[0].op);
    TEST_ASSERT_TRUE(cmd->redirs.items[0].heredoc_quoted);
    TEST_ASSERT_EQUAL_STRING("X", cmd->redirs.items[0].target);
    TEST_ASSERT_EQUAL_STRING("body\n", cmd->redirs.items[0].heredoc_body);
    TEST_ASSERT_EQUAL_INT(2, cmd->redirs.items[1].fd);
    TEST_ASSERT_EQUAL_STRING("err", cmd->redirs.items[1].target);
    TEST_ASSERT_NULL(cmd->redirs.items[1].heredoc_body);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_and_comment_only);
    RUN_TEST(test_simple_command);
    RUN_TEST(test_words_keep_quotes_and_expansions_raw);
    RUN_TEST(test_separators);
    RUN_TEST(test_and_or_left_associative);
    RUN_TEST(test_pipeline_and_negation);
    RUN_TEST(test_assignments_before_command_only);
    RUN_TEST(test_reserved_words_as_arguments);

    RUN_TEST(test_redirections);
    RUN_TEST(test_heredoc);
    RUN_TEST(test_heredoc_quoted_delimiter_and_tab_strip);
    RUN_TEST(test_two_heredocs_on_one_line_and_fd);
    RUN_TEST(test_heredoc_in_compound_and_pipeline);
    RUN_TEST(test_unterminated_heredoc_is_incomplete);

    RUN_TEST(test_if_one_line_and_multi_line);
    RUN_TEST(test_for_loop);
    RUN_TEST(test_while_until);
    RUN_TEST(test_case);
    RUN_TEST(test_brace_group_and_subshell);
    RUN_TEST(test_function_definition);
    RUN_TEST(test_nested_compounds);
    RUN_TEST(test_compound_in_pipeline_and_and_or);

    RUN_TEST(test_comments);
    RUN_TEST(test_line_continuations);
    RUN_TEST(test_escaped_separator_in_word);

    RUN_TEST(test_incomplete_compounds);
    RUN_TEST(test_incomplete_operators_and_quotes);

    RUN_TEST(test_syntax_errors);
    RUN_TEST(test_error_position_tracks_lines);
    RUN_TEST(test_node_positions);
    RUN_TEST(test_redirect_struct_fields);

    return UNITY_END();
}
