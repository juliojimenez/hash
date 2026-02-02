# Configuration (.hashrc)

Hash shell supports configuration through a `.hashrc` file in your home directory.

## Quick Start

Create your configuration file:

```bash
vim ~/.hashrc
```

Example `.hashrc`:

```bash
# Aliases
alias ll='ls -lah'
alias gs='git status'
alias gp='git push'

# Environment
export EDITOR=vim
export PATH=$HOME/bin:$PATH

# Prompt
set PS1='\W\g \e#>\e '

# Options
set colors=on
set welcome=off
```

Reload after editing:

```bash
source ~/.hashrc
```

## Aliases

Define command shortcuts:

```bash
alias name='command'
```

### Examples

```bash
# Navigation
alias ..='cd ..'
alias ...='cd ../..'

# Safety
alias rm='rm -i'
alias cp='cp -i'
alias mv='mv -i'

# Git
alias gs='git status'
alias ga='git add'
alias gc='git commit'
alias gp='git push'
alias gd='git diff'

# Docker
alias d='docker'
alias dc='docker-compose'
alias dps='docker ps'
```

### Managing Aliases

```bash
# List all aliases
alias

# Show specific alias
alias ll

# Remove alias
unalias ll
```

## Environment Variables

Set variables with `export`:

```bash
export VARIABLE=value
export VARIABLE='value with spaces'
export VARIABLE="value with $substitution"
```

### Common Variables

```bash
# Editor
export EDITOR=vim
export VISUAL=vim

# Path
export PATH=$HOME/bin:$PATH

# Locale
export LANG=en_US.UTF-8

# Custom
export MY_PROJECT=~/projects/myapp
```

## Shell Options

Configure behavior with `set`:

```bash
set option=value
```

### Available Options

| Option | Values | Description |
|--------|--------|-------------|
| `colors` | `on`/`off` | Enable colored output |
| `welcome` | `on`/`off` | Show welcome message |
| `PS1` | format | Custom prompt |

```bash
set colors=on
set welcome=off
set PS1='\W\g \e#>\e '
```

## Prompt Customization

Set your prompt with the `PS1` variable:

```bash
# Current directory + git + exit color
set PS1='\W\g \e#>\e '

# Full path + git
set PS1='\w\g \e#>\e '

# User@host:path
set PS1='\u@\h:\w\$ '
```

### Escape Sequences

| Sequence | Description |
|----------|-------------|
| `\w` | Full current path |
| `\W` | Current directory name |
| `\u` | Username |
| `\h` | Hostname |
| `\g` | Git branch with status |
| `\e` | Exit code color |
| `\$` | `$` for user, `#` for root |
| `\n` | Newline |

See [Prompt Customization](../shell-features/prompt.md) for complete details.

## Comments

Lines starting with `#` are ignored:

```bash
# This is a comment
alias ll='ls -lah'  # Inline comment
```

## Tips

### Organize by Category

```bash
# ============================================================================
# NAVIGATION
# ============================================================================
alias ..='cd ..'
alias projects='cd ~/projects'

# ============================================================================
# GIT
# ============================================================================
alias gs='git status'
alias gp='git push'
```

### Test Before Adding

Try aliases interactively first:

```bash
#> alias test='echo "works"'
#> test
works
# Now add to .hashrc
```

### Reload After Changes

```bash
source ~/.hashrc
```

## Troubleshooting

### Alias Not Working

Check syntax (quotes required):

```bash
# Wrong
alias ll=ls -lah

# Correct
alias ll='ls -lah'
```

### Variable Not Set

Use `export`:

```bash
# Wrong
MY_VAR=value

# Correct
export MY_VAR=value
```

### Config Not Loading

Check file location and reload:

```bash
ls -la ~/.hashrc
source ~/.hashrc
```
