#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

HASH_BIN="./hash-shell"
TEST_DIR="test_output"
PASSED=0
FAILED=0

# Portable timeout function
# Usage: run_with_timeout <seconds> <command> [args...]
run_with_timeout() {
    local timeout_duration="$1"
    shift

    # Check if GNU timeout is available (Linux, or macOS with coreutils)
    if command -v timeout &> /dev/null; then
        timeout "$timeout_duration" "$@"
        return $?
    # Check for gtimeout (macOS with Homebrew coreutils)
    elif command -v gtimeout &> /dev/null; then
        gtimeout "$timeout_duration" "$@"
        return $?
    else
        # Fallback: use perl (available on macOS by default)
        perl -e '
            use strict;
            use warnings;
            my $timeout = shift @ARGV;
            my $pid = fork();
            if ($pid == 0) {
                exec(@ARGV) or exit(127);
            }
            local $SIG{ALRM} = sub { kill("TERM", $pid); exit(124); };
            alarm($timeout);
            waitpid($pid, 0);
            alarm(0);
            exit($? >> 8);
        ' "$timeout_duration" "$@"
        return $?
    fi
}

setup() {
    echo "Setting up test environment..."
    mkdir -p "$TEST_DIR"

    # Build hash
    echo "Building hash..."
    make clean > /dev/null 2>&1
    make > /dev/null 2>&1

    if [[ ! -f "$HASH_BIN" ]]; then
        echo -e "${RED}FATAL: hash binary not found${NC}"
        exit 1
    fi

    echo -e "${GREEN}Setup complete${NC}\n"
    return 0
}

cleanup() {
    rm -rf "$TEST_DIR"
    return 0
}

run_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"

    echo -n "Testing: $test_name... "

    # Run command in hash shell and capture all output
    local output=$(printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" 2>&1)

    # Use grep -F for literal (fixed) string matching to avoid regex issues
    if echo "$output" | grep -qF "$expected"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected to find: $expected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    fi
}

run_command_test() {
    local test_name="$1"
    local command="$2"

    echo -n "Testing: $test_name... "

    # Run command, check exit code
    if printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Command failed or timed out"
        ((FAILED++))
        return 1
    fi
}

# Like run_test, but through the interactive prompt loop (-i), the path a real
# terminal session takes. HOME and HISTFILE point at the test directory so the
# run neither loads ~/.hashrc nor touches the real history file.
run_interactive_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"

    echo -n "Testing: $test_name... "

    local output=$(printf "%s\nexit\n" "$command" | \
        HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
        HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)

    if echo "$output" | grep -qF "$expected"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected to find: $expected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    fi
}

# Interactive variant that passes when the string is ABSENT from the output
run_interactive_test_absent() {
    local test_name="$1"
    local command="$2"
    local unexpected="$3"

    echo -n "Testing: $test_name... "

    local output=$(printf "%s\nexit\n" "$command" | \
        HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
        HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)

    if echo "$output" | grep -qF "$unexpected"; then
        echo -e "${RED}FAIL${NC}"
        echo "  Did not expect to find: $unexpected"
        echo "  Full output:"
        echo "$output" | head -20
        ((FAILED++))
        return 1
    else
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        return 0
    fi
}

run_file_test() {
    local test_name="$1"
    local command="$2"
    local output_file="$3"
    local expected_content="$4"

    echo -n "Testing: $test_name... "

    # Run command
    printf "%s\nexit\n" "$command" | run_with_timeout 2 "$HASH_BIN" > /dev/null 2>&1

    if [[ -f "$output_file" ]] && grep -q "$expected_content" "$output_file"; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
        rm -f "$output_file"
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  File not found or content mismatch"
        if [[ -f "$output_file" ]]; then
            echo "  File exists, content:"
            cat "$output_file"
        else
            echo "  File does not exist: $output_file"
        fi
        ((FAILED++))
        return 1
    fi
}

echo -e "${YELLOW}=== Hash Shell Test Suite ===${NC}\n"

setup

echo -e "${YELLOW}Basic Commands:${NC}"
run_test "echo command" "echo hello" "hello"
run_test "pwd command" "pwd" "/"
run_command_test "ls command" "ls"
run_command_test "date command" "date"

echo -e "\n${YELLOW}Built-in Commands:${NC}"
run_command_test "cd to /tmp" "cd /tmp"
run_command_test "cd to home (with tilde)" "cd ~"
run_command_test "cd to home (no args)" "cd"

# A variable directly followed by a quoted string keeps its name boundary (#374)
run_test "\$v\"ro\" expands the variable and appends the string" "v=ab; echo \$v\"ro\"" "abro"
run_test "\$v\"ro\" in an assignment" "v=ab; t=\$v\"ro\"; echo \"[\$t]\"" "[abro]"
run_test "\$v'ro' in an assignment" "v=ab; t=\$v'ro'; echo \"[\$t]\"" "[abro]"
run_test "\"\$v\"ro keeps the boundary too" "v=ab; t=\"\$v\"ro; echo \"[\$t]\"" "[abro]"
run_test "x\$v\"ro\" with a literal prefix" "v=ab; t=x\$v\"ro\"; echo \"[\$t]\"" "[xabro]"
run_test "positional and special parameters before a quote" "set -- one two; false; echo \"\$1\"x \$2\"y\" \$?\"z\"" "onex twoy 1z"
run_test "a quote after a variable that is not followed by one is unchanged" "v=ab; echo \$v \"\$v\" \$v\$v" "ab ab abab"

# A parameter inside arithmetic is substituted as text before evaluation (modernish's let)
run_test "a parameter holding an expression is evaluated inside \$(( ))" "f() { echo \$(( !(\$1) )); }; f '0 == 0'; x='1+2'; echo \$(( \$x ))" "0"
run_test "\$x holding an expression evaluates inside \$(( ))" "x='1+2'; echo \$(( \$x )) \$(( \${x} ))" "3 3"

# read assigns shell variables, not environment entries (#365)
run_test "read into a name a for loop used takes the new value" "for rv in a b; do :; done; echo fromread | { read rv; echo \"[\$rv]\"; }" "[fromread]"
run_test "read does not export the variable" "echo v | { read rx; env | grep -c '^rx='; }" "0"
run_test "read keeps an exported variable exported" "export re=old; echo new | { read re; env | grep '^re='; }" "re=new"
run_test "read with no name sets REPLY, unexported" "echo line | { read; echo \"[\$REPLY] \$(env | grep -c '^REPLY=')\"; }" "[line] 0"
run_test "read into a readonly variable fails and keeps the value" "readonly rr=keep; echo x | { read rr 2>/dev/null; echo \"st=\$? [\$rr]\"; }" "st=1 [keep]"

# Listing builtins fail when their output cannot be written; jobs prints nothing without jobs (#364)
run_test "jobs with no jobs prints nothing" "echo \"[\$(jobs)]\"" "[]"
run_test "jobs with no jobs still succeeds" "jobs; echo rc=\$?" "rc=0"
if [ -w /dev/full ]; then
    run_test "export fails when the write fails" "export >/dev/full 2>/dev/null; echo rc=\$?" "rc=1"
    run_test "set fails when the write fails" "set >/dev/full 2>/dev/null; echo rc=\$?" "rc=1"
    run_test "help fails when the write fails" "help >/dev/full 2>/dev/null; echo rc=\$?" "rc=1"
    run_test "kill -l fails when the write fails" "kill -l >/dev/full 2>/dev/null; echo rc=\$?" "rc=1"
    run_test "type fails when the write fails" "type echo >/dev/full 2>/dev/null; echo rc=\$?" "rc=1"
    run_test "a failed listing write names the builtin" "export >/dev/full" "export: write error"
fi

# ${var#word} / ${var%word}: the word is expanded, quoted parts match literally (#355)
run_test "\${t#*\"\$t2\"}: expansion inside a pattern word (modernish FTL_PSUB3)" "t=abcdefghij; t2=efg; echo \"\${t#*\"\$t2\"}\"" "hij"
run_test "\${1#\" \$2 -> \"}: positional parameters in a pattern word (FTL_PSUBPP)" "set -- ' foo -> bar' foo; echo \"\${1#\" \$2 -> \"}\"" "bar"
run_test "nested expansion in a pattern word is not re-expanded (FTL_PSUB2)" "t='a\${t2=BUG}'; unset -v t2; t=\${t%\"\${t#a}\"}; echo \"\$t,\${t2-}\"" "a,"
run_test "an unquoted expansion in a pattern word is a pattern" "t=abcdefghij; p='*'; echo \"[\${t##\$p}] [\${t##\"\$p\"}]\"" "[] [abcdefghij]"
run_test "\${t%/*} removes the shortest suffix" "t=/usr/local/bin/x; echo \"\${t%/*} \${t%%/*} \${t##*/} \${t#*/}\"" "/usr/local/bin  x usr/local/bin/x"
run_test "the shortest prefix match may be empty" "t=abc; echo \"[\${t#*}] [\${t##*}]\"" "[abc] []"
run_test "a } inside quotes in a pattern word" "t='a}b'; echo \"[\${t#\"a}\"}]\"" "[b]"
run_test "a lone [ in a pattern word is literal" "t='[a]bc'; echo \"[\${t#[}]\"" "[a]bc]"
run_test "an unquoted escaped backslash in a pattern word" "FOO='\\\\a'; echo \"[\${FOO#*\\\\\\\\}]\"" "[a]"

