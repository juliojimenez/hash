# Quick Reference

A consolidated reference for hash shell features.

## Command Line Options

| Option | Description |
|--------|-------------|
| `-c string` | Execute commands from string |
| `-i` | Force interactive mode |
| `-l`, `--login` | Run as login shell |
| `-s` | Read from stdin |
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

### Other

| Key | Action |
|-----|--------|
| `Tab` | Auto-complete |
| `Ctrl+L` | Clear screen |
| `Ctrl+C` | Cancel line |
| `Ctrl+D` | EOF / Exit |

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

| Sequence | Description |
|----------|-------------|
| `\w` | Full path |
| `\W` | Current directory |
| `\u` | Username |
| `\h` | Hostname |
| `\g` | Git branch + status |
| `\e` | Exit code color |
| `\$` | `$` or `#` |
| `\n` | Newline |

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

hash's own `set name=value` options (`colors`, `welcome`, `PS1`, `color.<element>`,
`syntax_highlight`, `autosuggest`, `danger_highlight`) are listed on the
[configuration page](../getting-started/hashrc.md).

## Built-in Commands

| Command | Description |
|---------|-------------|
| `alias` | Define/list aliases |
| `unalias` | Remove alias |
| `cd` | Change directory |
| `pwd` | Print working directory |
| `echo` | Print arguments |
| `export` | Set environment variable |
| `set` | Shell options and positional parameters |
| `unset` | Remove a variable or function |
| `readonly` | Make a variable read-only |
| `local` | Variables local to a function |
| `read` | Read a line into variables |
| `getopts` | Parse options in a script |
| `shift` | Drop positional parameters |
| `eval` | Run arguments as a command |
| `exec` | Replace the shell, or redirect it |
| `return` | Return from a function or sourced file |
| `break`, `continue` | Leave or continue a loop |
| `trap` | Run commands on signals and at exit |
| `type`, `command` | Describe or run a command |
| `wait` | Wait for background jobs |
| `source` | Execute file in current shell |
| `exit` | Exit shell |
| `logout` | Exit login shell |
| `history` | Show command history |
| `jobs` | List background jobs |
| `fg` | Bring job to foreground |
| `bg` | Continue job in background |
| `kill` | Send signal to process |
| `test` | Evaluate expression |
| `[` | Same as test |
| `true` | Return success |
| `false` | Return failure |

## Configuration Files

| File | When Loaded |
|------|-------------|
| `/etc/profile` | Login shell |
| `~/.hash_profile` | Login shell |
| `~/.profile` | Login shell (fallback) |
| `~/.hashrc` | Interactive shell |
| `~/.hash_logout` | Login shell exit |
