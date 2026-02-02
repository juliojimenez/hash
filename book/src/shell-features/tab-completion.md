# Tab Completion

Hash shell includes intelligent tab completion for commands, files, and directories.

## Basic Usage

### Command Completion

Press TAB after typing part of a command:

```bash
#> ec<TAB>
#> echo     # Completed!

#> gi<TAB>
#> git      # Completed from PATH
```

### File/Directory Completion

```bash
#> cat test<TAB>
#> cat testfile.txt    # Completed!

#> cd Doc<TAB>
#> cd Documents/       # Directory with trailing /
```

### Multiple Matches

**First TAB** - Complete common prefix:
```bash
#> cat test<TAB>
#> cat test            # Common prefix
```

**Second TAB** - Show all matches:
```bash
#> cat test<TAB><TAB>
testfile.txt  test.md  test_data.csv
#> cat test
```

## What Gets Completed

### First Word (Command)

Completes from:
- Built-in commands (cd, exit, alias, etc.)
- Aliases from .hashrc
- Executables in PATH

```bash
#> g<TAB><TAB>
ga    gc    gd    git    gl    gp    gs
```

### Subsequent Words (Files/Directories)

Completes:
- Files and directories
- Paths with tilde expansion
- Relative and absolute paths

```bash
#> cat /<TAB>
bin/  boot/  dev/  etc/  home/  ...

#> cd ~/.c<TAB>
#> cd ~/.config/
```

## Features

### Directory Handling

Directories get trailing `/`:

```bash
#> cd Doc<TAB>
#> cd Documents/
```

### Tilde Expansion

```bash
#> cat ~/.<TAB>
~/.bashrc  ~/.config/  ~/.hashrc  ...
```

### Path Completion

```bash
#> ls ~/projects/hash/src/m<TAB>
#> ls ~/projects/hash/src/main.c
```

### Case Sensitivity

Completion is case-sensitive:

```bash
#> cat Test<TAB>   # Won't match "test.txt"
#> cat test<TAB>   # Matches "test.txt"
```

## Behavior

### Single Match

- Completes full name
- Adds space after (ready for next arg)
- Directories get `/`

### Multiple Matches

**First TAB:** Complete common prefix
**Second TAB:** Show all matches in columns

### No Matches

Bell/beep sound, line unchanged.

## After Operators

Completion works after command chaining:

```bash
#> ls && ca<TAB>
#> ls && cat

#> cd /tmp ; pw<TAB>
#> cd /tmp ; pwd
```

## Tips & Tricks

### Quick Directory Navigation

```bash
#> cd /u/l/b<TAB>
#> cd /usr/local/bin/
```

### Exploring Directories

```bash
#> ls /etc/<TAB><TAB>
# Shows all files in /etc/
```

### Command Discovery

```bash
#> git<TAB><TAB>
git  git-shell  gitk  ...
```

## Limitations

Current limitations (may be added later):

- No variable name completion (after `$`)
- No username completion (after `~`)
- No command-specific completion (git subcommands)
- No programmable completion

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| Command completion | Yes | Yes |
| File/directory completion | Yes | Yes |
| Alias completion | Yes | Yes |
| Path completion | Yes | Yes |
| Double-TAB shows matches | Yes | Yes |
| Variable completion | No | Yes |
| Programmable completion | No | Yes |
