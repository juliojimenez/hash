# modernish against hash

[modernish](https://github.com/modernish/modernish) is the second conformance suite
after smoosh (#326). `bash tests/modernish/run.sh` fetches it at the commit in
`MODERNISH_REF`, runs `hash-shell bin/modernish --test -eq`, and compares the result
with `baseline.txt`. `.github/workflows/modernish.yml` does the same every Monday and
on changes to these files, and fails only when the result is worse than the baseline.

## Where things stand

All nine shell bugs that modernish's fatal-bug battery (`lib/modernish/adj/fatal.sh`)
found against v51 are fixed; the battery passes, and `bin/modernish` initialises on hash.
The regression suite itself then dies while loading its modules (#376), so `baseline.txt`
records `state=died`: the runner's middle state between `init-failed` and `ran`. Fixing
#376 should make the suite run; `run.sh` will then say "better than the baseline" and
ask for `--update-baseline` with the number of unexpected failures it reports. From then
on the weekly job turns red only when that number goes up.

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
