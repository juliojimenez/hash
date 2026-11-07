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

## It Is a Shell Script

`.hashrc` runs through the shell like any other script, so anything you can type
at the prompt works in it: functions, `if` and `case`, `source` for files you
share with other shells, command substitution in exports.

```bash
# Functions
mkcd() { mkdir -p "$1" && cd "$1"; }

# Conditionals
if [ -d "$HOME/bin" ]; then
    export PATH="$HOME/bin:$PATH"
fi

# Shared settings
[ -f ~/.aliases ] && source ~/.aliases
```

Errors in the file are reported when the shell starts, so a typo shows up right
away instead of being skipped. A `return` ends the file and the shell carries on;
an `exit` ends the shell before the prompt, with that status.

## Aliases

Define command shortcuts:

```bash
alias name='command'
```

Aliases are expanded when a command is read, as POSIX requires, so a definition
takes effect from the next line on: everything in `.hashrc` is ready for the
first prompt, while `alias x='echo hi'; x` on one line does not find `x`. The
name has to be the first word of a command and unquoted (`\ls` or `'ls'` skip
an `ls` alias), the text may hold reserved words (`alias not='! '` works in
`if not test -f file; then ...`), and a text that ends in a blank has the word
after it checked for an alias too (`alias sudo='sudo '`).

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
| `prompt` | `classic`, `plain`, `minimal`, `two-line`, `powerline`, `list` | Pick a prompt preset; `list` shows them |
| `PS1` | format | Custom prompt |
| `color.<element>` | style | Color of one element, e.g. `color.prompt=bold,blue` |
| `syntax_highlight` | `on`/`off` | Highlight the command line as you type |
| `autosuggest` | `on`/`off` | Suggest completions from history |
| `danger_highlight` | `on`/`off` | Highlight dangerous commands |

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

### Variable Not Visible to Programs

`MY_VAR=value` sets a shell variable. Programs you run only see it after `export`:

```bash
MY_VAR=value         # shell variable only
export MY_VAR=value  # also in the environment
```

### Config Not Loading

Check file location and reload:

```bash
ls -la ~/.hashrc
source ~/.hashrc
```
