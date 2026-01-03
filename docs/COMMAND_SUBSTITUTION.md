# Command Substitution in Hash Shell

Hash shell supports command substitution, allowing you to use the output of a command as part of another command.

## Basic Syntax

### $(...) - Preferred Syntax

```bash
#> echo $(pwd)
/home/user/projects

#> echo "Current directory: $(pwd)"
Current directory: /home/user/projects

#> cd $(dirname /path/to/file.txt)
# Changes to /path/to
```

### `...` - Backtick Syntax

```bash
#> echo `pwd`
/home/user/projects

#> echo "User: `whoami`"
User: julio
```

**Note:** The `$(...)` syntax is preferred because:
- Easier to read
- Easier to nest
- No escaping issues with backslashes

## How It Works

1. Hash finds `$(command)` or `` `command` `` in the input
2. Executes the command in a subshell
3. Captures the standard output
4. Replaces the substitution with the captured output
5. Trailing newlines are removed (like bash)

## Examples

### Basic Usage

```bash
#> echo $(whoami)
julio

#> echo $(date)
Wed Dec 31 10:30:00 EST 2025

#> echo "Files: $(ls | wc -l)"
Files: 42
```

### Using Output in Commands

```bash
#> cd $(pwd)/subdir
# Changes to subdir relative to current directory

#> cat $(which python3)
# Displays the python3 script/binary

#> vim $(find . -name "*.c" | head -1)
# Opens the first .c file found
```

### Assigning to Variables

```bash
#> export CURRENT_DIR=$(pwd)
#> echo $CURRENT_DIR
/home/user/projects

#> export FILE_COUNT=$(ls | wc -l)
#> echo "There are $FILE_COUNT files"
There are 42 files
```

### In Conditional Chains

```bash
#> test -d $(pwd)/build && echo "Build dir exists"
Build dir exists

#> [ $(whoami) = "root" ] && echo "Running as root" || echo "Not root"
Not root
```

### Nested Substitution

```bash
#> echo $(echo $(echo nested))
nested

#> echo $(cat $(which hash-shell) | wc -l)
# Counts lines in the hash-shell binary
```

## Output Handling

### Trailing Newlines Removed

Command output typically ends with a newline. Hash removes trailing newlines:

```bash
#> echo "Path: $(pwd)!"
Path: /home/user!
# Note: no extra newline before the !
```

### Multiple Lines

Internal newlines are preserved:

```bash
#> echo "$(printf 'line1\nline2')"
line1
line2
```

### Empty Output

Commands with no output expand to nothing:

```bash
#> echo "Result: $(true)"
Result: 
```

## Escaping

### Prevent Substitution

Use backslash to escape:

```bash
#> echo \$(pwd)
$(pwd)

#> echo \`pwd\`
`pwd`
```

### In Single Quotes

Single quotes prevent all expansion (including command substitution):

```bash
#> echo '$(pwd)'
$(pwd)
```

### In Double Quotes

Double quotes allow command substitution:

```bash
#> echo "$(pwd)"
/home/user/projects

#> echo "Hello $(whoami)!"
Hello julio!
```

## Common Patterns

### Get Script Directory

```bash
#> SCRIPT_DIR=$(dirname $0)
#> cd $SCRIPT_DIR
```

### Count Items

```bash
#> FILE_COUNT=$(ls | wc -l)
#> echo "Found $FILE_COUNT files"
```

### Conditional Based on Command

```bash
#> if [ $(whoami) = "root" ]; then echo "root"; fi
```

### Store Command Output

```bash
#> export GIT_BRANCH=$(git branch --show-current)
#> echo "On branch: $GIT_BRANCH"
```

### Use in Loops

```bash
#> for file in $(ls *.txt); do echo $file; done
```

### Path Manipulation

```bash
#> BASE=$(basename /path/to/file.txt)
#> echo $BASE
file.txt

#> DIR=$(dirname /path/to/file.txt)
#> echo $DIR
/path/to
```

## Error Handling

### Command Not Found

If the command doesn't exist, substitution returns empty:

```bash
#> echo "Result: $(nonexistent_command 2>/dev/null)"
Result: 
```

### Command Fails

The substitution still captures whatever output was produced:

```bash
#> echo "$(ls /nonexistent 2>&1)"
ls: cannot access '/nonexistent': No such file or directory
```

### Exit Codes

The exit code of the substituted command is **not** preserved. The exit code reflects the outer command:

```bash
#> echo $(false)

#> echo $?
0  # Echo succeeded, even though false failed
```

## Performance Notes

- Each `$(...)` spawns a new shell process
- Multiple substitutions = multiple processes
- For performance-critical loops, consider alternatives
- The subshell is `/bin/sh`, not hash itself

## Limitations

Current limitations:
- ❌ No `$(<file)` for reading files (use `$(cat file)`)
- ❌ No arithmetic expansion `$((1+2))` (separate feature)
- ❌ Nested backticks are tricky (use `$(...)` instead)

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| `$(command)` | ✅ | ✅ |
| `` `command` `` | ✅ | ✅ |
| Nested `$($(cmd))` | ✅ | ✅ |
| Trailing newline removal | ✅ | ✅ |
| `$(<file)` | ❌ | ✅ |
| Word splitting of output | ⚠️ Basic | ✅ Full |

## Tips

1. **Prefer `$(...)`** over backticks for readability
2. **Quote substitutions** when used in arguments: `"$(cmd)"`
3. **Redirect stderr** if you don't want error messages: `$(cmd 2>/dev/null)`
4. **Cache results** in variables if used multiple times
5. **Test commands** before using in substitutions

---

Back to [README](../README.md)
