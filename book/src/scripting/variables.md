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

Every special parameter below expands the same way with or without braces and
inside double quotes: `"$$"`, `${$}`, `${?}`, `${!}`, `${#}`, `${-}`, `${@}`
and `${*}` all work, and the braced forms take the usual modifiers
(`${?:-0}`, `${#?}`). `$$` is the pid of the shell itself, also inside
pipelines, subshells, `$( )` and background jobs.

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

### $LINENO - Current Line

The line of the current command: within the script, the `-c` string or the
`eval` text being run, and from the definition line inside a function body.
Syntax errors report the same line.

```bash
#> echo $LINENO
1
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

## Parameter Expansion Forms

The POSIX forms are all available:

| Form | Result |
|------|--------|
| `${VAR:-word}` | `$VAR`, or `word` when unset or empty (`${VAR-word}`: only when unset) |
| `${VAR:=word}` | Assign `word` when unset or empty, then expand |
| `${VAR:+word}` | `word` when set and non-empty, otherwise nothing |
| `${VAR:?word}` | `$VAR`, or print `word` and fail when unset or empty |
| `${#VAR}` | Length of the value |
| `${VAR#pattern}`, `${VAR##pattern}` | Remove the shortest, longest matching prefix |
| `${VAR%pattern}`, `${VAR%%pattern}` | Remove the shortest, longest matching suffix |

The pattern word of `${VAR#pattern}`, `${VAR##pattern}`, `${VAR%pattern}` and
`${VAR%%pattern}` is expanded before it is used: `${file#*"$prefix"}` removes a
literal prefix held in a variable, `${t##$glob}` uses the variable's value as a
pattern, and anything quoted in the word matches itself. `#` and `%` remove the
shortest match, `##` and `%%` the longest, and the shortest match may be empty
(`${t#*}` is `$t`).
