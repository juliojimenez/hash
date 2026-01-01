# Login Shell Behavior

Hash supports proper login shell behavior, loading different startup files depending on how it's invoked.

## Login Shell Detection

Hash considers itself a login shell when:

1. **`argv[0]` starts with `-`** - This is how `login`, `sshd`, and similar programs invoke login shells (e.g., `-hash`)
2. **`--login` or `-l` flag** - Explicitly request login shell behavior

When running as a login shell, hash displays "(login)" in the welcome message.

## Startup File Loading Order

### Login Shell

When hash is invoked as a login shell (SSH, console login, `hash --login`):

1. **`/etc/profile`** - System-wide settings (if it exists)
2. **User profile** - First one found:
   - `~/.hash_profile` (hash-specific)
   - `~/.hash_login` (hash-specific, alternative name)
   - `~/.profile` (shared with other shells)
3. **`~/.hashrc`** - Interactive settings

### Interactive Non-Login Shell

When hash is invoked as a regular interactive shell (running `hash` from another shell):

1. **`~/.hashrc`** only

## Profile Files

### `/etc/profile`

System-wide configuration. Typically managed by the system administrator:

```bash
# /etc/profile example
export PATH=/usr/local/bin:$PATH
export LANG=en_US.UTF-8
```

### `~/.hash_profile`

User-specific login configuration. Use this for:

- Environment variables needed by other programs
- PATH modifications
- One-time setup that only needs to run at login

```bash
# ~/.hash_profile example

# Environment setup
export EDITOR=vim
export VISUAL=vim
export PAGER=less

# PATH additions
export PATH=$HOME/bin:$HOME/.local/bin:$PATH

# SSH agent (if not already running)
# Note: hash doesn't support conditionals yet, so this is pseudo-code
# if [ -z "$SSH_AUTH_SOCK" ]; then
#     eval $(ssh-agent)
# fi
```

### `~/.hashrc`

Interactive shell configuration. Use this for:

- Aliases
- Prompt customization
- Shell options

```bash
# ~/.hashrc example

# Aliases
alias ll='ls -lah'
alias la='ls -A'
alias grep='grep --color=auto'

# Prompt
set PS1='\W\g \e#>\e '

# Shell options
set colors=on
set welcome=on
```

## Best Practices

### Separate Concerns

- Put **environment variables** and **PATH** in `~/.hash_profile`
- Put **aliases** and **prompt settings** in `~/.hashrc`

This follows the Unix convention and ensures your environment is properly set up whether you're in a login shell or a subshell.

### Example Setup

**~/.hash_profile:**
```bash
# Environment
export EDITOR=vim
export GOPATH=$HOME/go
export PATH=$HOME/bin:$GOPATH/bin:$PATH

# Load hashrc for interactive settings
# (hash does this automatically, but explicit is fine too)
```

**~/.hashrc:**
```bash
# Prompt
set PS1='\u@\h:\W\g #> '

# Aliases
alias ll='ls -lah'
alias gs='git status'
alias gp='git push'

# Options
set colors=on
set welcome=off
```

## Testing Login Shell Behavior

```bash
# Test as login shell
hash --login

# Or simulate how SSH invokes it
exec -a -hash /usr/local/bin/hash

# Check if you're in a login shell (look for "(login)" in welcome)
hash v14 (login)
Type 'exit' to quit
```

## Compatibility with Other Shells

If you want to share profile settings with bash or other shells:

1. Put shared settings in `~/.profile`
2. Use `~/.hash_profile` only for hash-specific settings
3. Hash will fall back to `~/.profile` if no hash-specific profile exists

## Differences from Bash

| Feature | Bash | Hash |
|---------|------|------|
| Login profile | `~/.bash_profile` | `~/.hash_profile` |
| Login alternative | `~/.bash_login` | `~/.hash_login` |
| Fallback profile | `~/.profile` | `~/.profile` |
| Interactive config | `~/.bashrc` | `~/.hashrc` |
| System profile | `/etc/profile` | `/etc/profile` |

## Future Enhancements

Planned features for login shell support:

- [ ] Logout file (`~/.hash_logout`)
- [ ] Conditional execution based on login shell status
- [ ] `shopt` equivalent for shell options
- [ ] Non-interactive shell mode (for scripts)

Back to [README](../README.md)
