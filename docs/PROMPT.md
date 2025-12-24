# Hash Shell Prompt Customization

Hash shell supports powerful prompt customization through the `PS1` variable, similar to bash. Create dynamic, informative prompts with colors, git integration, and exit code indicators.

## Default Prompt

The default hash prompt shows:

```
/home/user/projects git:(main) #> 
```

Features:
- **Blue** current path
- **Green** `git:` (changes to **yellow** if uncommitted changes)
- **Cyan** branch name
- **Blue** `#>` (changes to **red** if previous command failed)

## Quick Start

### In .hashrc

Add to your `~/.hashrc`:

```bash
# Simple prompt with just current directory
set PS1='\W #> '

# Full path with git
set PS1='\w\g \e#>\e '

# Username and hostname
set PS1='\u@\h:\w\g \$ '
```

### Environment Variable

Or set the PS1 environment variable:

```bash
export PS1='\w\g \e#>\e '
```

## Escape Sequences

### Path Information

| Sequence | Description | Example |
|----------|-------------|---------|
| `\w` | Full current working directory | `/home/user/projects` |
| `\W` | Current directory name only | `projects` |
| `~` | Home directory is replaced with `~` | `~/projects` |

### User Information

| Sequence | Description |
|----------|-------------|
| `\u` | Username |
| `\h` | Hostname |
| `\$` | `$` for regular user, `#` for root |

### Git Integration

| Sequence | Description |
|----------|-------------|
| `\g` | Git branch with status indicator |

The `\g` sequence:
- Shows nothing if not in a git repository
- Shows ` git:(branch)` when in a repo
- `git:` is **green** if repo is clean
- `git:` is **yellow** if there are uncommitted changes
- Branch name is **cyan**

### Exit Code Indicator

| Sequence | Description |
|----------|-------------|
| `\e` | Sets color based on previous command exit code |

Use `\e` before characters you want colored:
- `\e#>` - Bracket will be **blue** on success, **red** on failure

### Special Characters

| Sequence | Description |
|----------|-------------|
| `\n` | Newline |
| `\\` | Literal backslash |

## Example Prompts

### Minimal

```bash
# Just directory and prompt
set PS1='\W #> '
# Output: projects #>
```

### Classic

```bash
# User@host:path $
set PS1='\u@\h:\w \$ '
# Output: user@hostname:/home/user/projects $
```

### Full Path with Git

```bash
# Full path + git + colored bracket
set PS1='\w\g \e#>\e '
# Output: /home/user/projects git:(main) #>
```

### Compact with Git

```bash
# Just directory name + git
set PS1='\W\g \e#>\e '
# Output: projects git:(main) #>
```

### Two-Line Prompt

```bash
# Path and git on first line, prompt on second
set PS1='\w\g\n\e#>\e '
# Output:
# /home/user/projects git:(main)
# #>
```

### Fancy Prompt

```bash
# User@host in one color, path in another
set PS1='\u@\h:\w\g\n\e└─\$\e '
# Output:
# user@hostname:/home/user/projects git:(main)
# └─$
```

## Color Behavior

### Path Colors
- Full path (`\w`) and directory name (`\W`) are shown in **bold blue**

### Git Colors
- `git:` is **green** when repo is clean
- `git:` is **yellow** when there are uncommitted changes
- Branch name is always **cyan**

### Exit Code Colors
Use `\e` to mark portions of the prompt that should change color:
```bash
set PS1='\w \e#>\e '
        #     ^  ^
        #     |  Reset color
        #     Set color based on exit code
```

The `\e` sequence:
- First `\e` starts the color (blue for success, red for failure)
- Second `\e` resets to default color
- Wrap the characters you want colored between two `\e` sequences

## Git Integration Details

### Requirements
- Git must be installed
- Must be inside a git repository
- Hash checks git status automatically

### Performance
Git status checks are fast but:
- Run on every prompt generation
- May be slow for very large repositories
- Disable git integration if needed: `set PS1='\w \e#>\e '` (no `\g`)

### What Counts as "Dirty"
A repository is considered dirty (uncommitted changes) if:
- Modified files (tracked)
- Untracked files
- Staged changes not yet committed
- Deleted files

