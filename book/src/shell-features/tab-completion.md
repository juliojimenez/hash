# Tab Completion

Hash completes commands, files and directories, and knows what many commands expect as arguments: variable names after `$`, directories after `cd`, aliases after `unalias`, git subcommands and branches, make targets, manual pages. The `complete` builtin adds completions for any command.

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
- Shell functions
- Executables in PATH, scanned once and rescanned when `PATH` changes or after 15 seconds

```bash
#> g<TAB><TAB>
ga    gc    gd    git    gl    gp    gs
```

### Subsequent Words

By default, files and directories: relative and absolute paths, with tilde
expansion. A name typed without a directory completes to a bare name (`afile`,
not `./afile`). Several commands get something better suited:

| Command | Completes |
|---------|-----------|
| `cd`, `pushd`, `rmdir` | Directories only |
| `unalias` | Alias names |
| `unset` | Variable names; function names after `-f` |
| `export`, `readonly`, `local` | Variable names |
| `command`, `exec`, `builtin`, `type`, `which`, `sudo`, `doas`, `nohup`, `nice`, `time`, `env`, `xargs`, `watch` | A command for the first operand, then files |
| `git` | Subcommands; branches, tags and remotes for `checkout`, `switch`, `merge`, `rebase`, `log`, `diff`, `push`, `pull` and friends; files for `add`, `rm`, `restore` |
| `make`, `gmake` | Targets of `GNUmakefile`, `makefile` or `Makefile` in the current directory (`-f` and `-C` complete files and directories) |
| `man` | Page names from `MANPATH` or `manpath` |

Options (words starting with `-`) are not completed.

### Variables

After `$` or `${`, shell variables and the environment:

```bash
#> echo $HO<TAB>
#> echo $HOME
#> echo ${HO<TAB>
#> echo ${HOME}
```

### Users

`~` followed by the start of a login name completes to `~name/`:

```bash
#> ls ~ro<TAB>
#> ls ~root/
```

```bash
#> cat /<TAB>
bin/  boot/  dev/  etc/  home/  ...

#> cd ~/.c<TAB>
#> cd ~/.config/
```

## Programmable Completion

`complete` records how a command's arguments are completed, in the spirit of
bash's builtin but without arrays:

```bash
complete -W "start stop restart status" myservice
complete -F _mytool_complete mytool
complete -d cdto                 # directories only
complete -c sudo                 # commands (sudo already has this built in)
complete -o default -W "init build" tool   # files when no word matches
```

| Option | Meaning |
|--------|---------|
| `-W wordlist` | The words in `wordlist` (separated by blanks) that start with the current word |
| `-F function` | Run `function`; every line it prints that starts with the current word is a candidate |
| `-a` `-b` `-c` `-d` `-f` `-u` `-v` | Aliases, builtins, commands, directories, files, users, variables |
| `-o default` | Files when nothing else matched |
| `-o dirnames` | Directories when nothing else matched |
| `-p [name...]` | Print the specifications, as lines that can be typed back |
| `-r [name...]` | Remove them (all of them without names) |

A `-F` function runs with `$1` set to the command, `$2` to the word being
completed and `$3` to the word before it, and with `COMP_LINE`, `COMP_POINT`
and `COMP_CWORD` set to the whole line, the cursor position and the index of
the word. It prints candidates, one per line; hash keeps those that start with
`$2`, so the function may print its whole vocabulary or filter it itself:

```bash
_mytool_complete() {
    case $3 in
        --format) echo json; echo yaml; echo table ;;
        *)        mytool --list-commands ;;
    esac
}
complete -F _mytool_complete mytool
```

Put `complete` lines in `~/.hashrc` to have them every session.

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

- Options (`--long`, `-x`) are not completed; `complete -W` can supply them for a command.
- `git` subcommand detection does not know which options take an argument (`git -C dir <TAB>` sees `dir` as the subcommand).
- Candidates are plain words: no quoting of names with spaces.

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| Command completion | Yes | Yes |
| File/directory completion | Yes | Yes |
| Alias completion | Yes | Yes |
| Path completion | Yes | Yes |
| Double-TAB shows matches | Yes | Yes |
| Variable completion | Yes | Yes |
| User completion (`~name`) | Yes | Yes |
| Directories after `cd` | Yes | With bash-completion |
| git, make, man | Yes | With bash-completion |
| Programmable completion | `complete -W`, `-F`, actions | Yes (with `compgen`, arrays) |
