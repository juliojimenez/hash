# Tilde Expansion in Hash Shell

Hash shell automatically expands the tilde (`~`) character to represent home directories in all commands and arguments.

## Basic Usage

### Current User's Home

```bash
# ~ expands to your home directory
#> cd ~
#> pwd
/home/user

#> echo ~
/home/user

#> ls ~
# Lists your home directory

#> cat ~/file.txt
# Opens file.txt in your home directory
```

### With Paths

```bash
# Tilde with path
#> cd ~/Documents
#> pwd
/home/user/Documents

#> cp file.txt ~/backup/
# Copies to /home/user/backup/

#> vim ~/.bashrc
# Opens /home/user/.bashrc
```

### Other Users' Home Directories

```bash
# ~username expands to that user's home
#> ls ~root
# Lists /root

#> cd ~julio
# Changes to /home/julio

#> cat ~ubuntu/.bashrc
# Opens /home/ubuntu/.bashrc
```

## How It Works

Tilde expansion happens **after parsing but before execution**:

1. Command is parsed into tokens
2. Each token is checked for tilde at the start
3. If found, tilde is replaced with home directory
4. Expanded arguments are passed to command

## Expansion Rules

### When ~ is Expanded

✅ At the **beginning** of an argument:
```bash
cd ~              → cd /home/user
ls ~/Documents    → ls /home/user/Documents
echo ~            → echo /home/user
cat ~/file.txt    → cat /home/user/file.txt
```

✅ With **usernames**:
```bash
cd ~root          → cd /root
ls ~julio/work    → ls /home/julio/work
```

### When ~ is NOT Expanded

❌ In the **middle** of an argument:
```bash
echo hello~world  → hello~world (not expanded)
```

❌ **After** other characters:
```bash
echo /tmp~        → /tmp~ (not expanded)
```

❌ When **not at start**:
```bash
echo test~        → test~ (not expanded)
```

## Examples

### File Operations

```bash
# Copy files
#> cp ~/file.txt ~/backup/

# Move files
#> mv ~/Downloads/file.pdf ~/Documents/

# Create directories
#> mkdir ~/projects/new-project

# Remove files
#> rm ~/old-file.txt
```

### Navigation

```bash
# Go home
#> cd ~

# Go to subdirectory
#> cd ~/projects

# Go to another user's home
#> cd ~root

# Use in other commands
#> ls -la ~/Documents
```

### Editing Files

```bash
# Edit config files
#> vim ~/.hashrc
#> vim ~/.vimrc
#> cat ~/.ssh/config
```

### Multiple Arguments

```bash
# Multiple tildes expand independently
#> diff ~/file1.txt ~/file2.txt

#> cp ~/source.txt ~/backup/dest.txt

#> cat ~/file1 ~/file2 ~/file3
```

### With Aliases

```bash
# Tilde expands in alias values
alias docs='cd ~/Documents'
alias vimrc='vim ~/.vimrc'
alias backup='cp $1 ~/backup/'  # Note: $1 not expanded (no var substitution yet)
```

## Special Cases

### Home Directory Not Set

If `$HOME` is not set, hash tries to get it from the user database (`/etc/passwd`).

### User Not Found

If expanding `~username` and user doesn't exist:
```bash
#> cd ~nonexistentuser
# Tilde stays as-is, cd will fail with error
```

### Empty Arguments

```bash
#> echo "" ~
 /home/user
# Empty string stays empty, tilde expands
```

## Comparison with Bash

Hash shell's tilde expansion is similar to bash:

| Pattern | Hash | Bash |
|---------|------|------|
| `~` | ✅ | ✅ |
| `~/path` | ✅ | ✅ |
| `~user` | ✅ | ✅ |
| `~user/path` | ✅ | ✅ |
| `~+` (current dir) | ❌ | ✅ |
| `~-` (previous dir) | ❌ | ✅ |

## Integration with Other Features

### Works With Aliases

```bash
#> alias home='cd ~'
#> home
# Goes to /home/user
```

### Works With Chaining

```bash
#> cd ~ && ls
# Changes to home and lists contents

#> test -f ~/file.txt && cat ~/file.txt
# Checks if file exists and cats it
```

### Works With Quotes

Currently, tilde expansion happens regardless of quotes:

```bash
#> echo "~"
/home/user

#> echo '~'
/home/user
```

**Note:** This differs from bash, where single quotes prevent expansion. This may be refined in future versions.

## Tips

### Quick Navigation

```bash
# Add to .hashrc
alias home='cd ~'
alias docs='cd ~/Documents'
alias downloads='cd ~/Downloads'
alias projects='cd ~/projects'
```

### Backup Shortcuts

```bash
# Add to .hashrc
alias backup='cp $1 ~/backup/'  # When var expansion is added
alias vimrc='vim ~/.hashrc'
```

### File Paths

Instead of typing full paths:
```bash
# Long way
#> cat /home/user/projects/notes.txt

# Short way
#> cat ~/projects/notes.txt
```

## Limitations

Current limitations (may be added in future):
- ❌ No `~+` (current directory)
- ❌ No `~-` (previous directory)
- ❌ Quote-aware expansion (currently expands in all quotes)
- ❌ Tilde in middle of string not expanded

Back to [README](../README.md)
