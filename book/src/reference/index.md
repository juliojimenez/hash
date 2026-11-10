# Quick Reference

A consolidated reference for hash shell features.

## Command Line Options

| Option | Description |
|--------|-------------|
| `-c string` | Execute commands from string |
| `-i` | Force interactive mode |
| `-l`, `--login` | Run as login shell |
| `-s` | Read from stdin |
| `-a -b -C -e -f -m -n -u -x` | Shell options, as for `set` |
| `-v`, `--version` | Print version |
| `-h`, `--help` | Show help |

## Key Bindings

### Navigation

| Key | Action |
|-----|--------|
| `Left/Right` | Move cursor |
| `Up/Down` | History navigation |
| `Ctrl+A` | Beginning of line |
| `Ctrl+E` | End of line |
| `Home/End` | Beginning/End of line |

### Editing

| Key | Action |
|-----|--------|
| `Backspace` | Delete before cursor |
| `Delete` | Delete at cursor |
| `Ctrl+U` | Delete to beginning |
| `Ctrl+K` | Delete to end |
| `Ctrl+W` | Delete word |
| `Ctrl+H` | Same as Backspace |

### Other

| Key | Action |
|-----|--------|
| `Tab` | Auto-complete |
| `Ctrl+L` | Clear screen |
| `Ctrl+C` | Cancel line |
| `Ctrl+D` | EOF / Exit |
| `Enter`, `Ctrl+J` | Run the line |

