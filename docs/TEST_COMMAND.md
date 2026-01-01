# Test Command in Hash Shell

The `test` command (and its `[ ]` alias) evaluates conditional expressions and returns an exit code: 0 for true, 1 for false.

## Syntax

```bash
test expression
[ expression ]
```

**Note:** When using `[ ]`, spaces are required around the brackets.

## File Tests

Test attributes of files and directories.

| Operator | Description |
|----------|-------------|
| `-e file` | True if file exists |
| `-f file` | True if file exists and is a regular file |
| `-d file` | True if file exists and is a directory |
| `-r file` | True if file exists and is readable |
| `-w file` | True if file exists and is writable |
| `-x file` | True if file exists and is executable |
| `-s file` | True if file exists and has size > 0 |
| `-L file` | True if file is a symbolic link |
| `-h file` | Same as `-L` |
| `-b file` | True if file is a block device |
| `-c file` | True if file is a character device |
| `-p file` | True if file is a named pipe (FIFO) |
| `-S file` | True if file is a socket |
| `-u file` | True if file has setuid bit set |
| `-g file` | True if file has setgid bit set |
| `-k file` | True if file has sticky bit set |
| `-O file` | True if file is owned by effective user ID |
| `-G file` | True if file is owned by effective group ID |

### Examples

```bash
# Check if file exists
if [ -e "/etc/passwd" ]; then
    echo "File exists"
fi

# Check if directory
if [ -d "/tmp" ]; then
    echo "/tmp is a directory"
fi

# Check if file is readable and writable
if [ -r "$file" -a -w "$file" ]; then
    echo "File is readable and writable"
fi

# Check if script is executable
if [ -x "$0" ]; then
    echo "This script is executable"
fi
```

## File Comparison

Compare files by modification time or identity.

| Operator | Description |
|----------|-------------|
| `file1 -nt file2` | True if file1 is newer than file2 |
| `file1 -ot file2` | True if file1 is older than file2 |
| `file1 -ef file2` | True if file1 and file2 are the same file |

### Examples

```bash
# Check if config was modified after backup
if [ config.txt -nt config.txt.bak ]; then
    echo "Config has been modified since backup"
fi

# Check if files are hard links to same inode
if [ link1 -ef link2 ]; then
    echo "Same file (hard links)"
fi
```

## String Tests

Test string values and comparisons.

| Operator | Description |
|----------|-------------|
| `-z string` | True if string is empty (zero length) |
| `-n string` | True if string is not empty |
| `string` | True if string is not empty |
| `s1 = s2` | True if strings are equal |
| `s1 == s2` | True if strings are equal (same as `=`) |
| `s1 != s2` | True if strings are not equal |

### Examples

```bash
# Check if variable is empty
if [ -z "$VAR" ]; then
    echo "VAR is empty or unset"
fi

# Check if variable has value
if [ -n "$VAR" ]; then
    echo "VAR has a value: $VAR"
fi

# String comparison
if [ "$USER" = "root" ]; then
    echo "Running as root"
fi

# Not equal
if [ "$MODE" != "debug" ]; then
    echo "Not in debug mode"
fi
```

**Important:** Always quote string variables to handle empty values and spaces:

```bash
# Wrong - fails if VAR is empty
[ $VAR = "value" ]

# Correct - handles empty VAR
[ "$VAR" = "value" ]
```

## Integer Comparisons

Compare numeric values.

| Operator | Description |
|----------|-------------|
| `n1 -eq n2` | True if n1 equals n2 |
| `n1 -ne n2` | True if n1 not equals n2 |
| `n1 -lt n2` | True if n1 < n2 |
| `n1 -le n2` | True if n1 <= n2 |
| `n1 -gt n2` | True if n1 > n2 |
| `n1 -ge n2` | True if n1 >= n2 |

### Examples

```bash
# Check if count is greater than 10
if [ $count -gt 10 ]; then
    echo "Count exceeds 10"
fi

# Check if values are equal
if [ $a -eq $b ]; then
    echo "Values are equal"
fi

# Range check
if [ $num -ge 1 -a $num -le 100 ]; then
    echo "Number is between 1 and 100"
fi
```

## Logical Operators

Combine multiple conditions.

| Operator | Description |
|----------|-------------|
| `! expr` | Logical NOT - true if expr is false |
| `expr1 -a expr2` | Logical AND - true if both are true |
| `expr1 -o expr2` | Logical OR - true if either is true |
| `( expr )` | Grouping (requires escaping or quoting) |

### Examples

```bash
# NOT
if [ ! -f "$file" ]; then
    echo "File does not exist"
fi

# AND
if [ -f "$file" -a -r "$file" ]; then
    echo "File exists and is readable"
fi

# OR
if [ "$USER" = "root" -o "$USER" = "admin" ]; then
    echo "Administrative user"
fi

# Combined
if [ -d "$dir" -a \( -r "$dir" -o -w "$dir" \) ]; then
    echo "Directory is accessible"
fi
```

## Terminal Test

Test if a file descriptor is a terminal.

| Operator | Description |
|----------|-------------|
| `-t fd` | True if file descriptor fd is a terminal |

### Example

```bash
# Check if stdout is a terminal
if [ -t 1 ]; then
    echo "Running interactively"
else
    echo "Output is redirected"
fi
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Expression is true |
| 1 | Expression is false |
| 2 | Invalid expression (syntax error) |

### Using Exit Codes

```bash
# Direct use in if
if [ -f "$file" ]; then
    echo "File exists"
fi

