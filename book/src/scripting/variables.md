# Variables & Expansion

Hash shell supports environment variable expansion in commands and arguments.

## Basic Syntax

### $VAR - Simple Expansion

```bash
#> export NAME=Julio
#> echo Hello $NAME
Hello Julio
```

### ${VAR} - Braced Expansion

Use braces when concatenating:

```bash
#> export PRE=test
#> echo ${PRE}ing
testing

#> echo $PREing
# Tries to expand $PREing (undefined)
```

## Special Variables

### $? - Exit Code

```bash
#> true
#> echo $?
0

#> false
#> echo $?
1
```

### $$ - Process ID

```bash
#> echo $$
12345
```

### $0 - Shell Name

```bash
#> echo $0
hash
```

## Variable Name Rules

Valid names:
- Start with letter or underscore
- Contain letters, numbers, underscores
- Case-sensitive

```bash
$HOME       # Valid
$MY_VAR     # Valid
$_private   # Valid
$123        # Invalid (starts with number)
$my-var     # Invalid (hyphen)
```

## Escaping

Prevent expansion with backslash:

```bash
#> echo \$HOME
$HOME

#> echo Price: \$5
Price: $5
```

## Quote Handling

### Double Quotes - Variables Expand

```bash
#> export NAME=Julio
#> echo "Hello $NAME"
Hello Julio
```

### Single Quotes - No Expansion

```bash
#> echo 'Hello $NAME'
Hello $NAME
```

## Common Patterns

### Building Paths

```bash
#> export PROJECT=myapp
#> export BUILD_DIR=/tmp/builds
#> mkdir -p ${BUILD_DIR}/${PROJECT}
```

### PATH Manipulation

```bash
#> export PATH=$HOME/bin:$PATH
```

### Configuration

```bash
# In .hashrc
export EDITOR=vim
export PROJECT_DIR=~/projects

alias edit='$EDITOR'
alias proj='cd $PROJECT_DIR'
```

## Undefined Variables

Expand to empty string:

```bash
#> unset UNDEFINED
#> echo "Value: $UNDEFINED"
Value:
```

## Limitations

Current limitations (may be added):

- `${VAR:-default}` - Default value
- `${VAR:=value}` - Assign default
- `${#VAR}` - Length
- `${VAR%pattern}` - Remove suffix
- `${VAR#pattern}` - Remove prefix
