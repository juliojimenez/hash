# Interactive Features

Hash shell includes powerful interactive features for efficient command-line use.

## Features Overview

| Feature | Description |
|---------|-------------|
| [Line Editing](line-editing.md) | Cursor navigation, editing shortcuts |
| [Tab Completion](tab-completion.md) | Complete commands, files, directories |
| [Command History](history.md) | Navigate and search previous commands |
| [Prompt Customization](prompt.md) | Git integration, colors, custom format |

## Quick Reference

### Line Editing

| Key | Action |
|-----|--------|
| `Ctrl+A` | Move to beginning of line |
| `Ctrl+E` | Move to end of line |
| `Ctrl+U` | Delete to beginning |
| `Ctrl+K` | Delete to end |
| `Ctrl+W` | Delete word backward |
| `Ctrl+L` | Clear screen |
| `Ctrl+C` | Cancel current line |

### History Navigation

| Key | Action |
|-----|--------|
| `Up` | Previous command |
| `Down` | Next command |
| `!!` | Repeat last command |
| `!n` | Run command number n |
| `!prefix` | Run last command starting with prefix |

### Tab Completion

| Context | Completes |
|---------|-----------|
| First word | Commands, aliases, builtins |
| Other words | Files, directories |
| Double TAB | Show all matches |

## Default Prompt

The default prompt shows:

```
/home/user/projects git:(main) #>
```

- **Blue** path
- **Green/Yellow** git status (clean/dirty)
- **Cyan** branch name
- **Blue/Red** `#>` based on exit code

## Configuration

Configure interactive features in `~/.hashrc`:

```bash
# Custom prompt
set PS1='\W\g \e#>\e '

# Enable colors
set colors=on
```