# Found while running modernish's battery (all present in v51)
run_test "arithmetic inside a double-quoted case word" "case \"\$((1+1))\" in (2) echo ARITH-CASE-OK;; (*) echo BUG;; esac" "ARITH-CASE-OK"
run_test "set -u allows an arithmetic assignment to an unset variable" "set -u; unset t; : \$((t = 1)); echo \"[\$t]\"" "[1]"
run_test "a command substitution runs the commands after an external one" "x=\$(/bin/echo a; echo b); set -- \$x; echo \"n=\$# \$1 \$2\"" "n=2 a b"
run_test "set -- inside a function does not touch the caller's arguments" "f() { set -- x y; echo \"\$# \$1 \$2\"; }; f a b; echo \"after \$#\"" "after 0"
run_test "exec with a command applies its redirections" "x=\$(exec echo a 2>/dev/null); echo \"[\$x]\"" "[a]"
run_test "a parenthesis inside quotes in a command substitution" "x=\$(echo \"(\"); echo \"[\$x]\"" "[(]"
run_test "a quote inside a comment in a command substitution" "eval ': || \$( : # \"
)' && echo CSCMTQUOT-OK" "CSCMTQUOT-OK"
run_test "an escaped brace in a default word" "u=1; x=\${u:+a\\}b}; echo \"[\$x]\"" "[a}b]"
run_test "double-quote escape rules in a quoted default word" "echo \"\${t:-\\\$x} \${t:-a\\}b}\"" "\$x a\\}b"
run_test "an unquoted expansion in a case pattern is a pattern" "p='[+-]*'; case --test in (\$p) echo CASE-PATTERN-OK;; (*) echo BUG;; esac" "CASE-PATTERN-OK"
run_test "a quote character from an expansion is literal in a case pattern" "foo=\\\"; case \"\$foo\" in \$foo ) echo QUOTE-CHAR-OK;; *) echo BUG;; esac" "QUOTE-CHAR-OK"
run_test "a variable name built from an expansion inside arithmetic" "v=b; _a_b=7; echo \$((_a_\${v} + 1))" "8"

# ${var:-word} words honour their own quotes; function arguments carry no markers (#375)
run_test "\${1+\"\$@\"} keeps each parameter whole" "f() { printf '<%s>' \${1+\"\$@\"}; echo; }; f 'a b' c" "<a b><c>"
run_test "\${w:-\"a b\"} is one field" "printf '[%s]' \${w:-\"a b\"}; echo" "[a b]"
run_test "\${z:-\$(cmd)} splits like an unquoted expansion" "printf '[%s]' \${z:-\$(echo a b)}; echo" "[a][b]"
run_test "\"\${y:-\$(cmd)}\" is one field" "printf '[%s]' \"\${y:-\$(echo a b)}\"; echo" "[a b]"
run_test "\${x:=\$(cmd)} stores the value clean" ": \${x:=\$(echo hi there)}; echo \"[\$x]\"" "[hi there]"
run_test "function arguments carry no quote markers" "f() { echo \"\${#1} \${#2}\"; }; f 'a;b' 'a*b'" "3 3"
run_test "\"\$@\" in a function keeps a quoted semicolon clean" "f() { printf 'm: %s|' \"\$@\"; }; f 'a; b' 'c d'" "m: a; b|m: c d|"

# Real 0x01..0x04 bytes in values, output and source text survive expansion (#354);
# the shell uses those bytes as internal markers and used to drop or misread them
CC_SETUP='CC01=$(printf "\001"); CC02=$(printf "\002"); CC03=$(printf "\003"); CC04=$(printf "\004"); CC7F=$(printf "\177"); RO=ro'
run_test "a 0x01 byte in a value is kept with the character after it" "$CC_SETUP; t=\${CC01}x; echo len=\${#t}" "len=2"
run_test "0x01 and 0x7f together are two characters (modernish FTL_CC7F)" "$CC_SETUP; t=\$RO\$CC01\$CC7F\$RO; echo len=\${#t}" "len=6"
run_test "0x02 0x03 0x04 in a value reach the command intact" "$CC_SETUP; t=a\${CC02}b\${CC03}c\${CC04}d; printf '%s' \"\$t\" | od -An -c | tr -s ' '" " a 002 b 003 c 004 d"
run_test "unquoted expansion keeps the bytes and does not split on them" "$CC_SETUP; t=a\${CC03}b\${CC04}c; set -- \$t; echo fields=\$#" "fields=1"
run_test "command substitution output keeps control bytes" "$CC_SETUP; t=\$(printf 'p\\001q\\003r'); echo len=\${#t}" "len=5"
run_test "case matches a value holding a control byte" "$CC_SETUP; case a\${CC01}b in (a?b) echo CC-CASE-OK;; (*) echo BUG;; esac" "CC-CASE-OK"
run_test "modernish FTL_CASECC check" "$CC_SETUP; eval \"case 'ab\${CC01}c\${CC7F}d' in ( \\\\\\\\a\\\\\\\\b\\\\\\\\\${CC01}\\\\\\\\c\\\\\\\\\${CC7F}\\\\\\\\d ) echo CASECC-OK ;; ( * ) echo BUG ;; esac\"" "CASECC-OK"
run_test "a control byte in source text is a literal" "$CC_SETUP; eval \"t='A\${CC01}B'\"; echo len=\${#t}" "len=3"
run_test "positional parameters keep control bytes through \"\$@\"" "$CC_SETUP; set -- \"a\${CC04}b\" c; for x in \"\$@\"; do printf '%s|' \"\$x\"; done | od -An -c | tr -s ' '" " a 004 b | c |"
run_test "a here-document keeps control bytes" "$CC_SETUP; cat <<EOF | od -An -c | tr -s ' '
H\${CC01}I\$CC03
EOF" " H 001 I 003 \\n"

# case words and patterns with backslashes and quoted pattern characters (#352, #353)
run_test "case: unquoted \\\\ in the word is one backslash" "case a\\\\b in (a\\\\b) echo CASE-OK1;; (*) echo BUG;; esac" "CASE-OK1"
run_test "case: backslash in a double-quoted pattern is literal" "case \\\\z in (\"\\z\") echo CASE-OK2;; (*) echo BUG;; esac" "CASE-OK2"
run_test "case: a backslash from a quoted expansion matches (modernish FTL_CASEBKSL2)" "set -- \\\; case ab\\\\cd in (*\"\$1\"*) echo CASE-OK3;; (*) echo BUG;; esac" "CASE-OK3"
run_test "case: single quotes keep \$ literal in the word" "x=5; case '\$x' in (5) echo BUG;; (*) echo CASE-OK4;; esac" "CASE-OK4"
run_test "case: a quoted * is not a pattern character" "case ab in (\"a*\") echo BUG;; (*) echo CASE-OK5;; esac" "CASE-OK5"
run_test "case: a quoted [ is not a pattern character" "case 'a[b' in ('a[b') echo CASE-OK6;; (*) echo BUG;; esac" "CASE-OK6"
run_test "case: two single-quoted backslashes match two" "case '\\\\\\\\' in ('\\\\\\\\') echo CASE-OK7;; (*) echo BUG;; esac" "CASE-OK7"

# case with an empty matched clause completes with status 0 (#351; modernish FTL_ORNOT
# saw `false || ! case x in x) ;; esac` negate the stale 1); eval of nothing succeeds
run_test "empty matched case clause has status 0" "false; case x in x) ;; esac; echo rc=\$?" "rc=0"
run_test "a case clause with commands still sees the previous status" "false; case x in x) echo inside=\$?;; esac" "inside=1"
run_test "! case after || negates the case's own status" "false || ! case x in x) ;; esac; echo rc=\$?" "rc=1"
run_test "modernish FTL_ORNOT check does not fire" "{ ! : || ! case x in x) ;; esac; } && echo FATAL; echo CHECK-DONE" "CHECK-DONE"
run_test "eval with null arguments succeeds" "false; eval ''; echo rc=\$?" "rc=0"
run_test "eval with blank text succeeds" "false; eval ' '; echo rc=\$?" "rc=0"

