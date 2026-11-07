#!/usr/bin/env bash
# Run modernish's regression suite against hash and compare the result with the
# recorded baseline, so the number of unexpected failures can only go down.
#
#   bash tests/modernish/run.sh [--shell PATH] [--update-baseline] [-- <modernish --test options>]
#
# modernish is fetched at the commit in MODERNISH_REF into tests/modernish/modernish-src
# (ignored by git). The suite runs as `hash-shell bin/modernish --test -eq`: -e skips the
# expensive tests, -q reports expected and unexpected failures only. Output lands in
# tests/modernish/results/ (ignored by git); the summary also goes to the GitHub step
# summary when there is one.
#
# baseline.txt records what the last accepted run looked like, one of
#   state=init-failed                   modernish refused to start on hash
#   state=died                          modernish initialised but died before any summary
#   state=ran total=N fails=F           N tests ran, F failed unexpectedly
# The states are ordered init-failed < died < ran. The script exits 1 when the run is
# worse than the baseline (a lower state, or more unexpected failures). When it is
# better, it says so and asks for --update-baseline, which rewrites the file; commit
# that with the change that earned it.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
shell_bin=$root/hash-shell
src=${MODERNISH_SRC:-$here/modernish-src}
results=${MODERNISH_RESULTS:-$here/results}
baseline=$here/baseline.txt
ref=$(tr -d '[:space:]' < "$here/MODERNISH_REF")
update=0
extra=()

while [ $# -gt 0 ]; do
    case $1 in
        --shell) shell_bin=$2; shift 2 ;;
        --update-baseline) update=1; shift ;;
        --) shift; extra=("$@"); break ;;
        -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "run.sh: unknown argument $1" >&2; exit 2 ;;
    esac
done

[ -x "$shell_bin" ] || { echo "run.sh: $shell_bin is not executable; build hash first or pass --shell" >&2; exit 2; }
shell_bin=$(cd "$(dirname "$shell_bin")" && pwd)/$(basename "$shell_bin")

# Fetch modernish at the pinned commit
if [ ! -d "$src/.git" ]; then
    mkdir -p "$src"
    git -C "$src" init -q
    git -C "$src" remote add origin https://github.com/modernish/modernish.git
fi
if [ "$(git -C "$src" rev-parse HEAD 2>/dev/null)" != "$ref" ]; then
    git -C "$src" fetch -q --depth 1 origin "$ref" || { echo "run.sh: cannot fetch modernish commit $ref" >&2; exit 2; }
    git -C "$src" checkout -q --detach FETCH_HEAD
fi

# A bound on the whole run; coreutils timeout, Homebrew gtimeout, or none
limit=()
if command -v timeout >/dev/null 2>&1; then
    limit=(timeout 1800)
elif command -v gtimeout >/dev/null 2>&1; then
    limit=(gtimeout 1800)
fi

mkdir -p "$results"
output=$results/output.txt
shell_version=$("$shell_bin" --version 2>/dev/null | head -1)

# What the suite ran under, for reading platform differences off the uploaded
# results: the environment, the default PATH modernish will use, which signals
# arrived ignored, and the tools it reaches for
{
    echo "shell: $shell_bin ($shell_version)"
    echo "uname: $(uname -a)"
    echo "getconf PATH: $(getconf PATH 2>&1)"
    echo "awk: $(command -v awk 2>&1) -> $(readlink -f "$(command -v awk 2>/dev/null)" 2>/dev/null)"
    [ -r /proc/self/status ] && grep -E '^Sig(Ign|Blk):' /proc/self/status
    echo "--- ulimit -a ---"; ulimit -a
    echo "--- env ---"; env | sort
} > "$results/environment.txt" 2>&1
echo "modernish $(git -C "$src" rev-parse --short HEAD) on ${shell_version:-$shell_bin}"
(cd "$src" && ${limit[@]+"${limit[@]}"} "$shell_bin" bin/modernish --test -eq ${extra[@]+"${extra[@]}"}) > "$output" 2>&1
status=$?

# Read the result
state=ran
total=0; oks=0; skips=0; warns=0; xfails=0; fails=0
# The summary decides first: a test may make a subshell print "modernish:
# ... died" (io.t's SIGPIPE check does on Linux) and the suite still finishes
if grep -q 'Initialisation failed' "$output"; then
    state=init-failed
