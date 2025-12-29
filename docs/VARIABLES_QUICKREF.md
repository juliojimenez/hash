# Variable Expansion Quick Reference

## Syntax

| Syntax | Description | Example |
|--------|-------------|---------|
| `$VAR` | Simple expansion | `echo $HOME` |
| `${VAR}` | Braced expansion | `echo ${HOME}` |
| `\$` | Escaped dollar | `echo \$5` → `$5` |

## Special Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `$?` | Exit code of last command | `echo $?` → `0` or `1` |
| `$$` | Process ID | `echo $$` → `12345` |
| `$0` | Shell name | `echo $0` → `hash` |

## Common Patterns

### Set and Use
```bash
export NAME=value
echo $NAME
```

### PATH Manipulation
```bash
export PATH=$HOME/bin:$PATH
```

### Concatenation (Use Braces!)
```bash
export PRE=test
echo ${PRE}ing  # → testing
echo $PREing    # → (empty, looks for $PREing)
```

### With Tilde
```bash
export DIR=projects
cd ~/$DIR  # → /home/user/projects
```

### In Aliases
```bash
alias showpath='echo $PATH'
alias proj='cd $PROJECT_DIR'
```

## Quote Behavior

| Context | Expansion | Example |
|---------|-----------|---------|
| No quotes | ✅ Yes | `echo $HOME` |
| Double quotes | ✅ Yes | `echo "$HOME"` |
| Single quotes | ❌ No | `echo '$HOME'` → `$HOME` |

## Examples

```bash
# Check variables
#> echo $HOME
/home/user

#> echo $USER  
julio

#> echo $?
0

# Use in commands
#> export FILE=test.txt
#> cat $FILE

# Build paths
#> export PROJ=~/work
#> cd $PROJ

# Multiple variables
#> export A=hello B=world
#> echo $A $B
hello world
```

## Tips

1. **Use braces for concatenation**: `${VAR}text` not `$VARtext`
2. **Quote if spaces possible**: `"$PATH"` safer than `$PATH`
3. **Check before use**: `echo $VAR` to verify value
4. **Escape literal $**: `\$5` for literal dollar sign

Back to [README](../README.md)
