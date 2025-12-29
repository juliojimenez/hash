# Line Editing in Hash Shell

Hash shell includes a custom line editor with full cursor navigation and editing capabilities, similar to bash's readline.

## Key Bindings

### Cursor Movement

| Key | Action |
|-----|--------|
| **←** (Left Arrow) | Move cursor left one character |
| **→** (Right Arrow) | Move cursor right one character |
| **Ctrl+A** | Move to beginning of line |
| **Ctrl+E** | Move to end of line |
| **Home** | Move to beginning of line |
| **End** | Move to end of line |

### Editing

| Key | Action |
|-----|--------|
| **Backspace** | Delete character before cursor |
| **Delete** | Delete character at cursor |
| **Ctrl+H** | Same as Backspace |
| **Ctrl+U** | Delete from cursor to beginning of line |
| **Ctrl+K** | Delete from cursor to end of line |
| **Ctrl+W** | Delete word backward |

### Other

| Key | Action |
|-----|--------|
| **Tab** | Insert 4 spaces |
| **Ctrl+L** | Clear screen |
| **Ctrl+C** | Cancel current line (start fresh) |
| **Ctrl+D** | EOF (exit shell if line is empty) |
| **Enter** | Submit command |

## Features

### Insert Mode

Hash uses **insert mode** (like bash):

- Typing inserts characters at cursor position
- Existing text shifts right
- Can edit anywhere in the line

**Example:**
```
hello world
     ^
     cursor

Type 'beautiful ' → hello beautiful world
```

### Line Refresh

The line automatically redraws when:

- Characters are inserted/deleted
- Cursor moves
- Line is modified

This prevents the TAB+backspace prompt corruption issue!

### Safe Backspace

Backspace is **bounded**:
- Won't delete past the beginning of input
- Won't corrupt the prompt
- Properly handles multi-byte movements

### Ctrl+C Handling

Press Ctrl+C to cancel current input:
```
#> some long command that I don't want^C
#> 
# Fresh prompt!
```

### Ctrl+D Behavior

- **On empty line**: Exits the shell (like bash)
- **On non-empty line**: Does nothing

### Tab Handling

Tab inserts **4 spaces** instead of a tab character:
- Prevents alignment issues
- Consistent with modern editors
- Can be changed if needed

## Technical Details

### Raw Mode

Hash uses **raw terminal mode** for line editing:
- Disables canonical mode (line buffering)
- Disables echo (we handle display)
- Processes every key press immediately
- Restores terminal on exit

### ANSI Escape Sequences

Arrow keys send escape sequences:
- Left: `\x1b[D`
- Right: `\x1b[C`
- Up: `\x1b[A`
- Down: `\x1b[B`
- Home: `\x1b[H` or `\x1b[1~`
- End: `\x1b[F` or `\x1b[4~`

### Line Redrawing

After edits, the line is redrawn using:
```
\r         - Move to beginning
\x1b[K     - Clear to end of line
<prompt>   - Write prompt
<buffer>   - Write current line
\x1b[<n>C  - Move cursor to position
```

### Fallback Mode

If raw mode fails (non-TTY, pipe, etc.):
- Falls back to simple `getline()`
- No line editing
- Still works for scripts and pipes

## Differences from Bash

### Supported (Same as Bash)

- ✅ Arrow key navigation
- ✅ Ctrl+A, Ctrl+E
- ✅ Ctrl+U, Ctrl+K, Ctrl+W
- ✅ Ctrl+L (clear screen)
- ✅ Ctrl+C (cancel line)
- ✅ Ctrl+D (EOF)
- ✅ Backspace/Delete
- ✅ Home/End keys
- ✅ Insert mode

### Not Yet Supported

- ❌ Up/Down arrows (command history)
- ❌ Ctrl+R (reverse search)
- ❌ Tab completion
- ❌ Ctrl+T (transpose characters)
- ❌ Alt key bindings
- ❌ Vi mode
- ❌ Custom key bindings

### Different Behavior

**Tab:**
- Bash: Tab completion
- Hash: Inserts 4 spaces

**History:**
- Bash: Up/Down arrows navigate history
- Hash: Not yet implemented

## Tips & Tricks

### Quick Edits

```bash
# Typed wrong command
#> cat file.tx
# Press Ctrl+A, Ctrl+K, type correct command
#> cat file.txt
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
# Press Ctrl+U to clear, start fresh
#> 
```

### Cancel Without Running

```bash
#> rm -rf /important
# Wait, that's wrong!
# Press Ctrl+C
^C
#> 
# Phew!
```

## Troubleshooting

### Keys Not Working

**Check terminal type:**
```bash
echo $TERM
# Should be xterm, xterm-256color, screen, etc.
```

**If using tmux/screen:**
Most key bindings should work, but some terminals may send different sequences.

### Backspace Deletes Wrong Character

Different terminals send different codes for backspace:
- Most send `127` (DEL)
- Some send `8` (Ctrl+H)
- Hash handles both!

### Arrow Keys Print Escape Sequences

Your terminal isn't sending proper escape sequences. Try:
- Setting `TERM=xterm-256color`
- Using a modern terminal emulator

### Tab Inserts Literal Tab

Hash intentionally inserts spaces instead of tab. If you need a literal tab:
- Use Ctrl+V then Tab (future feature)
- Or type `\t` in quotes

## Performance

Line editing is **very fast**:
- Minimal redrawing
- Optimized for append (most common case)
- Only full refresh when necessary

## Integration

Works seamlessly with:

- ✅ Colored prompts
- ✅ Git integration in prompt
- ✅ Multi-line commands (;, &&, ||)
- ✅ Aliases
- ✅ Tilde expansion

Back to [README](../README.md)
