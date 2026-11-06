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

## Vi Mode

`set -o vi` switches the editor to vi's modal editing; `set -o emacs` (the
default) switches back. Both appear in `set -o`, and `set +o` prints the one in
effect as a command you can put in `~/.hashrc`.

Each line starts in insert mode, where every key works as in emacs mode. `Esc`
enters command mode: the cursor stands on a character, motions move it,
operators act on a motion, and `i`, `a`, `I`, `A` return to inserting. Words are
blank-separated. Counts are not implemented.

| Key | Action |
|-----|--------|
| `Esc` | Command mode |
| `h` `l` (or arrows) | Left, right |
| `0` `^` `$` | Start, start, end of line |
| `w` `b` `e` | Next word, previous word, end of word |
| `i` `a` `I` `A` | Insert here, after the cursor, at the start, at the end |
| `x` `X` | Delete the character under, before the cursor |
| `d` + motion, `dd`, `D` | Delete through a motion, the whole line, to the end |
| `c` + motion, `cc`, `C`, `s`, `S` | Delete the same, then insert (`cw` changes to the end of the word) |
| `y` + motion, `yy` | Copy to the cut buffer |
| `p` `P` | Put the cut buffer after, before the cursor |
| `r` + char | Replace the character under the cursor |
| `~` | Toggle case and move right |
| `u` | Undo the last edit (again redoes) |
| `k` `j` (or arrows) | Older, newer history entry, cursor at the start |
| `/` | Incremental history search: type, `Enter` runs the match, `Esc` keeps it for editing |
| `Enter` | Run the line, from either mode |

```bash
#> set -o vi
#> echo helo          # Esc, h, i, l, Esc, Enter
hello
```

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
- Ctrl+R reverse search
- Vi mode (`set -o vi`), the subset listed above

### Not Yet Supported

- Ctrl+T (transpose)
- Alt key bindings
- Vi counts and text objects
- Custom key bindings
