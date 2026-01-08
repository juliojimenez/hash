# Tab Completion in Hash Shell

Hash shell includes intelligent tab completion for commands, files, directories, and more.

## Basic Usage

### Command Completion

Press TAB after typing part of a command:

```bash
#> ec<TAB>
#> echo     # Completed!

#> gi<TAB>
#> git     # Completed from PATH

#> al<TAB>
#> alias   # Built-in command
```

### File/Directory Completion

Press TAB for files and directories:

```bash
#> cat test<TAB>
#> cat testfile.txt    # Completed!

#> cd Doc<TAB>
#> cd Documents/       # Directory with trailing /

#> ls ~/pro<TAB>
#> ls ~/projects/      # Works with tilde
```

### Multiple Matches

**First TAB** - Complete common prefix:
```bash
#> cat test<TAB>
#> cat test            # Common prefix "test"
# (testfile.txt, test.md, test_data.csv all exist)
```

**Second TAB** - Show all matches (basenames only):
```bash
#> cat test<TAB><TAB>
testfile.txt  test.md  test_data.csv  testing.log
#> cat test             # Line redisplayed

#> ls /usr/local/b<TAB><TAB>
bash  brew  bundle        # Shows basenames, not full paths
#> ls /usr/local/b
```

### No Matches

If no matches found, you'll hear a beep (bell character).

## What Gets Completed

### First Word (Command)

Completes from:
1. **Built-in commands** - cd, exit, alias, etc.
2. **Aliases** - Your custom aliases from .hashrc
3. **Executables in PATH** - All commands in $PATH directories

**Example:**
```bash
#> g<TAB><TAB>
ga    gc    gd    git    gl    gp    gs
# Shows: aliases (ga, gc, etc.) and git
```

### Subsequent Words (Files/Directories)

Completes files and directories from:
- Current directory
- Specified path
- Tilde-expanded paths

**Features:**
- Directories get trailing `/`
- Hidden files shown if you type `.`
- Relative and absolute paths work

**Examples:**
```bash
#> cat /<TAB>
bin/  boot/  dev/  etc/  home/  ...

#> cd ~/.c<TAB>
#> cd ~/.config/

#> vim src/m<TAB>
#> vim src/main.c
```

## Advanced Features

### Slash Handling

Hash is smart about directory separators:

```bash
#> cd /home/user/D<TAB>
#> cd /home/user/Documents/

#> ls ~/projects/hash/src/m<TAB>
#> ls ~/projects/hash/src/main.c
```

### Tilde Expansion in Completion

```bash
#> cat ~/.<TAB>
~/.bashrc  ~/.config/  ~/.hash_history  ~/.hashrc  ...
```

### Completing Partial Paths

```bash
#> cd /u/l/b<TAB>
#> cd /usr/local/bin/
```

### Case Sensitivity

Completion is case-sensitive (like bash):

```bash
#> cat Test<TAB>   # Won't match "test.txt"
#> cat test<TAB>   # Matches "test.txt"
```

## Behavior Details

### Single Match

When there's exactly one match:
- Completes the full name
- Adds a space after completion (ready for next arg)
- Directories get `/` appended

```bash
#> cat uniq<TAB>
#> cat unique_file.txt |   # Space added, ready to type
```

### Multiple Matches

**First TAB:**
- Completes the common prefix
- Waits for more input or second TAB

**Second TAB:**
- Shows all matches in columns (basenames only for cleaner display)
- Redraws your line
- You can continue typing to narrow down

**Example:**
```bash
#> cat t<TAB>
#> cat te           # Common prefix "te"

<TAB>
test.txt  temp.log  template.md
#> cat te           # Continue typing...

#> cat tem<TAB>
#> cat template.md  # Now unique!
```

### No Matches

- Beeps (bell character)
- Line unchanged
- Continue typing or try different completion

## Completion Contexts

### After Operators

Completion works after command chaining operators:

```bash
#> ls && ca<TAB>
#> ls && cat 

#> cd /tmp ; pw<TAB>
#> cd /tmp ; pwd
```

### In Quotes

Currently completes within quotes (may refine in future):

```bash
#> echo "test<TAB>
#> echo "testfile.txt
```

### With Variables

```bash
#> export DIR=projects
#> cd $DIR/ha<TAB>
#> cd $DIR/hash/
```

## Limitations

Current limitations (may be added later):
- ❌ No variable name completion (after `$`)
- ❌ No username completion (after `~`)
- ❌ No command-specific completion (git subcommands, etc.)
- ❌ No programmable completion
- ❌ No custom completion functions

## Tips & Tricks

### Quick Directory Navigation

```bash
#> cd /u/l/b<TAB>
#> cd /usr/local/bin/
```

### Completing Long Names

```bash
#> cat very-long-filename-t<TAB>
#> cat very-long-filename-that-is-hard-to-type.txt
```

### Exploring Directories

```bash
#> ls /etc/<TAB><TAB>
# Shows all files in /etc/

#> cd ~/.<TAB><TAB>  
# Shows all hidden files/dirs in home
```

### Command Discovery

```bash
#> git<TAB><TAB>
git  git-shell  gitk  ...
# See all git-related commands
```

## Performance

Tab completion is optimized for speed:
- ✅ Caches nothing (always up-to-date)
- ✅ Stops searching after MAX_COMPLETIONS (256)
- ✅ Only scans relevant directories
- ✅ Minimal memory allocation

Large directories may take a moment, but completion is generally instant.

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| Command completion | ✅ | ✅ |
| File/directory completion | ✅ | ✅ |
| Alias completion | ✅ | ✅ |
| Path completion | ✅ | ✅ |
| Double-TAB shows matches | ✅ | ✅ |
| Common prefix completion | ✅ | ✅ |
| Variable completion | ❌ | ✅ |
| Programmable completion | ❌ | ✅ |
| Git-aware completion | ❌ | ✅ (via scripts) |

Back to [README](../README.md)
