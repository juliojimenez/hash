# modernish against hash

[modernish](https://github.com/modernish/modernish) is the second conformance suite
after smoosh (#326). `bash tests/modernish/run.sh` fetches it at the commit in
`MODERNISH_REF`, runs `hash-shell bin/modernish --test -eq`, and compares the result
with `baseline.txt`. `.github/workflows/modernish.yml` does the same every Monday and
on changes to these files, and fails only when the result is worse than the baseline.

## Where things stand

The suite runs to its own summary, with the same result on macOS and on the GitHub
runner: `baseline.txt` records `state=ran total=389 fails=1` (389 tests, 364 succeeded,
12 skipped, 2 warnings, 10 expected failures, 1 unexpected failure). The weekly job turns
red only when the number of unexpected failures goes up; when a fix brings it down, rerun
with `--update-baseline` and commit `baseline.txt`.

`run.sh` exports `MSH_SHELL` so that modernish spawns hash for its subshells (it would
otherwise prefer the runner's bash), writes `results/environment.txt` (shell, uname,
`getconf PATH`, ignored signals, ulimit, env) next to the output, and with
`MODERNISH_TRACE=1` keeps a `-x` trace in `results/trace.txt`; the workflow uploads the
directory, so a failure that only happens on the runner can be read off the artifact. The
runner ignores SIGPIPE in every step, which is what exposed the `trap -` bug below.

Getting here (#376, after the battery work below) took parse-time alias expansion, a
`umask` builtin, `kill -l` with arguments, and a run of fixes the suite found as it got
further: `for` words under `set -f`, `read` status at end of file, unquoted `\\` and `\$`,
`"$@"` with zero or empty parameters, the job table filling up in scripts, a word beginning
with backslash-newline (`case \<newline>word \<newline>in`), protected bytes in a `case`
word (which made `shellquote` loop for ever), the 8 KB cap on a word's expansion, `${bad}`
as a bad substitution, `command` keeping quoted operators and skipping functions, a nested
`${...}` with quotes of its own inside another's word, and `trap -` keeping a signal that
arrived ignored ignored.

The one unexpected failure left, with the issue that tracks it (#399 listed the 43 of #381 by root
cause and every one of them has been fixed, in the order it gave; the pull requests are #404, #406,
#407, #410 to #418):

| Area | Tests | What fails | Issue |
|------|-------|------------|-------|
| `$@` corner | posparam.t 048 | `''"${novar-}$@$(:)"` with no parameters should be one empty field | #403 |

The two warnings (WRN_EREMBYTE, WRN_MULTIBYTE) are about multibyte characters in `ematch`
and IFS, as on other shells without full UTF-8 support. Also seen in the output, not
counted as failures: `pwd` after `cd -P` into a symlinked temp directory fails (`pwd: Not a
directory` on Linux; #397), and `$'...'` quoting, which hash parses as neither POSIX nor ANSI-C
(#388). Trap actions still run inside the signal handler (#398).

All nine shell bugs that modernish's fatal-bug battery (`lib/modernish/adj/fatal.sh`)
found against v51 are fixed; the battery passes, and `bin/modernish` initialises on hash.

Running the battery check by check against v51 found these nine bugs, each filed with a
reproducer; the fixes also turned up a dozen more (expansion, `case`, `$( )` parsing,
`exec`, function parameters) that are listed in the pull requests closing them:

| Issue | What fails | modernish check |
|-------|------------|-----------------|
| #347 | ~~`cd -- dir`, `cd -L`, `cd -P`~~ fixed | its very first command, `CDPATH='' cd -- "$dir" && pwd -P` |
| #348 | ~~`unalias -a`; `unalias a b`~~ fixed | FTL_UNALIASA |
| #349 | ~~`"$$"` in double quotes; `${$}`, `${?}`, `${!}`~~ fixed | FTL_HASHVAR |
| #350 | ~~`command continue` / `break` / `return` / `exit` lose their control flow~~ fixed | FTL_EVALCOBR |
| #351 | ~~`!` ignored on a pipeline after `&&` or `\|\|`~~ fixed: the real bug was `case` with an empty matched clause keeping the previous exit status | FTL_ORNOT |
| #352 | ~~`\\` right before an operator swallows the operator~~ fixed: the tokenizer was fine, the `case` word lost its backslash | FTL_CASEBKSL2 |
| #353 | ~~backslashes in unquoted and double-quoted `case` patterns~~ fixed | FTL_CASEBKSL |
| #354 | ~~a 0x01 byte in a value is dropped with the next character~~ fixed: real 0x01..0x04 bytes now travel as `\x01` + byte through every expansion step | FTL_CC7F, FTL_CASECC |
| #355 | ~~the word of `${var#word}` / `${var%word}` is not expanded~~ fixed | FTL_PSUB2, FTL_PSUB3, FTL_PSUBPP |

## Reproducing the battery check by check

`fatal.sh` exits at the first failing check and hides the shell's errors. To see every
failing check at once, run an instrumented copy that reports and continues:

```bash
src=tests/modernish/modernish-src
perl -pe 's/^(\s*)\( \* \)\s+exec 2>\/dev\/null ;;/$1( * ) : ;;/; s/\bexit\b/report \$LINENO/g' \
    "$src/lib/modernish/adj/fatal.sh" > /tmp/fatal_probe.sh
{ echo 'report() { echo "FATAL-CHECK-FAILED at fatal.sh line $1" >&3; }'; cat /tmp/fatal_probe.sh; } > /tmp/fatal_run.sh
(cd "$src" && ../../../hash-shell /tmp/fatal_run.sh 3>&1 >/dev/null)
```

Each reported line number points at a check in `fatal.sh`; the comment above the check
names the bug. dash reports nothing.

## Bumping modernish

Put the new commit in `MODERNISH_REF`, run `bash tests/modernish/run.sh`, and update
`baseline.txt` if the new tests change the count.
