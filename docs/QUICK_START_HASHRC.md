# .hashrc Quick Start

Get up and running with hash shell configuration in 5 minutes!

## Step 1: Create Your .hashrc

```bash
# Copy the example file
cp examples/.hashrc ~/.hashrc

# Or create from scratch
vim ~/.hashrc
```

## Step 2: Add Your First Aliases

Add these common aliases to your `~/.hashrc`:

```bash
# Navigation
alias ..='cd ..'
alias ...='cd ../..'

# List files
alias ll='ls -lah'
alias la='ls -A'

# Safety
alias rm='rm -i'
alias cp='cp -i'
alias mv='mv -i'

# Git
alias gs='git status'
alias ga='git add'
alias gc='git commit'
alias gp='git push'
```

## Step 3: Test Your Aliases

Reload the configuration:

```bash
# In hash shell:
source ~/.hashrc

# Or restart hash
exit
./hash
```

Try your aliases:

```bash
#> ll
# Shows detailed file listing

#> ..
# Goes up one directory

#> gs
# Shows git status
```

## Step 4: Customize Colors (Optional)

Add to your `.hashrc`:

```bash
# Enable colors
set colors=on

# Or disable if you prefer
set colors=off
```

## Step 5: Set Environment Variables

Add useful environment variables:

```bash
# Default editor
export EDITOR=vim

# Add custom bin directory
export PATH=$HOME/bin:$PATH

# Set locale
export LANG=en_US.UTF-8
```

## Common Use Cases

### For Developers

```bash
# Python
alias py='python3'
alias serve='python3 -m http.server'

# Node.js
alias ni='npm install'
alias ns='npm start'

# Docker
alias dps='docker ps'
alias dlogs='docker logs -f'
```

### For System Admins

```bash
# System info
alias meminfo='free -m -l -t'
alias cpuinfo='lscpu'
alias ports='netstat -tulanp'

# Logs
alias syslog='tail -f /var/log/syslog'
alias auth='tail -f /var/log/auth.log'
```

### For Everyone

```bash
# Network
alias myip='curl ifconfig.me'
alias ping='ping -c 5'

# File operations
alias grep='grep --color=auto'
alias mkdir='mkdir -pv'

# Quick edits
alias bashrc='vim ~/.bashrc'
alias hashrc='vim ~/.hashrc'
```

## Interactive Commands

You can also manage aliases from within hash:

```bash
# Create alias
#> alias ll='ls -lah'
Alias 'll' added

# List all aliases
#> alias
ll='ls -lah'
gs='git status'

# Remove alias
#> unalias ll
Alias 'll' removed

# Reload config
#> source ~/.hashrc
Loaded config from '/home/user/.hashrc'
```

## Tips

1. **Group related aliases** - organize by category (git, docker, system, etc.)
2. **Add comments** - document what each alias does
3. **Test before adding** - try aliases interactively first
4. **Reload after changes** - run `source ~/.hashrc`
5. **Keep it simple** - start small and add as you go

## Next Steps

- Read the full [.hashrc documentation](HASHRC.md)
- See the [example .hashrc](../examples/.hashrc)
- Learn about [built-in commands](BUILTINS.md)
- Explore [color options](COLORS.md)

## Troubleshooting

**Alias not working?**
```bash
# Check syntax (need quotes!)
alias ll='ls -lah'  # ✅ Good
alias ll=ls -lah     # ❌ Bad

# Reload config
source ~/.hashrc
```

**Want to reset?**
```bash
# Remove all aliases
rm ~/.hashrc
# Restart hash
```

Happy customizing! 🎨
