# Variable Expansion in Hash Shell

Hash shell supports environment variable expansion, allowing you to use variables in commands and arguments.

## Basic Syntax

### $VAR - Simple Expansion

```bash
#> export NAME=Julio
#> echo Hello $NAME
Hello Julio

#> export PATH=/usr/local/bin:/usr/bin
#> echo $PATH
/usr/local/bin:/usr/bin
```

### ${VAR} - Braced Expansion

Use braces when concatenating with other text:

```bash
#> export PRE=test
#> echo ${PRE}ing
testing

#> export DIR=projects
#> cd ~/${DIR}/myapp
# Goes to ~/projects/myapp
```

Without braces, the variable name extends to the first non-alphanumeric:

```bash
#> export PRE=test
#> echo $PREing
# Tries to expand $PREing (undefined), prints nothing

#> echo ${PRE}ing
testing
```

## Special Variables

### $? - Exit Code

The exit code of the last executed command:

```bash
#> true
#> echo $?
0

#> false  
#> echo $?
1

#> ls /nonexistent
ls: cannot access '/nonexistent': No such file or directory
#> echo $?
2
```

### $$ - Process ID

The process ID of the current shell:

```bash
#> echo $$
12345

#> echo "My PID: $$"
My PID: 12345
```

### $0 - Shell Name

The name of the shell:

```bash
#> echo $0
hash

#> echo "Running $0 shell"
Running hash shell
```

## Variable Name Rules

Valid variable names:
- Start with letter or underscore
- Contain letters, numbers, underscores
- Case-sensitive

**Valid:**
```bash
$HOME
$USER
$MY_VAR
$_private
$PATH2
```

**Invalid:**
```bash
$123      # Can't start with number
$my-var   # No hyphens
$my.var   # No dots
```

## Escaping

### Escaped Dollar Sign

Use backslash to prevent expansion:

```bash
#> echo \$HOME
$HOME

#> echo Price: \$5
Price: $5
```

### Literal Dollar

Dollar signs followed by invalid characters stay literal:

```bash
#> echo test$
test$

#> echo $$$ 
# (PID)(PID)$

#> echo $@#!
$@#!
```

## Expansion in Different Contexts

### In Commands

```bash
#> export EDITOR=vim
#> $EDITOR file.txt
# Opens file.txt in vim
```

### In Arguments

```bash
#> export DIR=/tmp
#> cd $DIR
#> pwd
/tmp
```

### With Other Expansions

Variables expand **after** tilde but in the same pass:

```bash
#> export SUBDIR=Documents
#> cd ~/$SUBDIR
# Goes to /home/user/Documents

#> export FILE=test.txt
#> cat ~/$FILE
# Opens ~/test.txt
```

### In Aliases

Variables in aliases expand when the alias is **used**, not when defined:

```bash
#> alias showuser='echo Current user: $USER'
#> export USER=julio
#> showuser
Current user: julio

#> export USER=admin
#> showuser
Current user: admin
```

## Quote Handling

### Double Quotes - Variables Expand

```bash
#> export NAME=Julio
#> echo "Hello $NAME"
Hello Julio

#> echo "Path: $PATH"
Path: /usr/bin:/bin
```

### Single Quotes - No Expansion

```bash
#> export NAME=Julio
#> echo 'Hello $NAME'
Hello $NAME

#> echo '$USER is $USER'
$USER is $USER
```

This matches bash behavior!

## Common Patterns

### Building Paths

```bash
#> export PROJECT=myapp
#> export BUILD_DIR=/tmp/builds
#> mkdir -p ${BUILD_DIR}/${PROJECT}
#> cd ${BUILD_DIR}/${PROJECT}
```

### Using in Commands

```bash
#> export LOG_FILE=/var/log/app.log
#> tail -f $LOG_FILE

#> export BACKUP_DIR=~/backups
#> cp important.txt $BACKUP_DIR/
```

### Checking Variables

```bash
#> echo "Home: $HOME"
Home: /home/user

#> echo "User: $USER"
User: julio

#> echo "Shell: $SHELL"
Shell: /bin/bash
```

### Default Values (Not Yet Supported)

```bash
# These don't work yet:
# ${VAR:-default}
# ${VAR:=default}  
# ${VAR:?error}
```

## Examples

### Configuration

```bash
# In .hashrc
export EDITOR=vim
export PAGER=less
export PROJECT_DIR=~/projects

# Then use them
alias edit='$EDITOR'
alias proj='cd $PROJECT_DIR'
```

### Dynamic Commands

```bash
#> export CMD=ls
#> export ARGS="-la"
#> $CMD $ARGS
# Runs: ls -la
```

### Exit Code Checking

```bash
#> make
# ... build output ...
#> echo "Build exit code: $?"
Build exit code: 0
```

### Process Tracking

```bash
#> echo $$ > /tmp/shell.pid
#> cat /tmp/shell.pid
12345
```

## Undefined Variables

Undefined variables expand to empty string (like bash):

```bash
#> unset UNDEFINED
#> echo "Value: $UNDEFINED"
Value: 

#> echo before${UNDEFINED}after
beforeafter
```

## Common Use Cases

### PATH Manipulation

```bash
#> export PATH=$HOME/bin:$PATH
# Now $HOME/bin is first in PATH
```

### Temporary Variables

```bash
#> export TEMP=/tmp/mysession
#> mkdir -p $TEMP
#> cd $TEMP
#> # Do work
#> rm -rf $TEMP
```

### Configuration Files

```bash
#> export CONFIG_FILE=~/.myapp/config.ini
#> cat $CONFIG_FILE
#> $EDITOR $CONFIG_FILE
```

### Build Systems

```bash
#> export CC=gcc
#> export CFLAGS="-Wall -O2"
#> $CC $CFLAGS -o program program.c
```

## Troubleshooting

### Variable Not Expanding

Check if it's set:
```bash
#> echo $MY_VAR
# If empty, variable isn't set

#> env | grep MY_VAR
# Verify it's in environment
```

### Wrong Value

```bash
#> export MY_VAR=wrong
#> echo $MY_VAR
wrong

#> export MY_VAR=correct
#> echo $MY_VAR
correct
```

### Expansion in Single Quotes

Single quotes prevent expansion:
```bash
#> echo '$HOME'
$HOME

# Use double quotes:
#> echo "$HOME"
/home/user
```

## Performance

Variable expansion is:
- ✅ Fast - single pass through string
- ✅ Bounded - limited to 8KB expanded result
- ✅ Safe - proper bounds checking
- ✅ Memory-managed - expanded strings tracked

## Limitations

Current limitations (may be added later):
- ❌ Parameter expansion: `${VAR:-default}`, `${VAR:=value}`
- ❌ String manipulation: `${VAR#pattern}`, `${VAR%pattern}`
- ❌ Length: `${#VAR}`
- ❌ Substring: `${VAR:offset:length}`
- ❌ Case modification: `${VAR^^}`, `${VAR,,}`

Back to [README](../README.md)