`set -o vi` switches to [vi mode](../shell-features/line-editing.md#vi-mode); `set -o emacs` is the default.

## Operators

| Operator | Description |
|----------|-------------|
| `\|` | Pipe stdout to stdin |
| `>` | Output to file (overwrite) |
| `>>` | Output to file (append) |
| `<` | Input from file |
| `2>` | Stderr to file |
| `&>` | Both stdout and stderr |
| `2>&1` | Stderr to stdout |
| `&&` | Run if previous succeeded |
| `\|\|` | Run if previous failed |
| `;` | Run sequentially |
| `&` | Run in background |

## Special Variables

| Variable | Description |
|----------|-------------|
| `$?` | Exit code of last command |
| `$$` | Shell process ID |
| `$0` | Shell/script name |
| `$1`-`$9` | Positional arguments |
| `$#` | Number of arguments |
| `$@` | All arguments (separate) |
| `$*` | All arguments (single) |
| `$-` | Option letters in effect |
| `$LINENO` | Current line number |

## Test Operators

### File Tests

| Test | Description |
|------|-------------|
| `-e` | Exists |
| `-f` | Regular file |
| `-d` | Directory |
| `-r` | Readable |
| `-w` | Writable |
| `-x` | Executable |
| `-s` | Size > 0 |
| `-L` | Symbolic link |

### String Tests

| Test | Description |
|------|-------------|
| `-z` | Empty string |
| `-n` | Non-empty string |
| `=` | Equal |
| `!=` | Not equal |

### Numeric Tests

| Test | Description |
|------|-------------|
| `-eq` | Equal |
| `-ne` | Not equal |
| `-lt` | Less than |
| `-le` | Less or equal |
| `-gt` | Greater than |
| `-ge` | Greater or equal |

## PS1 Escape Sequences

`set prompt=NAME` picks a preset: `classic`, `plain`, `minimal`, `two-line`, `powerline`; `set prompt=list` shows them. `PS1` builds your own ([Prompt Customization](../shell-features/prompt.md)):

| Sequence | Description |
|----------|-------------|
| `\w` | Full path |
| `\W` | Current directory |
| `\u` | Username |
| `\h` | Hostname, up to the first dot |
| `\H` | Full host name |
| `\g` | Git branch + status |
| `\b` | Git branch alone, nothing outside a repository |
| `\e` | Exit code color, between `\e...\e` |
| `\?` | Last exit status when it was not 0 |
| `\$` | `$` or `#` for root |
| `\t` `\T` `\@` | Time: 24-hour, 12-hour, 12-hour with am/pm |
| `\d` | Date |
| `\j` | Number of background jobs |
| `\!` | History number of the next command |
| `\s` `\v` | Shell name and version |
| `\n` | Newline |
| `\\` | A backslash |
| `\[` `\]` | Ignored (bash's colour-code brackets) |

## History Expansion

| Pattern | Description |
|---------|-------------|
| `!!` | Last command |
| `!n` | Command number n |
| `!-n` | n commands ago |
| `!prefix` | Last starting with prefix |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `HISTSIZE` | Commands in memory |
| `HISTFILESIZE` | Commands in file |
| `HISTFILE` | History file location |
| `FUNCNEST` | Function nesting limit, `eval` and `.` included (default 1000; `0` keeps only the stack guard) |
| `HISTCONTROL` | History behavior |
| `PS1` | Prompt format |
| `PS2` | Continuation prompt |
| `PS4` | Prefix of `set -x` lines |
| `PATH` | Command search path |
| `HOME` | Home directory |
| `EDITOR` | Default editor |

## Shell Options

Set with `set -x`, clear with `set +x`; cluster letters (`set -ex`) or use the
name (`set -o xtrace`). `set -o` alone lists them, `set +o` prints them as
commands, and `$-` holds the letters in effect.

| Letter | Name | Effect |
|--------|------|--------|
| `-a` | `allexport` | Every assignment is exported |
| `-b` | `notify` | Accepted; background job status is reported at the prompt |
| `-C` | `noclobber` | `>` does not overwrite an existing file (`>|` does) |
| `-e` | `errexit` | Exit when a command fails |
| `-f` | `noglob` | No pathname expansion |
| `-h` | | Accepted; commands are remembered as they run |
| `-m` | `monitor` | Job control |
| `-n` | `noexec` | Read and parse commands without running them (not at the prompt) |
| `-u` | `nounset` | Expanding an unset variable is an error |
| `-v` | `verbose` | Print input lines as they are read |
| `-x` | `xtrace` | Print each command, after expansion, before it runs (`PS4` is the prefix) |
| | `nolog` | Keep commands out of the history |
| | `nonlexicalctrl` | `break` and `continue` reach loops in calling functions |
| | `emacs` | Emacs-style line editing (the default) |
| | `vi` | Vi-style modal line editing; `set -o vi` turns `emacs` off and back |

hash's own `set name=value` options (`prompt`, `colors`, `welcome`, `PS1`, `color.<element>`,
`syntax_highlight`, `autosuggest`, `danger_highlight`) are listed on the
[configuration page](../getting-started/hashrc.md).

## Built-in Commands

The [Built-in Commands](builtins.md) page has the full entry for each, the text `help NAME` prints; this table is the one-line form. Every builtin takes `--` as the end of its options.

| Command | Synopsis and meaning |
|---------|----------------------|
| `:` | `: [arg...]`: do nothing, status 0 |
| `.`, `source` | `. file`, `source file`: run a file in the current shell |
| `[`, `test` | `[ expr ]`, `test expr`: evaluate an expression ([Test Command](../scripting/test-command.md)) |
| `[[` | `[[ expr ]]`: extended test, with `==`/`!=` patterns, `=~` regular expressions, `&&` and `\|\|` inside |
| `alias` | `alias [name[=value]...]`: define or print aliases |
| `bg` | `bg [%job]`: continue a stopped job in the background |
| `break` | `break [n]`: leave n enclosing loops |
| `cd` | `cd [-L\|-P] [dir]`: change directory; `-L` (default) keeps the logical path, `-P` resolves symlinks, `cd -` goes back |
| `command` | `command name [arg...]`, `command -v \| -V name...`: run a command without function lookup; `-v`/`-V` show how a name resolves |
| `complete` | `complete [-abcdfuv] [-o default\|dirnames] [-W words] [-F function] name...`, `complete -p [name...]`, `complete -r [name...]`: programmable completion |
| `continue` | `continue [n]`: next iteration of n enclosing loops |
| `echo` | `echo [-neE] [arg...]`: print the arguments |
| `eval` | `eval [arg...]`: run the arguments as a command |
| `exec` | `exec [command [arg...]]`, `exec redirection...`: replace the shell, or redirect it for good |
| `exit` | `exit [n]`: leave the shell |
| `export` | `export [-p] [name[=value]...]`: put variables in the environment; `-p` prints them |
| `false` | `false`: status 1 |
| `fg` | `fg [%job]`: bring a job to the foreground |
| `getopts` | `getopts optstring name [arg...]`: parse options in a script |
| `hash` | `hash [-r] [name...]`: remember where commands live; `-r` forgets |
| `help` | `help [-s] [pattern...]`: synopsis and description of builtins and keywords; `-s` synopsis only |
| `history` | `history [-c \| -w \| -r]`: show the history; clear it, write or read the file |
| `jobs` | `jobs [-l \| -p]`: list background jobs, with pids, or pids only |
| `kill` | `kill [-s signal \| -signal] pid \| %job ...`, `kill -l`: send a signal; `-l` lists the names |
| `local` | `local [name[=value]...]`: variables local to a function |
| `logout` | `logout`: leave a login shell |
| `pwd` | `pwd [-L\|-P]`: print the working directory, logical or physical |
| `read` | `read [-r] name...`: read a line into variables, split on IFS; `-r` keeps backslashes |
| `readonly` | `readonly [-p] [name[=value]...]`: make variables read-only; `-p` prints them |
| `return` | `return [n]`: return from a function or sourced file |
| `set` | `set [-abCefhmnuvx] [-o option] [+...] [--] [arg...]`, `set name=value`: shell options and positional parameters, and hash's own settings ([Shell Options](#shell-options)) |
| `shift` | `shift [n]`: drop positional parameters |
| `times` | `times`: user and system times of the shell and its children |
| `trap` | `trap [action] [signal...]`, `trap -l \| -p`: run commands on signals and at EXIT; `-l` lists the signals, `-p` prints the traps |
| `true` | `true`: status 0 |
| `type` | `type [-afptP] name...`: how a name would be interpreted; `-t` one word, `-p`/`-P` the file, `-a` every match, `-f` no functions |
| `ulimit` | `ulimit [-HS] [-a \| -cdfnstuv] [limit]`: show or set resource limits |
| `umask` | `umask [-S] [mode]`: show or set the file creation mask; `-S` symbolic |
| `unalias` | `unalias [-a] name...`: remove aliases; `-a` all of them |
| `unset` | `unset [-f \| -v] name...`: remove variables (`-v`) or functions (`-f`) |
| `update` | `update`: install a newer release of hash when there is one |
| `wait` | `wait [pid \| %job ...]`: wait for background jobs |

## Configuration Files

| File | When Loaded |
|------|-------------|
| `/etc/profile` | Login shell |
| `~/.hash_profile` | Login shell |
| `~/.profile` | Login shell (fallback) |
| `~/.hashrc` | Interactive shell |
| `~/.hash_logout` | Login shell exit |