# command with the control-flow builtins (#350): break, continue, return and exit keep
# their effect through `command`, also inside eval
run_test "command continue continues the loop" "for i in 1 2; do command continue; echo BUG; done; echo CC-OK" "CC-OK"
run_test "command break leaves the loop" "for i in 1 2; do command break; echo BUG; done; echo CB-OK" "CB-OK"
run_test "command continue 2 skips the outer loop" "for i in 1 2; do for j in a b; do echo \$i\$j; command continue 2; done; done | tr '\n' ' '" "1a 2a "
run_test "command return returns from the function with its status" "f() { command return 3; echo BUG; }; f; echo st=\$?" "st=3"
run_test "command exit exits with its status" "sh_out=\$(\"$HASH_BIN\" -c 'command exit 4; echo BUG'); echo \"out=[\$sh_out] st=\$?\"" "out=[] st=4"
run_test "eval 'command continue' continues the loop" "for i in 1 2; do eval 'command continue'; echo BUG; done; echo EV-OK" "EV-OK"
run_test "command still bypasses functions" "echo() { builtin_echo_shadowed; }; command echo PLAIN-OK; unset -f echo" "PLAIN-OK"

# Special parameters (#349): the same with and without braces, in and out of quotes;
# $$ is the shell's own pid in every subshell it forks
run_test "\"\$\$\" in double quotes is the pid" "[ \"\$\$\" = \$\$ ] && echo QUOTED-PID-OK" "QUOTED-PID-OK"
run_test "\${\$} is the pid" "[ \"\${\$}\" = \$\$ ] && echo BRACED-PID-OK" "BRACED-PID-OK"
run_test "\${?} is the exit status" "false; echo rc=\${?}" "rc=1"
run_test "\"\${?}\" quoted is the exit status" "(exit 3); echo \"rc=\${?}\"" "rc=3"
run_test "\${!} is the last background pid" ": & [ \"\${!}\" = \"\$!\" ] && [ -n \"\$!\" ] && echo BG-OK; wait" "BG-OK"
run_test "\${#} \${@} \${*} in braces" "set -- a b; echo \"\${#}:\${@}:\${*}\"" "2:a b:a b"
run_test "\${-} in braces is the option letters" "set -f; case \"\${-}\" in *f*) echo DASH-OK;; esac" "DASH-OK"
run_test "modifiers work on special parameters" "false; echo \"\${?:-x} \${?:+set} \${#?}\"" "1 set 1"
run_test "\$\$ in a pipeline is the shell's pid" "echo \$\$ | { read p; [ \"\$p\" = \$\$ ] && echo PIPE-PID-OK; }" "PIPE-PID-OK"
run_test "\$\$ in a subshell and a command substitution is the shell's pid" "[ \"\$(echo \$\$)\" = \$\$ ] && ( [ \$\$ = \"\$PPID\" ] || echo SUB-PID-OK )" "SUB-PID-OK"
run_test "\$\$ in arithmetic is the shell's pid" "[ \$(( \$\$ )) = \$\$ ] && echo ARITH-PID-OK" "ARITH-PID-OK"

# cd options (#347): -L keeps the logical path, -P resolves symlinks, -- ends options,
# $PWD follows cd (it was stale when PWD came from the environment)
CD_FIX=$(mktemp -d /tmp/hash-cd-XXXXXX); mkdir -p "$CD_FIX/real/sub"; ln -s real "$CD_FIX/link"
CD_PHYS=$(cd "$CD_FIX" && pwd -P)
run_test "cd -- dir works" "cd -- $CD_PHYS/real && echo CD-OK-\$PWD" "CD-OK-$CD_PHYS/real"
run_test "cd keeps the symlink in PWD (-L default)" "cd $CD_PHYS/link && echo \$PWD" "$CD_PHYS/link"
run_test "cd -P resolves the symlink" "cd -P $CD_PHYS/link && echo \$PWD" "$CD_PHYS/real"
run_test "cd .. under a symlink goes to the link's parent" "cd $CD_PHYS/link/sub && cd ../.. && echo DOTDOT=\$PWD" "DOTDOT=$CD_PHYS"
run_test "cd -P -L: the last option wins" "cd -P -L $CD_PHYS/link && echo \$PWD" "$CD_PHYS/link"
run_test "cd - prints the directory it lands in" "cd $CD_PHYS/real; cd /usr; cd -" "$CD_PHYS/real"
run_test "cd -- - still means the previous directory" "cd /usr; cd $CD_PHYS; cd -- - >/dev/null; echo BACK=\$PWD" "BACK=/usr"
run_test "cd -x is an invalid option with status 2" "cd -x /usr; echo rc=\$?" "rc=2"
run_test "cd to a missing directory names it and fails with 1" "cd /no/such/dir/12345 2>&1; echo rc=\$?" "rc=1"
run_test "cd error message names the directory" "cd /no/such/dir/12345" "cd: /no/such/dir/12345: No such file or directory"
run_test "OLDPWD follows cd" "cd $CD_PHYS/real; cd /usr; echo OLD=\$OLDPWD" "OLD=$CD_PHYS/real"
rm -rf "$CD_FIX"
run_command_test "exit command" "exit"

echo -e "\n${YELLOW}Error Handling:${NC}"
run_test "invalid command" "this_command_does_not_exist_12345" "hash: this_command_does_not_exist_12345: command not found"

echo -e "\n${YELLOW}Edge Cases:${NC}"
run_command_test "empty command" ""
run_command_test "whitespace only" "   "

echo -e "\n${YELLOW}Command Substitution:${NC}"
run_test "basic command substitution" 'echo $(echo hello)' "hello"
run_test "command substitution with pwd" 'echo $(pwd)' "/"
run_test "command substitution in string" 'echo "prefix-$(echo middle)-suffix"' "prefix-middle-suffix"
run_test "backtick substitution" 'echo `echo backtick`' "backtick"

echo -e "\n${YELLOW}Quote Handling:${NC}"
run_test "double quotes" "echo \"hello world\"" "hello world"
run_test "single quotes" "echo 'hello world'" "hello world"
run_test "escaped double quote" "echo \"He said \\\"hello\\\"\"" 'He said "hello"'
run_test "escaped backslash" "echo \"path\\\\to\\\\file\"" "path\\to\\file"
run_test "mixed quotes" "echo \"double\" 'single'" "double"

echo -e "\n${YELLOW}Alias Functionality:${NC}"
run_command_test "create alias" "alias testcmd='echo test works'"
run_test "use alias" $'alias testcmd=\'echo aliasworks\'\ntestcmd' "aliasworks"
run_test "alias with args" $'alias ll=\'ls -la\'\nll .' "total"
run_command_test "list aliases" "alias"
run_command_test "remove alias" "alias testcmd='echo hi'; unalias testcmd"
run_test "unalias removes several names" "alias ua1=:; alias ua2=:; alias ua3=:; unalias ua1 ua2; alias | tr '\n' '|'" "alias ua3=':'|"
run_test "unalias -a removes every alias" "alias ua1=:; alias ua2=:; unalias -a; echo rc=\$?; alias | wc -l | tr -d ' '" "rc=0"
run_test "unalias -a leaves nothing" "alias ua1=:; unalias -a; echo LEFT=\$(alias | wc -l | tr -d ' ')" "LEFT=0"
run_test "unalias names a missing alias and still removes the others" "alias ua1=:; unalias nosuch_ua ua1 2>&1; echo rc=\$? left=\$(alias | wc -l | tr -d ' ')" "rc=1 left=0"
run_test "unalias missing alias message" "unalias nosuch_ua" "unalias: nosuch_ua: not found"
run_test "unalias without names is a usage error with status 2" "unalias 2>/dev/null; echo rc=\$?" "rc=2"
run_test "unalias -- ends the options" "alias ua1=:; unalias -- ua1; echo rc=\$? left=\$(alias | wc -l | tr -d ' ')" "rc=0 left=0"
# Aliases are expanded while parsing (POSIX): a definition takes effect for the
# lines that follow it, never for the line it is on, and the text may stand for
# reserved words
run_test "alias defined on the same line is not expanded" "alias zz1='echo no'; zz1" "zz1: command not found"
run_test "alias can stand for the ! keyword" $'alias not=\'! \'\nnot false; echo st=$?' "st=0"
run_test "alias ! keyword works in an if condition" $'alias not=\'! \'\nif not false; then echo taken; fi' "taken"
run_test "alias can open a while loop" $'alias forever=\'while :;\'\nn=0; forever do n=$((n+1)); [ $n -ge 3 ] && break; done; echo n=$n' "n=3"
run_test "alias can be a brace group" $'alias so=\'{ [ "$?" -eq 0 ]; }\'\ntrue; if so; then echo so-ok; fi' "so-ok"
run_test "alias whose text names itself expands once" $'alias ls=\'ls -d\'\nls /tmp' "/tmp"
run_test "alias ending in a blank checks the next word" $'alias say=\'echo \'; alias loud=\'LOUD\'\nsay loud' "LOUD"
run_test "alias without a trailing blank leaves the next word alone" $'alias say2=\'echo\'; alias loud2=\'LOUD\'\nsay2 loud2' "loud2"
run_test "quoted word is not an alias" $'alias qq=\'echo aliased\'\n\'qq\'' "qq: command not found"
run_test "alias in argument position is not expanded" $'alias arg1=\'echo aliased\'\necho arg1' "arg1"
run_test "mutually recursive aliases stop" $'alias r1=r2; alias r2=r1\nr1 2>&1; echo st=$?' "st=127"
run_test "alias after a variable assignment prefix" $'alias pa=\'echo prefixed\'\nX=1 pa' "prefixed"
run_test "alias inside a function body" $'alias not=\'! \'\nf() { not false; }\nf; echo st=$?' "st=0"