### Branch Detection
Shows the current branch name or:
- `HEAD` if in detached HEAD state
- Nothing if not in a git repository

## .hashrc Examples

### Example 1: Simple

```bash
# Just show current directory
set PS1='\W \$ '
```

### Example 2: Default (Recommended)

```bash
# Full path, git status, exit code indicator
set PS1='\w\g \e#>\e '
```

### Example 3: Compact

```bash
# Directory name only, with git
set PS1='\W\g \e#>\e '
```

### Example 4: Bash-like

```bash
# Similar to default bash prompt
set PS1='\u@\h:\w\$ '
```

### Example 5: Two-Line

```bash
# Path and git on top, clean prompt below
set PS1='\w\g\n\e#>\e '
```

### Example 6: Powerline-style

```bash
# Use unicode characters for fancy prompt
set PS1='\u@\h \w\g\n\e➜\e '
```

## Tips & Tricks

### 1. Test Before Setting

Test PS1 values interactively before adding to .hashrc:

```bash
#> export PS1='\W\g \e#>\e '
projects git:(main) #> 
```

### 2. Keep It Simple

Complex prompts can slow down the shell:
```bash
# Fast
set PS1='\W #> '

# Slower (git checks)
set PS1='\w\g \e#>\e '
```

### 3. Use Two-Line for Long Paths

Prevent command wrapping:
```bash
set PS1='\w\g\n\e#>\e '
```

### 4. Disable Git in Large Repos

If git checks are slow:
```bash
# No git integration
set PS1='\w \e#>\e '
```

### 5. Match Your Style

Choose based on preference:
- **Minimalist**: `\W #> `
- **Informative**: `\u@\h:\w\g \$ `
- **Power user**: `\w\g\n\e#>\e `

## Troubleshooting

### Colors Not Showing

1. Check if colors are enabled:
```bash
set colors=on
```

2. Verify terminal supports colors:
```bash
echo $TERM
# Should show something like "xterm-256color"
```

### Git Branch Not Showing

1. Verify you're in a git repository:
```bash
git status
```

2. Check git is installed:
```bash
which git
```

3. Ensure `\g` is in your PS1:
```bash
set PS1='\w\g \e#>\e '
#           ^^ Make sure this is present
```

### Exit Code Colors Not Working

1. Wrap characters between two `\e` sequences:
```bash
# Wrong
set PS1='\w \e#> '

# Right
set PS1='\w \e#>\e '
#           ^   ^
```

2. Check colors are enabled:
```bash
set colors=on
```

### Prompt Too Slow

Git integration can be slow in large repos:

```bash
# Disable git for faster prompt
set PS1='\w \e#>\e '  # No \g

# Or use simpler prompt
set PS1='\W #> '
```

## Advanced Usage

### Conditional Formatting

While hash doesn't support conditional formatting directly in PS1, you can create different .hashrc files:

```bash
# ~/.hashrc.work
set PS1='\u@\h:\w\g\$ '

# ~/.hashrc.home
set PS1='\W\g \e#>\e '
```

Then source the appropriate one.

### Project-Specific Prompts

Create `.hashrc` in project directories:

```bash
# ~/project/.hashrc
set PS1='[PROJECT] \W\g \e#>\e '
```

Then: `source .hashrc` when entering the project.

### Integration with Aliases

Combine with aliases for productivity:

```bash
# Quick prompt switching
alias prompt-simple='export PS1="\W #> "'
alias prompt-full='export PS1="\w\g \e#>\e "'
```

## Comparison with Bash

### Similar Features
- `\w`, `\W`, `\u`, `\h`, `\$` work the same
- Color support
- Environment variable support

### Hash-Specific
- `\g` - Git integration
- `\e` - Exit code color indicator
- Automatic ~ substitution for home

### Not Supported (Yet)
- `\d` - Date
- `\t` - Time
- `\!` - History number
- `\[` and `\]` - Non-printing sequences

## See Also

- [.hashrc Configuration](HASHRC.md)
- [Color Support](COLORS.md)
- Example prompts in `examples/.hashrc`
