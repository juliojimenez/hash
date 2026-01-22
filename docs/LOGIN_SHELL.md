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

## Logout File

### `~/.hash_logout`

When exiting a login shell (via `exit` or `logout`), hash executes commands in `~/.hash_logout` if it exists.

**Common uses:**

```bash
# ~/.hash_logout example

# Clear screen for security (prevent shoulder surfing)
clear

# Display farewell message
echo "Goodbye! Session ended at $(date)"

# Clean up temporary files
rm -rf ~/tmp/session_* 2>/dev/null

# Log session end time
echo "$(date): Session ended" >> ~/.session_log
```

**Important:** The logout file is **only executed for login shells**. Regular interactive shells (subshells) will not run this file.

### The `logout` Command

Hash provides a `logout` builtin command specifically for login shells:

```bash
# In a login shell
#> logout
Bye :)

# In a non-login shell
#> logout
hash: logout: not a login shell
```

The `logout` command:
- Only works in login shells (use `exit` for non-login shells)
- Warns about running background jobs (just like `exit`)
- Executes `~/.hash_logout` before exiting

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
- Put **cleanup tasks** in `~/.hash_logout`

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

**~/.hash_logout:**
```bash
# Clear screen on logout
clear

# Farewell message
echo "Session ended. Goodbye!"
```

## Testing Login Shell Behavior

```bash
# Test as login shell
hash --login

# Or simulate how SSH invokes it
exec -a -hash /usr/local/bin/hash

# Check if you're in a login shell (look for "(login)" in welcome)
hash v29 (login)
Type 'exit' to quit

# Test logout
logout
# Should run ~/.hash_logout and exit
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
| Logout file | `~/.bash_logout` | `~/.hash_logout` |
| System profile | `/etc/profile` | `/etc/profile` |
| Logout command | `logout` | `logout` |

## Summary of Files

| File | When Loaded | Purpose |
|------|-------------|---------|
| `/etc/profile` | Login shell startup | System-wide settings |
| `~/.hash_profile` | Login shell startup | User login settings |
| `~/.hash_login` | Login shell startup | Alternative to hash_profile |
| `~/.profile` | Login shell startup (fallback) | Shared with other shells |
| `~/.hashrc` | Any interactive shell | Interactive settings |
| `~/.hash_logout` | Login shell exit | Cleanup tasks |

## Future Enhancements

Planned features for login shell support:

- [ ] Conditional execution based on login shell status
- [ ] `shopt` equivalent for shell options
- [ ] Non-interactive shell mode (for scripts)

Back to [README](../README.md)
