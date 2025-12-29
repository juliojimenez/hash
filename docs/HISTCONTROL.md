# HISTCONTROL in Hash Shell

The `HISTCONTROL` environment variable controls how hash shell saves commands to history.

## Options

### ignorespace

Commands starting with a space are NOT saved to history:

```bash
#> export HISTCONTROL=ignorespace

#> echo public
#>  echo private  ← note the space

#> history
    0  echo public
# "echo private" not saved!
```

**Use case:** Sensitive commands with passwords, API keys, etc.

### ignoredups

Consecutive duplicate commands are NOT saved:

```bash
#> export HISTCONTROL=ignoredups

#> ls
#> ls
#> ls
#> pwd

#> history
    0  ls
    1  pwd
# Only one "ls" entry!
```

**Use case:** Avoid cluttering history with repeated commands.

### ignoreboth

Combines `ignorespace` and `ignoredups`:

```bash
#> export HISTCONTROL=ignoreboth

# Ignores leading space AND duplicates
```

**Use case:** Common default setting, recommended for most users.

### erasedups

ALL previous instances of a command are removed before saving:

```bash
#> export HISTCONTROL=erasedups

#> echo first
#> ls
#> pwd
#> ls
#> echo test
#> ls

#> history
    0  echo first
    1  pwd  
    2  echo test
    3  ls
# Only most recent "ls" kept!
```

**Use case:** Keep history clean with only latest instance of commands.

## Setting HISTCONTROL

### In .hashrc

Add to `~/.hashrc`:

```bash
# Recommended setting
export HISTCONTROL=ignoreboth

# Or customize
export HISTCONTROL=ignoredups
export HISTCONTROL=ignorespace
export HISTCONTROL=erasedups
```

### Interactively

```bash
#> export HISTCONTROL=ignoreboth
# Takes effect immediately
```

### Multiple Options

You can combine by separating with colons (like bash):

```bash
export HISTCONTROL=ignorespace:ignoredups
# Same as ignoreboth

export HISTCONTROL=ignoreboth:erasedups
# Ignore space+dups AND erase all previous dups
```

## Default Behavior

If `HISTCONTROL` is not set, hash uses:
- `ignorespace` - enabled
- `ignoredups` - enabled

This matches common bash defaults.

## Examples

### Privacy Mode

```bash
# In .hashrc
export HISTCONTROL=ignorespace

# Then in shell
#>  export API_KEY=secret123
#>  curl -H "Auth: $API_KEY" api.example.com
# Neither command saved to history!
```

### Clean History

```bash
# In .hashrc
export HISTCONTROL=erasedups

# Your history only keeps latest instance of each command
```

### Minimal Clutter

```bash
# In .hashrc
export HISTCONTROL=ignoreboth

# Ignores duplicates AND space-prefixed commands
```

## Checking Current Setting

```bash
#> echo $HISTCONTROL
ignoreboth

#> export | grep HISTCONTROL
HISTCONTROL=ignoreboth
```

## Comparison with Bash

Hash shell's HISTCONTROL is fully compatible with bash:

| Option | Hash | Bash |
|--------|------|------|
| `ignorespace` | ✅ | ✅ |
| `ignoredups` | ✅ | ✅ |
| `ignoreboth` | ✅ | ✅ |
| `erasedups` | ✅ | ✅ |

Back to [README](../README.md)
