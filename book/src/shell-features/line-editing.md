# Line Editing

Hash shell includes a custom line editor with full cursor navigation and editing capabilities.

## Key Bindings

### Cursor Movement

| Key | Action |
|-----|--------|
| `Left Arrow` | Move cursor left |
| `Right Arrow` | Move cursor right |
| `Up Arrow` | Previous command in history |
| `Down Arrow` | Next command in history |
| `Ctrl+A` | Move to beginning of line |
| `Ctrl+E` | Move to end of line |
| `Home` | Move to beginning of line |
| `End` | Move to end of line |

### Editing

| Key | Action |
|-----|--------|
| `Backspace` | Delete character before cursor |
| `Delete` | Delete character at cursor |
| `Ctrl+H` | Same as Backspace |
| `Ctrl+U` | Delete from cursor to beginning |
| `Ctrl+K` | Delete from cursor to end |
| `Ctrl+W` | Delete word backward |

### Other

| Key | Action |
|-----|--------|
| `Tab` | Auto-complete |
| `Ctrl+L` | Clear screen |
| `Ctrl+C` | Cancel current line |
| `Ctrl+D` | EOF (exit if line empty) |
| `Enter` | Submit command |

## Features

### Insert Mode

Hash uses insert mode (like bash):

- Typing inserts characters at cursor position
- Existing text shifts right
- Edit anywhere in the line

```
hello world
     ^
     cursor

Type 'beautiful ' → hello beautiful world
```

### Tab Completion

Press TAB to auto-complete:

**Single match:**
```bash
#> ec<TAB>
#> echo   # Completed with space
```

**Multiple matches - first TAB:**
```bash
#> cat te<TAB>
#> cat test  # Common prefix
```

**Multiple matches - second TAB:**
```bash
test.txt  test.md  testing.log
#> cat test
```

See [Tab Completion](tab-completion.md) for details.

### Ctrl+C Handling

Cancel current input and get a fresh prompt:

```bash
#> some long command^C
#>
```

### Ctrl+D Behavior

- On empty line: Exits the shell
- On non-empty line: Does nothing

## Tips & Tricks

### Quick Edits

```bash
# Typed wrong command
#> cat file.tx
# Press Ctrl+A, Ctrl+K, type correct command
```

### Delete Mistakes

```bash
#> echo hello worldddd
# Press Ctrl+W to delete "worldddd"
#> echo hello
```

### Clear and Restart

```bash
#> some complex command
# Press Ctrl+U to clear
#>
```

### Cancel Without Running

```bash
#> rm -rf /important
# Press Ctrl+C
^C
#>
```

## Troubleshooting

### Keys Not Working

Check terminal type:
```bash
echo $TERM
# Should be xterm, xterm-256color, etc.
```

### Arrow Keys Print Escape Sequences

Set proper terminal:
```bash
export TERM=xterm-256color
```

## Comparison with Bash

### Supported (Same as Bash)

- Arrow key navigation
- History navigation
- Ctrl+A, Ctrl+E
- Ctrl+U, Ctrl+K, Ctrl+W
- Ctrl+L, Ctrl+C, Ctrl+D
- Home/End keys
- Insert mode

### Not Yet Supported

- Ctrl+R (reverse search)
- Ctrl+T (transpose)
- Alt key bindings
- Vi mode
- Custom key bindings