elif grep -q '^Out of [0-9]* tests\{0,1\}:' "$output"; then
    total=$(sed -n -E 's/^Out of ([0-9]+) tests?:.*/\1/p' "$output" | tail -1)
    count() { sed -n -E "s/^- ([0-9]+) $1.*/\1/p" "$output" | tail -1; }
    oks=$(count succeeded); skips=$(count '(was|were) skipped'); warns=$(count 'produced warnings')
    xfails=$(count 'failed expectedly'); fails=$(sed -n -E 's/^.*- ([0-9]+) failed unexpectedly.*/\1/p' "$output" | tail -1)
    oks=${oks:-0}; skips=${skips:-0}; warns=${warns:-0}; xfails=${xfails:-0}; fails=${fails:-0}
elif grep -q 'modernish died\|^modernish: .*died' "$output"; then
    state=died
elif [ "$status" -ne 0 ]; then
    state=died            # initialised, then the shell or the suite stopped before any summary
else
    state=no-summary
fi

case $state in
    init-failed) summary="modernish could not initialise on hash (status $status): $(grep -m1 -vE 'Initialisation failed|^$' "$output" | head -c 200)" ;;
    died)        summary="modernish initialised but died before any summary (status $status): $(grep -m1 -vE '^getconf|^$' "$output" | head -c 200)" ;;
    no-summary)  summary="modernish produced no summary (status $status); see $output" ;;
    ran)         summary="modernish: $total tests, $oks succeeded, $skips skipped, $warns warnings, $xfails expected failures, $fails unexpected failures" ;;
esac
echo "$summary"
if [ "$state" = ran ] && [ "${fails:-0}" -gt 0 ]; then
    echo "unexpected failures:"
    grep -E 'FAIL' "$output" | grep -vE 'xfail' | sed 's/^/  /' | head -60
fi
printf '%s\n' "$summary" > "$results/summary.txt"

# Compare with the baseline
verdict=0
if [ -f "$baseline" ]; then
    # shellcheck disable=SC1090
    base_state=$(sed -n 's/^state=\([a-z-]*\).*/\1/p' "$baseline")
    base_fails=$(sed -n 's/.*fails=\([0-9]*\).*/\1/p' "$baseline")
    rank() { case $1 in init-failed) echo 0 ;; died) echo 1 ;; ran) echo 2 ;; *) echo -1 ;; esac; }
    if [ "$state" = ran ] && [ "$base_state" = ran ]; then
        if [ "$fails" -gt "${base_fails:-0}" ]; then
            echo "REGRESSION: $fails unexpected failures, baseline allows ${base_fails:-0}"; verdict=1
        elif [ "$fails" -lt "${base_fails:-0}" ]; then
            echo "better than the baseline (${base_fails:-0} -> $fails unexpected failures): rerun with --update-baseline and commit baseline.txt"
        else
            echo "matches the baseline ($fails unexpected failures)"
        fi
    elif [ "$(rank "$state")" -lt 0 ]; then
        echo "no summary to compare with the baseline; see $output"; verdict=1
    elif [ "$(rank "$state")" -gt "$(rank "$base_state")" ]; then
        echo "better than the baseline ($base_state -> $state): rerun with --update-baseline and commit baseline.txt"
    elif [ "$(rank "$state")" -lt "$(rank "$base_state")" ]; then
        echo "REGRESSION: the baseline is $base_state, this run is $state"; verdict=1
    else
        echo "matches the baseline ($state; see tests/modernish/STATUS.md)"
    fi
else
    echo "no baseline recorded; use --update-baseline to record this run"
fi

if [ "$update" -eq 1 ]; then
    if [ "$state" = ran ]; then
        printf 'state=ran total=%s fails=%s\n' "$total" "$fails" > "$baseline"
    elif [ "$state" = died ]; then
        printf 'state=died\n' > "$baseline"
    else
        printf 'state=init-failed\n' > "$baseline"
    fi
    echo "baseline written: $(cat "$baseline")"
    verdict=0
fi

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "## modernish"
        echo
        echo "$summary"
        echo
        [ -f "$baseline" ] && echo "Baseline: \`$(cat "$baseline")\`"
        [ "$verdict" -ne 0 ] && echo && echo "**Regression against the baseline.**"
    } >> "$GITHUB_STEP_SUMMARY"
fi
exit $verdict