# kill -l translates between signal names and numbers
run_test "kill -l with a number prints the name" "kill -l 9" "KILL"
run_test "kill -l with several numbers" "kill -l 1 15 | tr '\n' ' '" "HUP TERM"
run_test "kill -l with a name prints the number" "echo n=\$(kill -l HUP)" "n=1"
run_test "kill -l with a SIG-prefixed name" "echo n=\$(kill -l SIGINT)" "n=2"
run_test "kill -l accepts an exit status above 128" "kill -l 129" "HUP"
run_test "kill -l rejects an unknown signal" "kill -l 999 2>/dev/null; echo rc=\$?" "rc=1"

# umask
run_test "umask prints the mask in octal" "umask 022; umask" "0022"
run_test "umask -S prints the symbolic form" "umask 027; umask -S" "u=rwx,g=rx,o="
run_test "umask takes a symbolic mode" "umask 022; umask g-w,o=; umask" "0027"
run_test "umask grants with a symbolic mode" "umask 0477; umask u+r; umask" "0077"
run_test "umask rejects an out of range number" "umask 999 2>/dev/null; echo rc=\$?" "rc=1"
run_test "umask applies to files the shell creates" "umask 077; : > $TEST_DIR/umask_file; ls -l $TEST_DIR/umask_file | cut -c2-10" "rw-------"

# Expansion corner cases found by modernish
run_test "quoted \$@ with no parameters is no field" "f() { /bin/echo \"[\$#]\" \"\$@\" x; }; f" "[0] x"
run_test "quoted \$@ keeps one empty parameter" "set -- ''; /bin/echo \"[\$#]\" \"\$@\" x" "[1]  x"
run_test "quoted \$@ keeps empty parameters in the middle" "set -- a '' b; printf '[%s]' \"\$@\"; echo" "[a][][b]"
run_test "for over an empty quoted \$@ runs no iteration" "set --; for w in \"\$@\"; do echo iterated; done; echo done" "done"
run_test "quoted \$@ splits when the first parameter has an equals sign" "f() { echo n=\$# second=\$2; }; g() { f \"\$@\"; }; g n=1 to three" "n=3 second=to"
run_test "quoted \$@ in an assignment joins with spaces" "set -- a b; x=\"\$@\"; echo \"[\$x]\"" "[a b]"
run_test "case word with a 0x03 byte does not match a 0x01 pattern" "c3=\$(printf '\\003'); c1=\$(printf '\\001'); v=\"a\${c3}b*c\"; case \$v in ( *\"\$c1\"* ) echo bug ;; ( * ) echo case-ok ;; esac" "case-ok"
run_test "command keeps a quoted operator as an argument" "command expr X '<' X" "0"
run_test "command skips a function of the same name" "f() { echo function; }; command f 2>&1; echo rc=\$?" "rc=127"
run_test "a 16 KB value expands whole" "v=x; i=0; while [ \$i -lt 14 ]; do v=\$v\$v; i=\$((i+1)); done; printf '%s' \"\$v\" | wc -c | tr -d ' '" "16384"
run_test "an invalid parameter name is a bad substitution" "echo \${.sh.version+s} 2>&1" "bad substitution"
run_test "a script may start more background jobs than the table holds" "i=0; while [ \$i -lt 80 ]; do ( : ) & i=\$((i+1)); done; wait; echo jobs-ok 2>&1" "jobs-ok"
run_test "for loop words honour set -f" "set -f; a='*'; for w in \$a; do echo w=\$w; done" "w=*"
run_test "read reports end of file without a newline" "printf abc | { read -r x; echo rc=\$? x=\$x; }" "rc=1 x=abc"
run_test "escaped backslash before a parameter keeps the backslash" $'V=star; W=\\\\${V}; printf \'%s\\n\' "$W"' '\star'
run_test "escaped dollar sign is literal" $'x=VAL; printf \'%s\\n\' \\$x' '$x'
run_test "escaped backslash then escaped dollar" $'x=VAL; printf \'%s\\n\' \\\\\\$x' '\$x'

echo -e "\n${YELLOW}File Operations:${NC}"
run_file_test "create file with echo" "echo test_content > $TEST_DIR/testfile.txt" "$TEST_DIR/testfile.txt" "test_content"

echo -e "\n${YELLOW}Interactive Prompt (-i):${NC}"
run_interactive_test "simple command" 'echo plain-ok' "plain-ok"
run_interactive_test "one-line for loop" 'for i in 1 2; do echo loop$i; done' "loop2"
run_interactive_test "one-line if" 'if true; then echo branch-yes; fi' "branch-yes"
run_interactive_test "multi-line for loop" $'for i in 1 2\ndo\necho multi$i\ndone' "multi2"
run_interactive_test "while loop" 'n=0; while [ $n -lt 2 ]; do n=$((n+1)); done; echo while$n' "while2"
run_interactive_test "case statement" 'case x in x) echo case-x;; esac' "case-x"
run_interactive_test "function definition and call" 'hi() { echo fn-called; }; hi' "fn-called"
run_interactive_test "brace group" '{ echo brace-a; echo brace-b; }' "brace-b"
run_interactive_test "exit code after loop" 'for i in 1; do false; done; echo ec=$?' "ec=1"
run_interactive_test "loop inside skipped branch keeps shell alive" $'if false\nthen\nfor i in 1 2\ndo\necho skipped\ndone\nfi\necho still-alive' "still-alive"
run_interactive_test "stray semicolon keeps shell alive" $';\necho still-alive' "still-alive"
run_interactive_test "one-line loop inside skipped branch" 'if false; then for i in 1 2; do echo skipped; done; fi; echo still-alive' "still-alive"
run_interactive_test "one-line loop inside taken branch" 'if true; then for i in 1 2; do echo taken$i; done; fi' "taken2"
run_interactive_test "incomplete eval keeps shell alive" $'eval \'if true\'\necho still-alive' "still-alive"
run_interactive_test_absent "semicolon inside comment is not a separator" 'echo visible # comment; echo should-not-run' "should-not-run"
run_interactive_test_absent "comment right after a semicolon" 'echo visible;# comment; echo should-not-run' "should-not-run"
run_interactive_test "syntax error inside if does not stick the prompt" $'if false; then done; fi\necho still-alive' "still-alive"
run_interactive_test_absent "function in skipped branch is not defined" 'f() { echo original; }; if false; then f() { echo replaced; }; fi; f' "replaced"
run_interactive_test "function in taken branch is defined" 'if true; then g() { echo defined; }; fi; g' "defined"
run_interactive_test_absent "command after function in skipped branch does not run" 'if false; then f() { :; } && echo leaked; fi; echo after' "leaked"
run_interactive_test "escaped semicolon before # is not a comment" 'echo first; echo foo\;#bar; echo after' "after"
run_interactive_test "escaped semicolon is literal" 'echo foo\;#bar' "foo;#bar"

run_interactive_test "line continuation across prompt lines" $'printf \'%s|\' a \\\n  b c; echo' "a|b|c|"

