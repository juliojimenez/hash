# Hash Shell Configuration (.hashrc)

Hash shell supports configuration through a `.hashrc` file in your home directory. This allows you to customize your shell experience with aliases, environment variables, and settings.

## Quick Start

1. **Create your .hashrc file:**

```bash
cp examples/.hashrc ~/.hashrc
```

2. **Edit it:**

```bash
vim ~/.hashrc
```

3. **Reload configuration:**

```bash
source ~/.hashrc
# or restart hash
```

## Configuration File Location

Hash looks for configuration in: `~/.hashrc`

The file is loaded automatically when hash starts.

## Syntax

### Comments

Lines starting with `#` are comments and are ignored:

```bash
# This is a comment
alias ll='ls -lah'  # This is also a comment
```

### Aliases

Define command shortcuts with `alias`:

```bash
alias name='command'
alias name="command"
```

**Examples:**
```bash
# Simple alias
alias ll='ls -lah'

# Alias with options
alias grep='grep --color=auto'

# Alias with pipes
alias psgrep='ps aux | grep'

# Navigation shortcuts
alias ..='cd ..'
alias ...='cd ../..'

# Git shortcuts
alias gs='git status'
alias gp='git push'
```

### Environment Variables

Set environment variables with `export`:

```bash
export VARIABLE=value
export VARIABLE='value with spaces'
export VARIABLE="value with $substitution"
```

**Examples:**
```bash
# Set default editor
export EDITOR=vim

# Add to PATH
export PATH=$HOME/bin:$PATH

# Set locale
export LANG=en_US.UTF-8

# Custom variables
export MY_API_KEY=abc123
```

### Shell Options

Configure shell behavior with `set`:

```bash
set option=value
```

**Available options:**
- `colors=on|off` - Enable or disable colored output
- `welcome=on|off` - Show or hide welcome message
- `PS1=<format>` - Set custom prompt format

**Examples:**
```bash
# Disable colors
set colors=off

# Hide welcome message
set welcome=off

# Set custom prompt (see PROMPT.md for details)
set PS1='\W\g \e#>\e '
```

## Prompt Customization

Hash supports powerful prompt customization through the `PS1` variable:

```bash
# Simple prompt
set PS1='\W #> '

# Full path with git integration
set PS1='\w\g \e#>\e '

# User@host:path
set PS1='\u@\h:\w\$ '
```

**Common PS1 escape sequences:**
- `\w` - Full current path
- `\W` - Current directory name only
- `\u` - Username
- `\h` - Hostname
- `\g` - Git branch with status
- `\e` - Exit code color indicator

## Built-in Commands

### alias

Manage aliases interactively:

```bash
# List all aliases
alias

# Show specific alias
alias ll

# Create alias
alias ll='ls -lah'

# Create alias with equals in command
alias serve='python3 -m http.server'
```

### unalias

Remove an alias:

```bash
unalias ll
unalias gs
```

### cd

Change directory:

```bash
cd /tmp              # Change to /tmp
cd ~                 # Change to home directory
cd ~/Documents       # Change to ~/Documents
cd                   # No arguments - goes to home directory (like bash)
```

### source

Reload configuration file:

```bash
# Reload default .hashrc
source ~/.hashrc

# Load another config file
source ~/my-custom-config
```

### exit

Exit the shell:

```bash
exit
```

## How Aliases Work

When you type a command, hash:

1. Checks if it's an alias
2. Expands the alias
3. Appends any additional arguments
4. Executes the expanded command

**Example:**
```bash
# Define alias
alias ll='ls -lah'

# Running this:
ll /tmp

# Expands to:
ls -lah /tmp
```

## Common Patterns

### Navigation Shortcuts

```bash
alias ..='cd ..'
alias ...='cd ../..'
alias ....='cd ../../..'
alias ~='cd ~'
alias -- -='cd -'  # Go back to previous directory
```

### Safety Nets

```bash
alias rm='rm -i'    # Prompt before removal
alias cp='cp -i'    # Prompt before overwrite
alias mv='mv -i'    # Prompt before overwrite
alias ln='ln -i'    # Prompt before overwrite
```

### Colorized Commands

