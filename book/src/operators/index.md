# Operators & Redirection

Hash shell supports Unix operators for connecting and controlling commands.

## Overview

| Operator | Type | Description |
|----------|------|-------------|
| `\|` | Pipe | Connect stdout to stdin |
| `>` | Redirect | Output to file (overwrite) |
| `>>` | Redirect | Output to file (append) |
| `<` | Redirect | Input from file |
| `2>` | Redirect | Stderr to file |
| `&>` | Redirect | Both stdout and stderr |
| `&&` | Chain | Run if previous succeeded |
| `\|\|` | Chain | Run if previous failed |
| `;` | Chain | Run sequentially |
| `&` | Background | Run in background |

## Quick Examples

### Pipes

```bash
ls | grep txt
cat file.txt | sort | uniq
ps aux | grep nginx | wc -l
```

### Redirection

```bash
echo "Hello" > file.txt       # Overwrite
echo "World" >> file.txt      # Append
cat < input.txt               # Input from file
command 2> errors.log         # Stderr
command &> all.log            # Both
```

### Command Chaining

```bash
make && make install          # Run if success
cd /nonexistent || echo "Failed"  # Run if fail
echo "Start"; command; echo "End" # Sequential
```

### Background

```bash
long_command &                # Run in background
```

## Learn More

- [Pipes](pipes.md) - Connect command output to input
- [I/O Redirection](redirection.md) - Control file input/output
- [Command Chaining](command-chaining.md) - Sequential and conditional execution
- [Command Substitution](command-substitution.md) - Use command output as arguments
- [Background Processes](background.md) - Run commands in background