# Check exit code
test -f "$file"
if [ $? -eq 0 ]; then
    echo "File exists"
fi

# Shorthand with && and ||
[ -f "$file" ] && echo "File exists"
[ -f "$file" ] || echo "File not found"
```

## Common Patterns

### Checking Arguments

```bash
# Check for required argument
if [ -z "$1" ]; then
    echo "Usage: $0 <filename>" >&2
    exit 1
fi
```

### File Existence Check

```bash
# Create file only if it doesn't exist
if [ ! -f "$config_file" ]; then
    touch "$config_file"
fi
```

### Permission Check

```bash
# Ensure file is executable
if [ ! -x "$script" ]; then
    chmod +x "$script"
fi
```

### Environment Check

```bash
# Check for required environment variable
if [ -z "${API_KEY:-}" ]; then
    echo "Error: API_KEY not set" >&2
    exit 1
fi
```

### Numeric Validation

```bash
# Simple integer check (POSIX-compatible)
case "$input" in
    *[!0-9]*|'')
        echo "Not a valid number"
        exit 1
        ;;
esac
```

## Extended Test: `[[ ]]`

Hash also supports the bash-style extended test `[[ ]]`, which has additional features.

### Syntax

```bash
[[ expression ]]
```

### Differences from `[ ]`

| Feature | `[ ]` (test) | `[[ ]]` (extended) |
|---------|--------------|--------------------|
| Word splitting | Yes | No |
| Pattern matching | No | Yes (`==`, `!=`) |
| Regex | No | Yes (`=~`) |
| String comparison | `=`, `!=` | `<`, `>` also |
| `&&` / `\|\|` inside | No (use `-a`/`-o`) | Yes |
| `-v VAR` | No | Yes |
| POSIX compatible | Yes | No |

### Pattern Matching

In `[[ ]]`, the `==` and `!=` operators do pattern matching (glob-style):

```bash
# Match files ending in .txt
[[ "$file" == *.txt ]] && echo "Text file"

# Match anything starting with "test"
[[ "$name" == test* ]] && echo "Test item"

# Using character classes
[[ "$char" == [a-z] ]] && echo "Lowercase letter"
```

### Regex Matching

The `=~` operator matches against extended regular expressions:

```bash
# Match email pattern
[[ "$email" =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]] && echo "Valid email"

# Match digits
[[ "$input" =~ ^[0-9]+$ ]] && echo "All digits"

# Match version number
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] && echo "Valid semver"
```

### String Comparison

`[[ ]]` supports lexicographic string comparison:

```bash
# String less than
[[ "apple" < "banana" ]] && echo "apple comes before banana"

# String greater than  
[[ "zebra" > "apple" ]] && echo "zebra comes after apple"
```

### Logical Operators

`[[ ]]` uses `&&` and `||` instead of `-a` and `-o`:

```bash
# AND - both conditions must be true
[[ -f "$file" && -r "$file" ]] && echo "File exists and is readable"

# OR - either condition can be true
[[ -z "$VAR" || "$VAR" == "default" ]] && echo "Using default"

# Combined with NOT
[[ ! -d "$dir" || ! -w "$dir" ]] && echo "Directory not writable"
```

### Variable Set Test

`[[ -v VAR ]]` tests if a variable is set (even if empty):

```bash
# Check if variable is defined
[[ -v HOME ]] && echo "HOME is set"

# Note: This is different from -n which checks non-empty
unset MYVAR
[[ -v MYVAR ]] && echo "set" || echo "not set"  # Prints: not set

MYVAR=""
[[ -v MYVAR ]] && echo "set" || echo "not set"  # Prints: set
```

### Examples

```bash
# Pattern matching for file extensions
process_file() {
    local file="$1"
    
    if [[ "$file" == *.txt ]]; then
        echo "Processing text file"
    elif [[ "$file" == *.{jpg,png,gif} ]]; then
        echo "Processing image"
    else
        echo "Unknown file type"
    fi
}

# Input validation with regex
validate_input() {
    local input="$1"
    
    if [[ ! "$input" =~ ^[a-zA-Z][a-zA-Z0-9_]*$ ]]; then
        echo "Invalid identifier: must start with letter"
        return 1
    fi
    return 0
}

# Combined conditions
if [[ -f "$config" && -r "$config" && $(stat -c%s "$config") -gt 0 ]]; then
    source "$config"
fi
```

### When to Use Each

**Use `[ ]` when:**
- Writing POSIX-compatible scripts
- Simple file and string tests
- Maximum portability is required

**Use `[[ ]]` when:**
- Pattern matching is needed
- Regex matching is needed
- Cleaner syntax for complex conditions
- No need for POSIX compatibility

## Troubleshooting

### Common Errors

**Missing spaces:**
```bash
# Wrong
if [-f "$file"]; then

# Correct
if [ -f "$file" ]; then
```

**Unquoted variable:**
```bash
# Wrong - fails if VAR is empty
[ $VAR = "value" ]

# Correct
[ "$VAR" = "value" ]
```

**Missing closing bracket:**
```bash
# Wrong
if [ -f "$file" ; then

# Correct
if [ -f "$file" ]; then
```

**Using = for numbers:**
```bash
# Wrong - string comparison
[ $count = 10 ]

# Correct - numeric comparison
[ $count -eq 10 ]
```

## See Also

- [Shell Scripting](SCRIPTING.md)
- [Control Structures](CONTROL_STRUCTURES.md)
- [Command Chaining](COMMAND_CHAINING.md)

Back to [README](../README.md)
