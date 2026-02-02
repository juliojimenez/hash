# Prompt Customization

Hash shell supports powerful prompt customization through the `PS1` variable.

## Default Prompt

```
/home/user/projects git:(main) #>
```

- **Blue** current path
- **Green** `git:` (yellow if dirty)
- **Cyan** branch name
- **Blue** `#>` (red if last command failed)

## Quick Start

In `~/.hashrc`:

```bash
# Simple prompt
set PS1='\W #> '

# Full path with git
set PS1='\w\g \e#>\e '

# User@host:path
set PS1='\u@\h:\w\$ '
```

Or as environment variable:

```bash
export PS1='\w\g \e#>\e '
```

## Escape Sequences

### Path Information

| Sequence | Description | Example |
|----------|-------------|---------|
| `\w` | Full current path | `/home/user/projects` |
| `\W` | Current directory only | `projects` |

### User Information

| Sequence | Description |
|----------|-------------|
| `\u` | Username |
| `\h` | Hostname |
| `\$` | `$` for user, `#` for root |

### Git Integration

| Sequence | Description |
|----------|-------------|
| `\g` | Git branch with status |

The `\g` sequence:
- Shows nothing outside git repos
- Shows ` git:(branch)` in repos
- `git:` is **green** if clean, **yellow** if dirty
- Branch name is **cyan**

### Exit Code Indicator

| Sequence | Description |
|----------|-------------|
| `\e` | Sets color based on exit code |

Use `\e` before and after text:
```bash
set PS1='\w \e#>\e '
#           ^  ^
#           |  Reset color
#           Set color (blue=success, red=fail)
```

### Special Characters

| Sequence | Description |
|----------|-------------|
| `\n` | Newline |
| `\\` | Literal backslash |

## Example Prompts

### Minimal

```bash
set PS1='\W #> '
# Output: projects #>
```

### Classic

```bash
set PS1='\u@\h:\w \$ '
# Output: user@hostname:/home/user $
```

### Full Path with Git

```bash
set PS1='\w\g \e#>\e '
# Output: /home/user/projects git:(main) #>
```

### Two-Line

```bash
set PS1='\w\g\n\e#>\e '
# Output:
# /home/user/projects git:(main)
# #>
```

### Powerline Style

```bash
set PS1='\u@\h \w\g\n\e\e '
```

## Color Behavior

### Path Colors

`\w` and `\W` are **bold blue**

### Git Colors

- `git:` is **green** when clean
- `git:` is **yellow** when dirty
- Branch name is **cyan**

### Exit Code Colors

- `\e` sets **blue** on success (exit 0)
- `\e` sets **red** on failure (exit non-zero)

## Tips

### Test Before Setting

```bash
#> export PS1='\W\g \e#>\e '
projects git:(main) #>
```

### Keep It Simple

Complex prompts can slow down the shell:

```bash
# Fast
set PS1='\W #> '

# Slower (git checks)
set PS1='\w\g \e#>\e '
```

### Two-Line for Long Paths

```bash
set PS1='\w\g\n\e#>\e '
```

### Disable Git in Large Repos

```bash
set PS1='\w \e#>\e '  # No \g
```

## Troubleshooting

### Colors Not Showing

Enable colors:
```bash
set colors=on
```

Check terminal:
```bash
echo $TERM
# Should be xterm-256color or similar
```

### Git Branch Not Showing

Verify you're in a git repo:
```bash
git status
```

Ensure `\g` is in your PS1.

### Exit Colors Not Working

Wrap characters between two `\e`:

```bash
# Wrong
set PS1='\w \e#> '

# Correct
set PS1='\w \e#>\e '
```

## Comparison with Bash

### Similar

- `\w`, `\W`, `\u`, `\h`, `\$` work the same
- Color support
- Environment variable support

### Hash-Specific

- `\g` - Git integration
- `\e` - Exit code color
- Automatic `~` substitution

### Not Supported (Yet)

- `\d` - Date
- `\t` - Time
- `\!` - History number
- `\[` and `\]` - Non-printing sequences
