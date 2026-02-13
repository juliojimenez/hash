# Login Shell Setup

Hash supports proper login shell behavior, loading different startup files depending on how it's invoked.

## Setting Hash as Default Shell

### Linux (chsh)

```bash
sudo bash -c 'echo "/usr/local/bin/hash-shell" >> /etc/shells'
chsh -s /usr/local/bin/hash-shell
```

### Linux (usermod)

```bash
sudo usermod -s /usr/local/bin/hash-shell your_username
```

### macOS

```bash
sudo bash -c 'echo "/usr/local/bin/hash-shell" >> /etc/shells'
chsh -s /usr/local/bin/hash-shell
```

If installed with Homebrew:

```bash
sudo bash -c 'echo "$(brew --prefix)/bin/hash-shell" >> /etc/shells'
chsh -s "$(brew --prefix)/bin/hash-shell"
```

### FreeBSD

```bash
sudo sh -c 'echo "/usr/local/bin/hash-shell" >> /etc/shells'
chsh -s /usr/local/bin/hash-shell
```

**Note:** Log out and log back in for changes to take effect.

## Login Shell Detection

Hash considers itself a login shell when:

1. `argv[0]` starts with `-` (how SSH/login invoke shells)
2. `--login` or `-l` flag is used

Login shells display "(login)" in the welcome message.

## Startup File Order

### Login Shell

When invoked as a login shell:

1. `/etc/profile` - System-wide settings
2. User profile (first found):
   - `~/.hash_profile`
   - `~/.hash_login`
   - `~/.profile`
3. `~/.hashrc` - Interactive settings

### Non-Login Interactive Shell

```
~/.hashrc only
```

## Configuration Files

### ~/.hash_profile

User-specific login configuration:

```bash
# Environment
export EDITOR=vim
export PAGER=less

# PATH
export PATH=$HOME/bin:$HOME/.local/bin:$PATH
```

### ~/.hashrc

Interactive settings:

```bash
# Aliases
alias ll='ls -lah'
alias gs='git status'

# Prompt
set PS1='\W\g \e#>\e '
```

### ~/.hash_logout

Executed when exiting a login shell:

```bash
# Clear screen
clear

# Farewell message
echo "Goodbye! Session ended at $(date)"

# Cleanup
rm -rf ~/tmp/session_* 2>/dev/null
```

## The logout Command

Use `logout` to exit login shells:

```bash
# In a login shell
#> logout
Bye :)

# In a non-login shell
#> logout
hash: logout: not a login shell
# Use 'exit' instead
```

## Best Practices

### Separate Concerns

- **~/.hash_profile**: Environment variables, PATH
- **~/.hashrc**: Aliases, prompt settings
- **~/.hash_logout**: Cleanup tasks

### Example Setup

**~/.hash_profile:**
```bash
export EDITOR=vim
export GOPATH=$HOME/go
export PATH=$HOME/bin:$GOPATH/bin:$PATH
```

**~/.hashrc:**
```bash
set PS1='\u@\h:\W\g #> '
alias ll='ls -lah'
set colors=on
```

**~/.hash_logout:**
```bash
clear
echo "Session ended. Goodbye!"
```

## Testing

```bash
# Test as login shell
hash-shell --login

# Check for "(login)" in welcome message
hash v38 (login)
Type 'exit' to quit
```