# Syntax errors name their source and line and exit 2: a script stops, eval and
# source report and let the caller go on. $LINENO is the current line.
echo -n "Testing: syntax error in a script names file and line, exits 2... "
printf 'echo one\nfi\necho three\n' > "$TEST_DIR/synerr.sh"
se_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/synerr.sh" 2>&1); se_ec=$?
se_want=$(printf "one\nhash: %s: line 2: syntax error near unexpected token \`fi'" "$TEST_DIR/synerr.sh")
if [[ "$se_out" == "$se_want" && "$se_ec" == "2" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got ec=$se_ec:"; echo "$se_out" | head -3; ((FAILED++))
fi

echo -n "Testing: syntax error under -c and on stdin names the line... "
c_out=$(run_with_timeout 2 "$HASH_BIN" -c 'echo a; fi' 2>&1); c_ec=$?
s_out=$(printf 'echo x\nfi\n' | run_with_timeout 2 "$HASH_BIN" 2>&1); s_ec=$?
c_want=$(printf "hash: -c: line 1: syntax error near unexpected token \`fi'")
s_want=$(printf "x\nhash: line 2: syntax error near unexpected token \`fi'")
if [[ "$c_out" == "$c_want" && "$c_ec" == "2" && "$s_out" == "$s_want" && "$s_ec" == "2" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  -c: [$c_out] ec=$c_ec; stdin: [$s_out] ec=$s_ec"; ((FAILED++))
fi

echo -n "Testing: eval and source report the error and the caller goes on... "
printf 'fi\n' > "$TEST_DIR/bad.sh"
ev_out=$(run_with_timeout 2 "$HASH_BIN" -c 'eval "fi"; echo "after=$?"' 2>&1)
so_out=$(run_with_timeout 2 "$HASH_BIN" -c "source $TEST_DIR/bad.sh; echo \"after=\$?\"" 2>&1)
ev_want=$(printf "hash: eval: line 1: syntax error near unexpected token \`fi'\nafter=2")
so_want=$(printf "hash: %s: line 1: syntax error near unexpected token \`fi'\nafter=2" "$TEST_DIR/bad.sh")
if [[ "$ev_out" == "$ev_want" && "$so_out" == "$so_want" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  eval: [$ev_out]"; echo "  source: [$so_out]"; ((FAILED++))
fi

echo -n "Testing: unexpected end of file names the last line... "
printf 'if true; then\necho x\n' > "$TEST_DIR/eof.sh"
eof_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/eof.sh" 2>&1); eof_ec=$?
if [[ "$eof_out" == "hash: $TEST_DIR/eof.sh: line 2: syntax error: unexpected end of file" && "$eof_ec" == "2" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$eof_out] ec=$eof_ec"; ((FAILED++))
fi

echo -n "Testing: \$LINENO in a script, a function and eval... "
printf 'echo "l1=$LINENO"\n\necho "l3=$LINENO $((LINENO))"\nf() {\n  echo "f=$LINENO"\n}\nf\neval '"'"'echo "e=$LINENO"'"'"'\necho "l9=$LINENO"\n' > "$TEST_DIR/lineno.sh"
ln_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/lineno.sh" 2>&1)
if [[ "$ln_out" == $'l1=1\nl3=3 3\nf=5\ne=1\nl9=9' ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got:"; echo "$ln_out"; ((FAILED++))
fi

# getopts: the POSIX option parser, checked against bash's output for the same
# script and arguments
cat > "$TEST_DIR/getopts.sh" <<'GETOPTS'
while getopts "ab:" opt; do
  case $opt in
    a) echo "A" ;;
    b) echo "B=$OPTARG" ;;
    ?) echo "bad opt=$opt OPTARG=${OPTARG-unset}" ;;
  esac
done
echo "OPTIND=$OPTIND"
shift $((OPTIND-1))
echo "rest=$*"
GETOPTS
sed 's/"ab:"/":ab:"/' "$TEST_DIR/getopts.sh" > "$TEST_DIR/getopts_silent.sh"
echo -n "Testing: getopts walks flags, arguments, clusters and -- ... "
g1=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" -a -b val x y 2>&1 | tr '\n' '|')
g2=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" -ab val -- -z 2>&1 | tr '\n' '|')
g3=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" -abval x 2>&1 | tr '\n' '|')
g4=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" x -a 2>&1 | tr '\n' '|')
if [[ "$g1" == "A|B=val|OPTIND=4|rest=x y|" && "$g2" == "A|B=val|OPTIND=4|rest=-z|" && "$g3" == "A|B=val|OPTIND=2|rest=x|" && "$g4" == "OPTIND=1|rest=x -a|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  g1=[$g1]"; echo "  g2=[$g2]"; echo "  g3=[$g3]"; echo "  g4=[$g4]"; ((FAILED++))
fi

echo -n "Testing: getopts reports unknown options and missing arguments... "
g5=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" -a -z x 2>&1 | tr '\n' '|')
g6=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts.sh" -b 2>&1 | tr '\n' '|')
g7=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts_silent.sh" -z x 2>&1 | tr '\n' '|')
g8=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/getopts_silent.sh" -a -b 2>&1 | tr '\n' '|')
if [[ "$g5" == "A|$TEST_DIR/getopts.sh: illegal option -- z|bad opt=? OPTARG=unset|OPTIND=3|rest=x|" && "$g6" == "$TEST_DIR/getopts.sh: option requires an argument -- b|bad opt=? OPTARG=unset|OPTIND=2|rest=|" && "$g7" == "bad opt=? OPTARG=z|OPTIND=2|rest=x|" && "$g8" == "A|bad opt=: OPTARG=b|OPTIND=3|rest=|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  g5=[$g5]"; echo "  g6=[$g6]"; echo "  g7=[$g7]"; echo "  g8=[$g8]"; ((FAILED++))
fi

run_test "getopts parses again after OPTIND=1" 'getopts "a" o -a; OPTIND=1; getopts "a" o -a; echo "o=$o OPTIND=$OPTIND"' "o=a OPTIND=2"
run_test "OPTIND starts at 1" 'echo "OPTIND=$OPTIND"' "OPTIND=1"
run_test "shift drops positional parameters" 'set -- a b c; shift; echo "$# $1"; shift 2; echo "$#"' "2 b"
echo -n "Testing: shift past the end fails and keeps the parameters... "
sh_out=$(run_with_timeout 2 "$HASH_BIN" -c 'set -- a b; shift 3; echo "st=$? n=$# first=$1"' 2>&1 | tr '\n' '|')
if [[ "$sh_out" == "hash: shift: shift count out of range|st=1 n=2 first=a|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$sh_out]"; ((FAILED++))
fi

# local: function variables, checked against bash's output for the same script
run_test "local shadows a global and restores it" 'x=global; f() { local x=inner; echo "in=$x"; }; f; echo "out=$x"' "out=global"
run_test "local without a value is unset in the function" 'x=g; f() { local x; echo "in=${x-unset}"; }; f; echo "out=$x"' "in=unset"
run_test "local is seen by called functions" 'g() { echo "g sees $v"; }; f() { local v=f; g; }; v=top; f; echo "after=$v"' "g sees f"
run_test "local restores an exported variable in the environment" 'export E=1; f() { local E=2; echo "env=$(printenv E)"; }; f; echo "after=$(printenv E)"' "after=1"
echo -n "Testing: local outside a function and on a readonly name fail... "
lo_out=$(run_with_timeout 2 "$HASH_BIN" -c 'local z=1; echo "st=$?"; readonly r=1; f() { local r=2; echo "st=$? r=$r"; }; f' 2>&1 | tr '\n' '|')
if [[ "$lo_out" == "hash: local: can only be used in a function|st=1|hash: local: r: readonly variable|st=1 r=1|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$lo_out]"; ((FAILED++))
fi

# set options, checked against bash's output for the same commands
echo -n "Testing: set -x traces commands after expansion... "
xt=$(run_with_timeout 2 "$HASH_BIN" -c 'set -x; echo hi; x=1; echo "$x two"; set +x; echo quiet' 2>&1 | tr '\n' '|')
if [[ "$xt" == "+ echo hi|hi|+ x=1|+ echo '1 two'|1 two|+ set +x|quiet|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$xt]"; ((FAILED++))
fi

echo -n "Testing: set -C refuses to clobber, >| overrides, devices are fine... "
rm -f "$TEST_DIR/ncf.txt"
nc=$(run_with_timeout 2 "$HASH_BIN" -c "set -C; echo a > $TEST_DIR/ncf.txt; echo b > $TEST_DIR/ncf.txt; echo \"st=\$?\"; cat $TEST_DIR/ncf.txt; echo c >| $TEST_DIR/ncf.txt; cat $TEST_DIR/ncf.txt; echo d >> $TEST_DIR/ncf.txt; echo e > /dev/null; echo \"null=\$?\"" 2>&1 | tr '\n' '|')
if [[ "$nc" == "hash: $TEST_DIR/ncf.txt: cannot overwrite existing file|st=1|a|c|null=0|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$nc]"; ((FAILED++))
fi

echo -n "Testing: set -n and -n on the command line parse without running... "
nx1=$(printf 'echo before\nset -n\necho after\n' | run_with_timeout 2 "$HASH_BIN" 2>&1 | tr '\n' '|')
printf 'echo not-run\n' > "$TEST_DIR/noexec.sh"
nx2=$(run_with_timeout 2 "$HASH_BIN" -n "$TEST_DIR/noexec.sh" 2>&1); nx2_ec=$?
if [[ "$nx1" == "before|" && -z "$nx2" && "$nx2_ec" == "0" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  stdin: [$nx1]; -n script: [$nx2] ec=$nx2_ec"; ((FAILED++))
fi

echo -n "Testing: set -v echoes input, -a exports, -f stops globbing... "
vb=$(printf 'set -v\necho hi\n' | run_with_timeout 2 "$HASH_BIN" 2>&1 | tr '\n' '|')
ae=$(run_with_timeout 2 "$HASH_BIN" -c 'set -a; FOO=bar; printenv FOO' 2>&1)
ng=$(cd "$TEST_DIR" && run_with_timeout 2 "../$HASH_BIN" -c 'set -f; echo *; set +f; echo * | grep -c "\*"' 2>&1 | tr '\n' '|')
if [[ "$vb" == "echo hi|hi|" && "$ae" == "bar" && "$ng" == "*|0|" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  -v: [$vb]; -a: [$ae]; -f: [$ng]"; ((FAILED++))
fi

echo -n "Testing: set -o lists, set +o prints commands, bad options exit 2... "
lo=$(run_with_timeout 2 "$HASH_BIN" -c 'set -ex; set -o | grep -c -E "^(errexit|xtrace)[[:space:]]+on$"; set +o | grep -c -E "^set -o (errexit|xtrace)$"; set +o | grep -c "^set +o noclobber$"' 2>/dev/null | tr '\n' '|')
run_with_timeout 2 "$HASH_BIN" -c 'set -z' >/dev/null 2>&1; bad_letter=$?
run_with_timeout 2 "$HASH_BIN" -c 'set -o bogus' >/dev/null 2>&1; bad_name=$?
if [[ "$lo" == "2|2|1|" && "$bad_letter" == "2" && "$bad_name" == "2" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  listing: [$lo]; set -z: $bad_letter; set -o bogus: $bad_name"; ((FAILED++))
fi

run_test "\$- shows the options in effect" 'set -ex; echo "flags=$-"' "flags=ex"
echo -n "Testing: -ex on the command line sets the options... "
cl=$(run_with_timeout 2 "$HASH_BIN" -ex -c 'echo "$-"' 2>/dev/null)
if [[ "$cl" == "ex" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$cl]"; ((FAILED++))
fi
run_test "set -h and set -b are accepted" 'set -h; set -b; echo "st=$?"' "st=0"

# exit N at the prompt, and end of input, leave the shell with the last status
echo -n "Testing: exit status at the prompt... "
printf 'exit 3\n' | HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i >/dev/null 2>&1; ec_exit=$?
printf 'false\n' | HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i >/dev/null 2>&1; ec_eof=$?
if [[ "$ec_exit" == "3" && "$ec_eof" == "1" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  exit 3 gave $ec_exit (want 3); end of input after false gave $ec_eof (want 1)"; ((FAILED++))
fi

# A script on standard input shares that input with the commands it runs: a
# redirection of descriptor 0 must not eat the lines after it, and a command
# can read what follows it in the script
echo -n "Testing: piped script keeps running after a stdin redirect... "
echo one > "$TEST_DIR/stdin_redirect.txt"
redirect_out=$(printf 'cat < %s\necho after-redirect\nexit 4\n' "$TEST_DIR/stdin_redirect.txt" | run_with_timeout 2 "$HASH_BIN" 2>&1); redirect_ec=$?
if [[ "$redirect_out" == $'one\nafter-redirect' && "$redirect_ec" == "4" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected: one, after-redirect, exit 4"; echo "  Got (exit $redirect_ec):"; echo "$redirect_out" | head -5; ((FAILED++))
fi

echo -n "Testing: command reads the rest of the piped script... "
share_out=$(printf 'cat\nhello from the input\n' | run_with_timeout 2 "$HASH_BIN" 2>&1)
if [[ "$share_out" == "hello from the input" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected: hello from the input"; echo "  Got:"; echo "$share_out" | head -5; ((FAILED++))
fi

# Blank lines typed inside an open compound command must survive into the body
echo -n "Testing: blank line inside loop heredoc is kept... "
heredoc_lines=$(printf 'for i in 1\ndo\ncat <<EOF\nline1\n\nline3\nEOF\ndone\nexit\n' | \
    HOME="$PWD/$TEST_DIR" HISTFILE="$PWD/$TEST_DIR/hash_history" PS1='$ ' \
    HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>/dev/null | wc -l | tr -d ' ')
if [[ "$heredoc_lines" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected 3 output lines, got $heredoc_lines"; ((FAILED++))
fi

# ~/.hashrc is a shell script: functions, conditionals and the hash options all
# take effect at startup, and none of its lines land in the history
echo -n "Testing: .hashrc functions and conditionals take effect... "
mkdir -p "$TEST_DIR/rchome"
printf '%s\n' "alias rc_alias='echo aliased'" 'rc_fn() {' '  echo "fn $1"' '}' 'if true; then' '  RC_IF=yes' 'fi' 'set welcome=off' > "$TEST_DIR/rchome/.hashrc"
rc_out=$(printf 'rc_fn one\necho "if=$RC_IF"\nrc_alias\nexit\n' | HOME="$PWD/$TEST_DIR/rchome" HISTFILE="$PWD/$TEST_DIR/rchome/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rc_out" | grep -qF "fn one" && echo "$rc_out" | grep -qF "if=yes" && echo "$rc_out" | grep -qF "aliased" && ! grep -qs 'rc_fn() {' "$TEST_DIR/rchome/hash_history"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rc_out" | head -10; echo "  History:"; cat "$TEST_DIR/rchome/hash_history" 2>/dev/null | head -5; ((FAILED++))
fi

# exit in .hashrc ends the shell before the prompt, with that status
echo -n "Testing: exit in .hashrc ends the shell with its status... "
mkdir -p "$TEST_DIR/rcexit"
printf 'exit 7\n' > "$TEST_DIR/rcexit/.hashrc"
rcx_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rcexit" HISTFILE="$PWD/$TEST_DIR/rcexit/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1); rcx_ec=$?
if [[ "$rcx_ec" == "7" ]] && ! echo "$rcx_out" | grep -qF "alive"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 7 and no prompt; got exit $rcx_ec:"; echo "$rcx_out" | head -5; ((FAILED++))
fi

# exit in a sourced file exits the shell
echo -n "Testing: exit in a sourced file exits the shell... "
printf 'exit 5\necho after-exit\n' > "$TEST_DIR/exit5.sh"
src_out=$(run_with_timeout 2 "$HASH_BIN" -c "source $TEST_DIR/exit5.sh; echo after-source" 2>&1); src_ec=$?
if [[ "$src_ec" == "5" && -z "$src_out" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 5 and no output; got exit $src_ec: $src_out"; ((FAILED++))
fi

# Special parameters inside $(( )): $# excludes $0, $? is the last status, and the
# count is right however deep the loop nesting
run_test "special parameters inside arithmetic" 'set -- a b; false; echo "q=$(( $? )) n=$(( $# )) b=$(( ${#} ))"' "q=1 n=2 b=2"
run_test "arithmetic \$# inside nested loops" 'set -- a b; for i in 1; do for j in 1; do for k in 1; do for l in 1; do for m in 1; do for n in 1; do for o in 1; do for p in 1; do for q in 1; do for r in 1; do echo "deep=$(( $# ))"; done; done; done; done; done; done; done; done; done; done' "deep=2"

# EXIT trap: runs under -c too, sees the exit status as $?, and keeps it unless
# the trap itself calls exit with an operand; a bare exit uses $?
echo -n "Testing: EXIT trap under -c keeps the exit status... "
trap_c_out=$(run_with_timeout 2 "$HASH_BIN" -c "trap 'echo bye' EXIT; exit 3" 2>&1); trap_c_ec=$?
trap_c2_out=$(run_with_timeout 2 "$HASH_BIN" -c "trap 'echo st=\$?; exit' EXIT; exit 4" 2>&1); trap_c2_ec=$?
if [[ "$trap_c_out" == "bye" && "$trap_c_ec" == "3" && "$trap_c2_out" == "st=4" && "$trap_c2_ec" == "4" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$trap_c_out] ec=$trap_c_ec (want bye, 3) and [$trap_c2_out] ec=$trap_c2_ec (want st=4, 4)"; ((FAILED++))
fi

echo -n "Testing: exit with an operand inside the EXIT trap replaces the status... "
run_with_timeout 2 "$HASH_BIN" -c "trap 'exit 5' EXIT; exit 3" >/dev/null 2>&1; trap_rep_ec=$?
run_with_timeout 2 "$HASH_BIN" -c "trap 'false' EXIT; exit 3" >/dev/null 2>&1; trap_keep_ec=$?
if [[ "$trap_rep_ec" == "5" && "$trap_keep_ec" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  exit 5 in trap gave $trap_rep_ec (want 5); false in trap gave $trap_keep_ec (want 3)"; ((FAILED++))
fi

echo -n "Testing: EXIT trap in a script and on stdin keeps the last status... "
printf "trap 'echo bye' EXIT\nfalse\n" > "$TEST_DIR/trap_false.sh"
trap_f_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/trap_false.sh" 2>&1); trap_f_ec=$?
trap_s_out=$(printf "trap 'echo bye' EXIT\nexit 3\n" | run_with_timeout 2 "$HASH_BIN" 2>&1); trap_s_ec=$?
if [[ "$trap_f_out" == "bye" && "$trap_f_ec" == "1" && "$trap_s_out" == "bye" && "$trap_s_ec" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  script: [$trap_f_out] ec=$trap_f_ec (want bye, 1); stdin: [$trap_s_out] ec=$trap_s_ec (want bye, 3)"; ((FAILED++))
fi

run_test "EXIT trap in a subshell keeps the subshell status" "(trap 'echo sub' EXIT; exit 6); echo \"s=\$?\"" "s=6"

echo -n "Testing: bare exit uses the last status... "
run_with_timeout 2 "$HASH_BIN" -c 'false; exit' >/dev/null 2>&1; bare_ec=$?
run_with_timeout 2 "$HASH_BIN" -c 'true; exit' >/dev/null 2>&1; bare0_ec=$?
if [[ "$bare_ec" == "1" && "$bare0_ec" == "0" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  false; exit gave $bare_ec (want 1); true; exit gave $bare0_ec (want 0)"; ((FAILED++))
fi

# A command that cannot be started names itself in the message and exits with
# the POSIX status: 127 when not found, 126 when it exists but cannot run
echo -n "Testing: command not found exits 127 with the name... "
nf_out=$(run_with_timeout 2 "$HASH_BIN" -c 'nosuchcmd_xyz_12345' 2>&1); nf_ec=$?
np_out=$(run_with_timeout 2 "$HASH_BIN" -c '/nonexistent/dir/prog' 2>&1); np_ec=$?
if [[ "$nf_out" == "hash: nosuchcmd_xyz_12345: command not found" && "$nf_ec" == "127" && "$np_out" == "hash: /nonexistent/dir/prog: No such file or directory" && "$np_ec" == "127" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$nf_out] ec=$nf_ec and [$np_out] ec=$np_ec"; ((FAILED++))
fi

echo -n "Testing: a file that cannot run exits 126... "
printf 'echo hi\n' > "$TEST_DIR/noexec.sh"; chmod 644 "$TEST_DIR/noexec.sh"
ne_out=$(run_with_timeout 2 "$HASH_BIN" -c "$TEST_DIR/noexec.sh" 2>&1); ne_ec=$?
dir_out=$(run_with_timeout 2 "$HASH_BIN" -c "./$TEST_DIR" 2>&1); dir_ec=$?
if [[ "$ne_out" == "hash: $TEST_DIR/noexec.sh: Permission denied" && "$ne_ec" == "126" && "$dir_out" == "hash: ./$TEST_DIR: is a directory" && "$dir_ec" == "126" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$ne_out] ec=$ne_ec and [$dir_out] ec=$dir_ec"; ((FAILED++))
fi

echo -n "Testing: 127 reaches set -e, || and command substitution... "
e_out=$(run_with_timeout 2 "$HASH_BIN" -c 'set -e; nosuchcmd_xyz_12345; echo after' 2>/dev/null); e_ec=$?
f_out=$(run_with_timeout 2 "$HASH_BIN" -c 'nosuchcmd_xyz_12345 || echo "fallback=$?"' 2>/dev/null)
s_out=$(run_with_timeout 2 "$HASH_BIN" -c 'v=$(nosuchcmd_xyz_12345 2>/dev/null); echo "st=$?"' 2>/dev/null)
if [[ -z "$e_out" && "$e_ec" == "127" && "$f_out" == "fallback=127" && "$s_out" == "st=127" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  set -e: [$e_out] ec=$e_ec; ||: [$f_out]; cmdsub: [$s_out]"; ((FAILED++))
fi

echo -n "Testing: exec of a missing command exits 127 and ends the shell... "
x_out=$(run_with_timeout 2 "$HASH_BIN" -c 'exec nosuchcmd_xyz_12345; echo after' 2>&1); x_ec=$?
if [[ "$x_out" == "hash: exec: nosuchcmd_xyz_12345: not found" && "$x_ec" == "127" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got [$x_out] ec=$x_ec"; ((FAILED++))
fi

# The shell removes the quotes around an alias value; quotes inside it stay
run_test "alias keeps the quotes inside its value" $'alias greet="\'printf\' \'%s\\n\' \'hello world\'"\ngreet' "hello world"

# return in .hashrc ends the file, and only the file
echo -n "Testing: return in .hashrc ends only the file... "
mkdir -p "$TEST_DIR/rcreturn"
printf 'RC=1\nreturn\nRC=2\n' > "$TEST_DIR/rcreturn/.hashrc"
rcr_out=$(printf 'echo "rc=$RC"\nif true; then echo if-ran; fi\nexit\n' | HOME="$PWD/$TEST_DIR/rcreturn" HISTFILE="$PWD/$TEST_DIR/rcreturn/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rcr_out" | grep -qF "rc=1" && echo "$rcr_out" | grep -qF "if-ran"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rcr_out" | head -5; ((FAILED++))
fi

# source inside an EXIT trap does not cut the trap short
echo -n "Testing: source inside an EXIT trap runs the rest of the trap... "
printf 'true\n' > "$TEST_DIR/true.sh"
printf "trap 'source %s; echo finished' EXIT\nexit 3\n" "$TEST_DIR/true.sh" > "$TEST_DIR/trap_source.sh"
trap_out=$(run_with_timeout 2 "$HASH_BIN" "$TEST_DIR/trap_source.sh" 2>&1)
if echo "$trap_out" | grep -qF "finished"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected 'finished'; got: $trap_out"; ((FAILED++))
fi

# A login shell keeps the requested exit status across its logout file, unless
# the logout file exits itself
echo -n "Testing: logout file keeps the exit status unless it exits... "
mkdir -p "$TEST_DIR/rclogin"
printf 'exit 7\n' > "$TEST_DIR/rclogin/.hashrc"
printf 'true\n' > "$TEST_DIR/rclogin/.hash_logout"
printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogin" HISTFILE="$PWD/$TEST_DIR/rclogin/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i >/dev/null 2>&1; lo_keep=$?
printf 'exit 3\n' > "$TEST_DIR/rclogin/.hash_logout"
printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogin" HISTFILE="$PWD/$TEST_DIR/rclogin/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i >/dev/null 2>&1; lo_exit=$?
if [[ "$lo_keep" == "7" && "$lo_exit" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  logout 'true' gave $lo_keep (want 7); logout 'exit 3' gave $lo_exit (want 3)"; ((FAILED++))
fi

# A file sourced from .hashrc does not end .hashrc: a return after it still works
echo -n "Testing: return after a nested source in .hashrc... "
mkdir -p "$TEST_DIR/rcnested"
printf 'S=1\n' > "$TEST_DIR/rcnested/settings.sh"
printf 'source %s\nreturn\necho not-reached\n' "$TEST_DIR/rcnested/settings.sh" > "$TEST_DIR/rcnested/.hashrc"
rcn_out=$(printf 'echo "s=$S"\nexit\n' | HOME="$PWD/$TEST_DIR/rcnested" HISTFILE="$PWD/$TEST_DIR/rcnested/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if echo "$rcn_out" | grep -qF "s=1" && ! echo "$rcn_out" | grep -qF "not-reached" && ! echo "$rcn_out" | grep -qF "can only"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Output:"; echo "$rcn_out" | head -5; ((FAILED++))
fi

# logout in a login shell's .hashrc ends the shell before the prompt
echo -n "Testing: logout in .hashrc of a login shell skips the prompt... "
mkdir -p "$TEST_DIR/rclogout"
printf 'logout\necho rc-after\n' > "$TEST_DIR/rclogout/.hashrc"
lo_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rclogout" HISTFILE="$PWD/$TEST_DIR/rclogout/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -l -i 2>&1); lo_ec=$?
if [[ "$lo_ec" == "0" ]] && ! echo "$lo_out" | grep -qE "alive|rc-after"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 0 with no prompt; got exit $lo_ec: $lo_out"; ((FAILED++))
fi

# exit inside a condition in .hashrc still ends the shell with its status
echo -n "Testing: exit inside an if condition in .hashrc ends the shell... "
mkdir -p "$TEST_DIR/rcifexit"
printf 'if exit 7; then\n:\nfi\necho after\n' > "$TEST_DIR/rcifexit/.hashrc"
ife_out=$(printf 'echo alive\n' | HOME="$PWD/$TEST_DIR/rcifexit" HISTFILE="$PWD/$TEST_DIR/rcifexit/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1); ife_ec=$?
if [[ "$ife_ec" == "7" ]] && ! echo "$ife_out" | grep -qE "alive|after"; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  Expected exit 7 and no output; got exit $ife_ec: $ife_out"; ((FAILED++))
fi

# Function nesting is limited (FUNCNEST, default 1000; eval and . count too), so
# unbounded recursion fails with a message and status 1 instead of overflowing
# the stack. A non-interactive shell stops there, as zsh does. Sanitizer builds
# use far more stack per level and hit the stack guard first, whose message
# names the depth instead of the limit, so only the FUNCNEST=20 checks compare
# the whole line.
echo -n "Testing: unbounded recursion fails with a message, status 1... "
r_out=$(run_with_timeout 10 "$HASH_BIN" -c 'f() { f; }; f; echo never' 2>&1); r_ec=$?
if [[ "$r_out" == *"hash: f: maximum function nesting level exceeded"* && "$r_out" != *never* && "$r_ec" == "1" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got ec=$r_ec: $r_out" | head -3; ((FAILED++))
fi

echo -n "Testing: FUNCNEST sets the limit; eval and . count as levels... "
n_out=$(run_with_timeout 10 "$HASH_BIN" -c 'FUNCNEST=20; f() { f; }; f' 2>&1); n_ec=$?
e_out=$(run_with_timeout 10 "$HASH_BIN" -c 'FUNCNEST=20; e() { eval e; }; e' 2>&1); e_ec=$?
printf '. "$0"\n' > "$TEST_DIR/self.sh"
d_out=$(FUNCNEST=20 run_with_timeout 10 "$HASH_BIN" "$TEST_DIR/self.sh" 2>&1); d_ec=$?
if [[ "$n_out" == "hash: f: maximum function nesting level exceeded (20)" && "$n_ec" == "1" \
      && "$e_out" == *"maximum function nesting level exceeded (20)"* && "$e_ec" == "1" \
      && "$d_out" == "hash: .: maximum function nesting level exceeded (20)" && "$d_ec" == "1" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  f: [$n_out] ec=$n_ec; eval: [$e_out] ec=$e_ec; dot: [$d_out] ec=$d_ec"; ((FAILED++))
fi

run_test "recursion within the limit still works" 'n=0; f() { n=$((n+1)); [ $n -lt 200 ] && f; }; f; echo "depth $n"' "depth 200"
# FUNCNEST=20 keeps this well inside the 2 s budget of run_interactive_test on a
# loaded runner; the default limit is covered by the -c case above
run_interactive_test "recursion limit keeps an interactive session going" $'FUNCNEST=20\nf() { f; }\nf\necho alive $?' "alive 1"

# A compound command typed over several lines is one history entry, saved in the
# extended format (a "#<seconds>" line before each entry) and loaded back whole.
echo -n "Testing: multi-line command is one history entry, saved and reloaded... "
rm -rf "$TEST_DIR/mlhist"; mkdir -p "$TEST_DIR/mlhist"
ml1=$(printf 'if true; then\necho yes\nfi\nhistory\nexit\n' | HOME="$PWD/$TEST_DIR/mlhist" HISTFILE="$PWD/$TEST_DIR/mlhist/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
ml_markers=$(grep -c '^#[0-9][0-9]*$' "$TEST_DIR/mlhist/hash_history")   # three entries: the if, history, exit
ml2=$(printf 'history\nexit\n' | HOME="$PWD/$TEST_DIR/mlhist" HISTFILE="$PWD/$TEST_DIR/mlhist/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
ml_want=$(printf '    0  if true; then\necho yes\nfi\n    1  history')
if [[ "$ml1" == *"$ml_want"* && "$ml1" != *"1  echo yes"* && "$ml2" == *"$ml_want"* && "$ml_markers" == "3" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  session 1:"; echo "$ml1" | head -8; echo "  session 2:"; echo "$ml2" | head -8; echo "  file:"; cat "$TEST_DIR/mlhist/hash_history" | head -8; ((FAILED++))
fi

echo -n "Testing: a history file in the old one-line format still loads... "
rm -rf "$TEST_DIR/oldhist"; mkdir -p "$TEST_DIR/oldhist"
printf 'echo old-one\necho old-two\n' > "$TEST_DIR/oldhist/hash_history"
oh=$(printf 'history\nexit\n' | HOME="$PWD/$TEST_DIR/oldhist" HISTFILE="$PWD/$TEST_DIR/oldhist/hash_history" PS1='$ ' HASH_DISABLE_UPDATE_CHECK=1 run_with_timeout 2 "$HASH_BIN" -i 2>&1)
if [[ "$oh" == *"0  echo old-one"* && "$oh" == *"1  echo old-two"* && "$oh" == *"2  history"* ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "$oh" | head -6; ((FAILED++))
fi

# complete: programmable completion specs are recorded, printed back in a form
# that can be typed again, and removed
run_test "complete -W records and prints a spec" "complete -d -W 'alpha beta' hctool; complete -p hctool" "complete -d -W 'alpha beta' hctool"
run_test "complete -F records a function spec" "complete -F _hc -o default hcother; complete hcother" "complete -o default -F _hc hcother"
run_test "complete -r removes a spec" "complete -W x hcgone; complete -r hcgone; complete -p hcgone; echo \"status=\$?\"" "status=1"
run_test "complete rejects an unknown option" "complete -Z x; echo \"status=\$?\"" "status=2"

# set -o vi and set -o emacs are the two states of the editing mode
run_test "set -o vi turns emacs off" "set -o vi; set -o | grep -E '^(vi|emacs)' | tr -s ' \t' ' ' | tr '\n' ' '" "emacs off vi on"
run_test "set +o vi restores emacs" "set -o vi; set +o vi; set +o | grep -E 'emacs|vi' | tr '\n' ' '" "set -o emacs set +o vi"

# help: every builtin and keyword has a synopsis; help NAME describes one
run_test "help lists trap and the keywords" "help | grep -E '^(trap|if|case) ' | wc -l | tr -d ' '" "4"
run_test "help NAME prints synopsis, description and the book page" "help trap" "More: https://hash-shell.org/"
run_test "help -s prints the synopsis only" "help -s cd exit | tr '\n' '|'" "cd [-L|-P] [dir]|exit [n]|"
run_test "help takes patterns" "help -s 'ex*' | head -1" "exit [n]"
run_test "help set lists hash's own options" "help set" "welcome=on|off"
run_test "help with an unknown topic fails" "help nosuchthing; echo \"status=\$?\"" "status=1"
run_test "help rejects an unknown option" "help -z; echo \"status=\$?\"" "status=2"

# Did you mean: an interactive shell suggests the closest command after
# "command not found"; a non-interactive one does not, and the status stays 127
run_interactive_test "unknown command suggests the closest name" 'ehco hi; echo "st=$?"' "Did you mean: echo?"
run_interactive_test "the status after a suggestion is still 127" 'ehco hi; echo "st=$?"' "st=127"
echo -n "Testing: no suggestion in a non-interactive shell... "
ni_out=$(run_with_timeout 2 "$HASH_BIN" -c 'ehco hi' 2>&1); ni_ec=$?
if [[ "$ni_out" == "hash: ehco: command not found" && "$ni_ec" == "127" ]]; then
    echo -e "${GREEN}PASS${NC}"; ((PASSED++))
else
    echo -e "${RED}FAIL${NC}"; echo "  got ec=$ni_ec: $ni_out"; ((FAILED++))
fi

# Prompt presets: set prompt=NAME, the listing, and a rejected name
run_test "set prompt=NAME picks a preset" "set prompt=minimal; echo \"status=\$?\"" "status=0"
run_test "set prompt=list names every preset and marks the current one" "set prompt=two-line; set prompt=list | grep -oE '^[* ] [a-z-]+' | tr -s ' ' | tr '\n' '|'" "classic| plain| minimal|* two-line| powerline|"
run_test "an unknown preset fails without touching the parameters" "set -- a b; set prompt=nosuch 2>/dev/null; echo \"status=\$? params=\$#\""  "status=1 params=2"
run_test "an unknown preset names the presets" "set prompt=nosuch 2>&1" "the presets are classic, plain, minimal, two-line, powerline"

cleanup

echo -e "\n${YELLOW}=== Test Summary ===${NC}"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo -e "Total:  $((PASSED + FAILED))"

if [[ $FAILED -eq 0 ]]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
