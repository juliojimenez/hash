# Command History

Hash shell maintains persistent command history with bash compatibility.

## Basic Usage

### Up/Down Arrows

Navigate through history:

```bash
#> echo first
#> echo second
#> echo third

# Press Up arrow
#> echo third  # Most recent

# Press Up again
#> echo second
```

### History Command

List all commands:

```bash
#> history
    0  ls -la
    1  cd /tmp
    2  echo hello
    3  git status
```

## History Expansion

### !! - Last Command

```bash
#> echo hello
hello
#> !!
echo hello
hello

#> apt update
Permission denied
#> sudo !!
sudo apt update
```

### !n - Command by Number

```bash
#> history
    0  ls -la
    1  cd /tmp
    2  echo test

#> !1
cd /tmp
```

### !-n - Relative History

```bash
#> echo first
#> echo second
#> echo third
#> !-2
echo second
second
```

### !prefix - Last Command Starting With

```bash
#> git status
#> ls -la
#> pwd
#> !git
git status
```

## Environment Variables

### HISTSIZE

Commands kept in memory:

```bash
export HISTSIZE=1000        # Default
export HISTSIZE=5000        # More
export HISTSIZE=-1          # Unlimited
```

### HISTFILESIZE

Commands saved to file:

```bash
export HISTFILESIZE=2000    # Default
export HISTFILESIZE=-1      # Unlimited
```

### HISTFILE

History file location:

```bash
# Default
~/.hash_history

# Custom
export HISTFILE=~/my_history
```

### HISTCONTROL

Control what gets saved:

```bash
export HISTCONTROL=ignorespace  # Skip space-prefixed
export HISTCONTROL=ignoredups   # Skip consecutive dups
export HISTCONTROL=ignoreboth   # Both (recommended)
export HISTCONTROL=erasedups    # Remove all dups
```

## Privacy Feature

Commands starting with space are NOT saved:

```bash
#> echo public
# Saved to history

#>  echo private
# NOT saved (leading space)
```

## What Gets Saved

- Successfully executed commands
- Commands with errors
- Multi-line commands (&&, ||, ;)

## What Doesn't Get Saved

- Empty lines
- Whitespace-only lines
- Duplicate of previous command
- Commands starting with space

## History File

### Location

Default: `~/.hash_history`

### Incremental Saving

History saves immediately after each command:

```bash
#> echo test1
# Immediately saved

#> echo test2
# Also saved

# Even if shell crashes, both are preserved!
```

### Manual Commands

```bash
history -w    # Force save
history -r    # Reload from file
history -c    # Clear history (memory only)
```

## Examples

### Repeat Last Command

```bash
#> make
#> !!
make
```

### Repeat with Sudo

```bash
#> apt update
Permission denied
#> sudo !!
sudo apt update
```

### History Search

```bash
#> history | grep git
# See all git commands
```

### Private Commands

```bash
#>  export API_KEY=secret
# Leading space - not saved!
```

## Troubleshooting

### History Not Saving

Check file permissions:
```bash
ls -la ~/.hash_history
```

### !! Not Working

Make sure you've run a command first.

### Up Arrow Not Working

Check terminal:
```bash
echo $TERM
```

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| `!!` | Yes | Yes |
| `!n` | Yes | Yes |
| `!-n` | Yes | Yes |
| `!prefix` | Yes | Yes |
| Up/Down arrows | Yes | Yes |
| History file | Yes | Yes |
| Privacy (space prefix) | Yes | Yes |
| `Ctrl+R` | No | Yes |
| `!?pattern` | No | Yes |
| `^old^new` | No | Yes |
