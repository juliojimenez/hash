# modernish against hash

[modernish](https://github.com/modernish/modernish) is the second conformance suite
after smoosh (#326). `bash tests/modernish/run.sh` fetches it at the commit in
`MODERNISH_REF`, runs `hash-shell bin/modernish --test -eq`, and compares the result
with `baseline.txt`. `.github/workflows/modernish.yml` does the same every Monday and
on changes to these files, and fails only when the result is worse than the baseline.

## Where things stand

modernish does not start on hash yet. Before it runs a single regression test it
checks the shell against a battery of fatal bugs (`lib/modernish/adj/fatal.sh`) and
aborts on the first one. Running that battery check by check against v51 found nine
distinct shell bugs, each filed with a reproducer:

| Issue | What fails | modernish check |
|-------|------------|-----------------|
| #347 | ~~`cd -- dir`, `cd -L`, `cd -P`~~ fixed | its very first command, `CDPATH='' cd -- "$dir" && pwd -P` |
| #348 | ~~`unalias -a`; `unalias a b`~~ fixed | FTL_UNALIASA |
| #349 | ~~`"$$"` in double quotes; `${$}`, `${?}`, `${!}`~~ fixed | FTL_HASHVAR |
| #350 | `command continue` / `break` / `return` / `exit` lose their control flow | FTL_EVALCOBR |
| #351 | `!` ignored on a pipeline after `&&` or `\|\|` | FTL_ORNOT |
| #352 | `\\` right before an operator swallows the operator | FTL_CASEBKSL2 |
| #353 | backslashes in unquoted and double-quoted `case` patterns | FTL_CASEBKSL |
| #354 | a 0x01 byte in a value is dropped with the next character | FTL_CC7F, FTL_CASECC |
| #355 | the word of `${var#word}` / `${var%word}` is not expanded | FTL_PSUB2, FTL_PSUB3, FTL_PSUBPP |

Once all nine are fixed, `run.sh` will report "better than the baseline"; rerun it with
`--update-baseline` and commit `baseline.txt` with the number of unexpected failures the
regression suite itself reports. From then on the weekly job turns red only when that
number goes up.

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
