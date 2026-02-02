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
| `HISTCONTROL` | History behavior |
| `PS1` | Prompt format |
| `PATH` | Command search path |
| `HOME` | Home directory |
| `EDITOR` | Default editor |

## Built-in Commands

| Command | Description |
|---------|-------------|
| `alias` | Define/list aliases |
| `unalias` | Remove alias |
| `cd` | Change directory |
| `pwd` | Print working directory |
| `echo` | Print arguments |
| `export` | Set environment variable |
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
