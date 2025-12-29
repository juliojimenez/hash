# Command History in Hash Shell

Hash shell maintains a persistent command history with full bash compatibility, including support for HISTSIZE, HISTFILESIZE, and HISTCONTROL.

## Environment Variables

### HISTSIZE

Maximum number of commands to keep in memory:

```bash
# In .hashrc
export HISTSIZE=1000        # Keep 1000 commands (default)
export HISTSIZE=5000        # Keep 5000 commands
export HISTSIZE=-1          # Unlimited (grows as needed)
```

### HISTFILESIZE

Maximum number of commands to keep in history file:

```bash
# In .hashrc
export HISTFILESIZE=2000    # Save 2000 commands (default)
export HISTFILESIZE=10000   # Save 10000 commands
export HISTFILESIZE=-1      # Unlimited file size
```

**Typical settings:**
```bash
# Conservative (default)
export HISTSIZE=1000
export HISTFILESIZE=2000

# Power user
export HISTSIZE=10000
export HISTFILESIZE=50000

# Unlimited everything
export HISTSIZE=-1
export HISTFILESIZE=-1
```

### HISTFILE

Location of history file:

```bash
# Default
~/.hash_history

# Custom location
export HISTFILE=~/my_custom_history

# Per-project history
export HISTFILE=~/projects/myapp/.history
```

### HISTCONTROL

Control which commands are saved:

```bash
export HISTCONTROL=ignorespace  # Skip space-prefixed
export HISTCONTROL=ignoredups   # Skip consecutive dups
export HISTCONTROL=ignoreboth   # Both (recommended)
export HISTCONTROL=erasedups    # Remove all previous dups
```

See [HISTCONTROL.md](HISTCONTROL.md) for complete details.

## Basic Usage

### Up/Down Arrows

Navigate through command history:

```bash
#> echo first
first
#> echo second
second
#> echo third
third

# Press Up arrow
#> echo third  (most recent)

# Press Up arrow again
#> echo second

# Press Down arrow
#> echo third

# Press Down at end → blank line for new command
```

### History Command

List all commands with line numbers:

```bash
#> history
    0  ls -la
    1  cd /tmp
    2  echo hello
    3  git status
```

## History Expansion

### !! - Last Command

Repeat the last command:

```bash
#> echo hello
hello
#> !!
echo hello
hello

#> sudo apt update
...
#> sudo !!
sudo sudo apt update
```

### !n - Command by Number

Run command number n:

```bash
#> history
    0  ls -la
    1  cd /tmp
    2  echo test

#> !1
cd /tmp
# Executes: cd /tmp
```

### !-n - Relative History

Run command n commands ago:

```bash
#> echo first
#> echo second  
#> echo third
#> !-2
echo second
second
```

### !prefix - Last Command Starting With Prefix

```bash
#> git status
#> ls -la
#> pwd
#> !git
git status
# Runs the last command starting with "git"
```

## History File

### Location

Default: `~/.hash_history`

Customize with:
```bash
export HISTFILE=~/my_history
```

### Persistence

- History is loaded when hash starts
- **History is saved immediately** after each command execution
- No data loss if shell crashes or is killed
- Respects HISTFILESIZE limit

### Incremental Saving

Unlike some shells that only save on exit, hash saves after every command:

```bash
#> echo test1
# Immediately saved to ~/.hash_history

#> echo test2
# Also immediately saved

# Even if shell crashes, both commands are preserved!
```

### Manual Save/Load

```bash
# Force save (already done automatically)
#> history -w

# Reload from file
#> history -r

# Clear history (memory only, file unchanged)
#> history -c
```

## History Behavior

### What Gets Saved

✅ Successfully executed commands  
✅ Commands with errors  
✅ Multi-line commands (chained with &&, ||, ;)  

### What Doesn't Get Saved

❌ Empty lines  
❌ Lines with only whitespace  
❌ Duplicate of previous command  
❌ Commands starting with space (privacy feature)  

### Privacy Feature

Commands starting with space are NOT saved:

```bash
#> echo public
# Saved to history

#>  echo private
# NOT saved (note the leading space)

#> history
    0  echo public
# "echo private" is missing
```

## Advanced Features

### Combining Expansions

History expansion works with other features:

```bash
#> export DIR=/tmp
#> cd $DIR
#> !!
cd /tmp
```

### History in Aliases

```bash
#> alias redo='!!'
#> echo test
test
#> redo
echo test
test
```

### Escaped Exclamation

```bash
#> echo hello\\!
hello!

#> echo price is \\$5\\!
price is $5!
```

## History Commands

### List History

```bash
#> history
```

### Clear History

```bash
#> history -c
```

### Save History

```bash
#> history -w
```

### Reload History

```bash
#> history -r
```

## Configuration

### History Size

Default: 1000 commands (defined in `HISTORY_MAX_SIZE`)

To change, modify `src/history.h` and recompile.

### Disable History

Not yet supported. History is always enabled.

## Examples

### Repeat Last Command

```bash
#> make
...build output...
#> !!
make
```

### Repeat with Sudo

```bash
#> apt update
Permission denied
#> sudo !!
sudo apt update
# Runs with sudo!
```

### Find and Rerun

```bash
#> history
    45  git status
    46  ls -la
    47  git commit -m "fix"
    48  git push

#> !47
git commit -m "fix"
```

### Prefix Search

```bash
#> git status
#> git add .
#> ls -la
#> !git
git add .
# Runs most recent git command
```

### Relative Reference

```bash
#> echo one
#> echo two
#> echo three
#> !-2
echo two
two
```

## Tips & Tricks

### Quick Redo

```bash
#> complex command with many args
#> !!
# Runs it again
```

### Fix and Retry

```bash
#> git commit -m "typo in mesage"
#> !!
# Press Up arrow, fix typo, press Enter
```

### Private Commands

```bash
#>  export API_KEY=secret
# Starts with space - won't be in history!
```

### History Audit

```bash
#> history | grep git
# See all git commands you ran
```

### Clean Up History

```bash
#> history -c
# Start fresh
```

## Key Bindings

| Key | Action |
|-----|--------|
| **↑** | Previous command (older) |
| **↓** | Next command (newer) |
| **Ctrl+R** | Reverse search (not yet implemented) |

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| `!!` | ✅ | ✅ |
| `!n` | ✅ | ✅ |
| `!-n` | ✅ | ✅ |
| `!prefix` | ✅ | ✅ |
| Up/Down arrows | ✅ | ✅ |
| History file | ✅ (~/.hash_history) | ✅ (~/.bash_history) |
| Privacy (space prefix) | ✅ | ✅ |
| `Ctrl+R` | ❌ | ✅ |
| `!?pattern` | ❌ | ✅ |
| `^old^new` | ❌ | ✅ |

## Troubleshooting

### History Not Saving

Check file permissions:
```bash
ls -la ~/.hash_history
```

### History Not Loading

Verify file exists:
```bash
cat ~/.hash_history
```

### !! Not Working

Make sure you've run a command first:
```bash
#> echo test
#> !!
# Works
```

Empty history:
```bash
#> !!
# No previous command
```

### Up Arrow Not Working

Check terminal:
```bash
echo $TERM
# Should be xterm-256color or similar
```

Back to [README](../README.md)
