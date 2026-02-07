# Quick Start

Welcome to hash shell! This guide will help you get started with the basics.

## First Steps

After [installing hash](../installation/README.md), start it by running:

```bash
hash-shell
```

You'll see:

```
hash v35
Type 'exit' to quit

#>
```

## Basic Commands

Hash works like any POSIX shell:

```bash
#> echo "Hello, World!"
Hello, World!

#> ls -la
# Lists files

#> cd ~/projects
# Changes directory

#> pwd
/home/user/projects
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-c string` | Execute commands from string |
| `-i` | Force interactive mode |
| `-l`, `--login` | Run as a login shell |
| `-s` | Read commands from standard input |
| `-v`, `--version` | Print version information |
| `-h`, `--help` | Show help message |

### Examples

```bash
# Execute a command
hash-shell -c 'echo "Hello!"'

# Run a script
hash-shell script.sh

# Run as login shell
hash-shell --login
```

## Configuration

Hash loads configuration from `~/.hashrc` on startup. Create your configuration file:

```bash
cp /path/to/hash/examples/.hashrc ~/.hashrc
```

Or create a minimal one:

```bash
# ~/.hashrc
alias ll='ls -lah'
alias gs='git status'
set PS1='\W\g \e#>\e '
```

See [Configuration (.hashrc)](hashrc.md) for complete details.

## Key Features

- **Line Editing**: Arrow keys, Ctrl+A/E, history navigation
- **Tab Completion**: Complete commands, files, and directories
- **Git Integration**: Branch and status in prompt
- **Aliases**: Create command shortcuts
- **Scripting**: POSIX-compatible shell scripts

## Next Steps

- [Configure your .hashrc](hashrc.md) - Customize your shell
- [Set up as login shell](login-shell.md) - Make hash your default
- [Learn line editing](../shell-features/line-editing.md) - Master keyboard shortcuts
- [Write scripts](../scripting/README.md) - Automate tasks
