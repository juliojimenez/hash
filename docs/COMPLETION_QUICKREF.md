# Tab Completion Quick Reference

## How To Use

| Action | Result |
|--------|--------|
| Type prefix + **TAB** | Complete if unique match |
| Type prefix + **TAB** **TAB** | Show all matches |

## What Gets Completed

| Position | Completes |
|----------|-----------|
| First word | Commands (PATH + built-ins + aliases) |
| Other words | Files and directories |

## Examples

### Commands
```bash
#> ec<TAB>       → echo
#> gi<TAB><TAB>  → git  gitk  ...
```

### Files
```bash
#> cat test<TAB>      → cat testfile.txt
#> cd Doc<TAB>        → cd Documents/
#> vim ~/.has<TAB>    → vim ~/.hashrc
```

### Multiple Matches

**First TAB:** Complete common prefix
```bash
#> cat te<TAB>
#> cat test
```

**Second TAB:** Show all
```bash
<TAB>
test.txt  test.md  testing.log
#> cat test
```

## Special Cases

✅ Directories get `/` appended  
✅ Space added after unique completion  
✅ Works with paths and tildes  
✅ Hidden files (start with `.`)  
✅ Handles `/usr/local/...` paths  

❌ No match → beep

---

Back to [README](../README.md)
