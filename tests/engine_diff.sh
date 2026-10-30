#!/bin/bash
# Differential check of the two command engines.
#
# Runs every smoosh test script (and the inline snippets below) with
# HASH_ENGINE=legacy and HASH_ENGINE=ast, and reports any case where stdout,
# stderr or the exit status differ. Used while the AST engine replaces the
# line-oriented one; delete together with the legacy engine.
#
# Usage: tests/engine_diff.sh [path/to/smoosh/tests]   (default: ./smoosh-tests/tests)

HASH_BIN="${HASH_BIN:-$(pwd)/hash-shell}"
SMOOSH="${1:-smoosh-tests/tests}"
export HASH_DISABLE_UPDATE_CHECK=1
same=0; diff_count=0

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

run_both() {  # name, command...   (each engine runs in its own fresh directory)
    local name="$1"; shift
    local dir_l dir_a out_l err_l ec_l out_a err_a ec_a
    dir_l=$(mktemp -d "$work/legacy.XXXXXX")
    dir_a=$(mktemp -d "$work/ast.XXXXXX")
    out_l=$(cd "$dir_l" && HASH_ENGINE=legacy "$@" 2>"$work/err_l"); ec_l=$?
    out_a=$(cd "$dir_a" && HASH_ENGINE=ast "$@" 2>"$work/err_a"); ec_a=$?
    # Mask what legitimately differs between the two runs: the directory names
    # (as given and as pwd -P resolves them), process ids and timestamps
    local real_l real_a
    real_l=$(cd "$dir_l" && pwd -P); real_a=$(cd "$dir_a" && pwd -P)
    out_l=$(printf '%s' "$out_l" | sed -E -e "s|$real_l|<cwd>|g" -e "s|$dir_l|<cwd>|g" -e 's/[0-9]{4,}/<n>/g')
    out_a=$(printf '%s' "$out_a" | sed -E -e "s|$real_a|<cwd>|g" -e "s|$dir_a|<cwd>|g" -e 's/[0-9]{4,}/<n>/g')
    err_l=$(sed -E -e "s|$real_l|<cwd>|g" -e "s|$dir_l|<cwd>|g" -e 's/[0-9]{4,}/<n>/g' "$work/err_l")
    err_a=$(sed -E -e "s|$real_a|<cwd>|g" -e "s|$dir_a|<cwd>|g" -e 's/[0-9]{4,}/<n>/g' "$work/err_a")
    rm -rf "$dir_l" "$dir_a"
    if [ "$out_l" = "$out_a" ] && [ "$err_l" = "$err_a" ] && [ "$ec_l" = "$ec_a" ]; then
        same=$((same + 1))
    else
        diff_count=$((diff_count + 1))
        echo "DIFF: $name"
        [ "$ec_l" = "$ec_a" ] || echo "  exit: legacy=$ec_l ast=$ec_a"
        [ "$out_l" = "$out_a" ] || { echo "  stdout legacy: [$(printf '%s' "$out_l" | head -c 200)]"; echo "  stdout ast:    [$(printf '%s' "$out_a" | head -c 200)]"; }
        [ "$err_l" = "$err_a" ] || { echo "  stderr legacy: [$(printf '%s' "$err_l" | head -c 200)]"; echo "  stderr ast:    [$(printf '%s' "$err_a" | head -c 200)]"; }
    fi
}

# Inline snippets (each line is one program run with -c)
while IFS= read -r snippet; do
    [ -n "$snippet" ] || continue
    run_both "-c $snippet" "$HASH_BIN" -c "$snippet"
done <<'SNIPPETS'
echo a; echo b
true && echo and || echo or
false && echo and || echo or
for i in 1 2 3; do echo $i; done
n=0; while [ $n -lt 3 ]; do n=$((n+1)); done; echo $n
case abc in a*) echo match;; *) echo no;; esac
f() { echo fn $1 $#; }; f one two
{ echo a; echo b; } | wc -l | tr -d ' '
(x=1); echo "[$x]"
X=1; X=2 sh -c 'echo $X'; echo $X
echo $((2*21)) $(echo sub) ~ "q $HOME" 'lit $HOME' | sed "s|$HOME|HOME|g"
if false; then :; fi; echo $?
for i in 1 2; do for j in a b; do echo $i$j; break 2; done; done
set -e; false; echo not-reached
echo visible # comment; echo hidden
echo foo\;#bar; echo after
exit 7
SNIPPETS

# Programs fed on standard input (the stdin command loop); \n escapes are
# expanded. Here-documents and continued lines are left out because the
# legacy loop does not support them.
while IFS= read -r program; do
    [ -n "$program" ] || continue
    run_both "stdin: $program" sh -c 'printf "%b" "$1" | "$2"' sh "$program" "$HASH_BIN"
done <<'PROGRAMS'
echo one > f.txt\ncat < f.txt\necho after-redirect\nexit 4
cat\nhello from the shell input
read x\nvalue\necho got=$x\necho after-read
echo one > f.txt\nread x < f.txt\necho x=$x
for i in 1 2\ndo\necho l$i\ndone\necho after-loop
exit 5
PROGRAMS

# smoosh test scripts
if [ -d "$SMOOSH/shell" ]; then
    export TEST_SHELL="$HASH_BIN" TEST_UTIL="$SMOOSH/util"
    for t in "$SMOOSH"/shell/*.test; do
        name=$(basename "$t" .test)
        run_both "$name" "$HASH_BIN" "$t"
    done
else
    echo "note: smoosh tests not found at $SMOOSH, inline snippets only"
fi

echo "engine_diff: $same identical, $diff_count different"
[ "$diff_count" -eq 0 ]