```bash
alias ls='ls --color=auto'
alias grep='grep --color=auto'
alias diff='diff --color=auto'
```

### Quick Directory Access

```bash
alias projects='cd ~/projects'
alias docs='cd ~/Documents'
alias downloads='cd ~/Downloads'
```

### Development Tools

```bash
# Python
alias py='python3'
alias pip='pip3'
alias venv='python3 -m venv'
alias serve='python3 -m http.server'

# Node.js
alias ni='npm install'
alias ns='npm start'
alias nt='npm test'

# Git
alias g='git'
alias gs='git status'
alias ga='git add'
alias gc='git commit'
alias gp='git push'
alias gl='git pull'
alias gd='git diff'
alias gco='git checkout'
```

### Docker

```bash
alias d='docker'
alias dc='docker-compose'
alias dps='docker ps'
alias dimg='docker images'
alias dexec='docker exec -it'
alias dlogs='docker logs -f'
```

### System Information

```bash
alias meminfo='free -m -l -t'
alias cpuinfo='lscpu'
alias diskinfo='df -h'
alias ports='netstat -tulanp'
alias processes='ps aux | grep'
```

## Example .hashrc

See `examples/.hashrc` for a comprehensive example with:
- Common aliases
- Environment variables
- Shell options
- Organized by category
- Commented examples

## Tips & Best Practices

### 1. Organize by Category

Group related aliases together:
```bash
# ============================================================================
# GIT ALIASES
# ============================================================================
alias gs='git status'
alias ga='git add'
...

# ============================================================================
# DOCKER ALIASES
# ============================================================================
alias dps='docker ps'
...
```

### 2. Document Your Aliases

Add comments explaining non-obvious aliases:
```bash
# Run development server on port 3000
alias dev='npm run dev -- --port 3000'
```

### 3. Use Descriptive Names

Prefer clarity over brevity:
```bash
# Good
alias serve-here='python3 -m http.server'

# Less clear
alias s='python3 -m http.server'
```

### 4. Don't Override Important Commands

Avoid aliasing core commands that might confuse scripts:
```bash
# Bad
alias cd='cd && ls'

# Good
alias cdd='cd && ls'
```

### 5. Test Before Adding

Try aliases interactively before adding to .hashrc:
```bash
# Test it
alias test='echo "This is a test"'
test

# If it works, add to .hashrc
```

### 6. Reload After Editing

After editing .hashrc:
```bash
source ~/.hashrc
```

## Troubleshooting

### Alias not working

1. **Check syntax:**
```bash
# Wrong
alias ll=ls -lah

# Right  
alias ll='ls -lah'
```

2. **Check for typos:**
```bash
# List all aliases to verify
alias
```

3. **Reload config:**
```bash
source ~/.hashrc
```

### Environment variable not set

1. **Use export:**
```bash
# Wrong
MY_VAR=value

# Right
export MY_VAR=value
```

2. **Check value:**
```bash
echo $MY_VAR
```

### Config not loading

1. **Check file location:**
```bash
ls -la ~/.hashrc
```

2. **Check for syntax errors:**
```bash
# Hash will show warnings for invalid lines
```

3. **Manually source:**
```bash
source ~/.hashrc
```

## Advanced Usage

### Conditional Aliases

Since .hashrc is just a config file, you can't use conditionals directly. Instead, set up different aliases:

```bash
# Development environment
alias start-dev='npm run dev'

# Production environment  
alias start-prod='npm run start'
```

### Platform-Specific Aliases

Create separate .hashrc files:
```bash
~/.hashrc              # Main config
~/.hashrc.linux        # Linux-specific
~/.hashrc.mac          # macOS-specific
```

Then source the appropriate one from main .hashrc.

### Chaining Commands

```bash
# Update system and clean
alias update='sudo apt update && sudo apt upgrade && sudo apt autoremove'

# Git commit and push
alias gcp='git commit && git push'
```

## Future Enhancements

Planned features for .hashrc:
- [ ] Functions (like bash functions)
- [ ] Conditionals based on environment
- [ ] Include/import other config files
- [ ] Key bindings
- [ ] Command history settings

Back to [README](../README.md)
