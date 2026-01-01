# Shell Scripting in Hash

Hash shell supports POSIX-compatible shell scripting, allowing you to automate tasks and create reusable scripts.

## Running Scripts

### Shebang Line

Create a script file with the hash shebang:

```bash
#!/usr/local/bin/hash-shell

echo "Hello from hash!"
```

Make it executable and run:

```bash
chmod +x script.sh
./script.sh
```

### Direct Execution

Run a script without shebang:

```bash
hash-shell script.sh
```

### Command String

Execute commands directly:

```bash
hash-shell -c 'echo "Hello"; echo "World"'
```

### From Standard Input

Pipe commands to hash:

```bash
echo 'echo "Hello from pipe"' | hash-shell -s
```

## Script Arguments

### Positional Parameters

Access script arguments with `$1`, `$2`, etc.:

```bash
#!/usr/local/bin/hash-shell

echo "Script name: $0"
echo "First arg: $1"
echo "Second arg: $2"
echo "All args: $@"
echo "Arg count: $#"
```

Run with arguments:

```bash
./script.sh hello world
# Output:
# Script name: ./script.sh
# First arg: hello
# Second arg: world
# All args: hello world
# Arg count: 2
```

### Special Variables

| Variable | Description |
|----------|-------------|
| `$0` | Script name |
| `$1` - `$9` | Positional arguments 1-9 |
| `$#` | Number of arguments |
| `$@` | All arguments (as separate words) |
| `$*` | All arguments (as single word) |
| `$?` | Exit code of last command |
| `$$` | Process ID of shell |

## Control Structures

### If/Then/Else

```bash
if [ condition ]; then
    commands
elif [ condition ]; then
    commands
else
    commands
fi
```

**Example:**

```bash
if [ -f "/etc/passwd" ]; then
    echo "passwd file exists"
elif [ -f "/etc/shadow" ]; then
    echo "shadow file exists"
else
    echo "neither file found"
fi
```

### For Loops

```bash
for var in list; do
    commands
done
```

**Examples:**

```bash
# Iterate over words
for name in Alice Bob Charlie; do
    echo "Hello, $name"
done

# Iterate over files
for file in *.txt; do
    echo "Processing $file"
done

# Iterate over arguments
for arg in "$@"; do
    echo "Argument: $arg"
done
```

### While Loops

```bash
while [ condition ]; do
    commands
done
```

**Example:**

```bash
count=0
while [ $count -lt 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done
```

### Until Loops

```bash
until [ condition ]; do
    commands
done
```

**Example:**

```bash
count=0
until [ $count -ge 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done
```

## Test Command

The `test` command (or `[ ]`) evaluates expressions:

### File Tests

| Test | Description |
|------|-------------|
| `-e file` | File exists |
| `-f file` | Is a regular file |
| `-d file` | Is a directory |
| `-r file` | Is readable |
| `-w file` | Is writable |
| `-x file` | Is executable |
| `-s file` | Has size > 0 |
| `-L file` | Is a symbolic link |

### String Tests

| Test | Description |
|------|-------------|
| `-z string` | String is empty |
| `-n string` | String is not empty |
| `s1 = s2` | Strings are equal |
| `s1 != s2` | Strings are not equal |

### Numeric Tests

| Test | Description |
|------|-------------|
| `n1 -eq n2` | Equal |
| `n1 -ne n2` | Not equal |
| `n1 -lt n2` | Less than |
| `n1 -le n2` | Less than or equal |
| `n1 -gt n2` | Greater than |
| `n1 -ge n2` | Greater than or equal |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `! expr` | NOT |
| `expr -a expr` | AND |
| `expr -o expr` | OR |

**Examples:**

```bash
# File tests
if [ -f "/etc/passwd" ]; then
    echo "File exists"
fi

# String comparison
if [ "$USER" = "root" ]; then
    echo "Running as root"
fi

# Numeric comparison
if [ $count -gt 10 ]; then
    echo "Count is greater than 10"
fi

# Combined conditions
if [ -f "$file" -a -r "$file" ]; then
    echo "File exists and is readable"
fi
```

## Functions

Define reusable functions:

```bash
# Function definition
greet() {
    echo "Hello, $1!"
}

# Function call
greet "World"
```

**Example with return value:**

```bash
is_even() {
    if [ $(($1 % 2)) -eq 0 ]; then
        return 0  # true
    else
        return 1  # false
    fi
}

if is_even 4; then
    echo "4 is even"
fi
```

## Variables

### Assignment

```bash
NAME="value"        # No spaces around =
export NAME="value" # Export to environment
```

### Expansion

```bash
echo $NAME          # Simple expansion
echo ${NAME}        # Braced expansion
echo "${NAME}"      # Quoted expansion (preserves whitespace)
```

## Exit Codes

Scripts should return appropriate exit codes:

```bash
#!/usr/local/bin/hash-shell

if [ ! -f "$1" ]; then
    echo "Error: File not found" >&2
    exit 1
fi

# Process file...
exit 0
```

Common exit codes:
- `0` - Success
- `1` - General error
- `2` - Misuse of command

## Comments

```bash
# This is a comment

echo "Hello"  # Inline comment
```

## Practical Examples

### Backup Script

```bash
#!/usr/local/bin/hash-shell

BACKUP_DIR="/backup"
DATE=$(date +%Y%m%d)

if [ ! -d "$BACKUP_DIR" ]; then
    mkdir -p "$BACKUP_DIR"
fi

for dir in /home/*; do
    if [ -d "$dir" ]; then
        user=$(basename "$dir")
        tar -czf "$BACKUP_DIR/${user}_${DATE}.tar.gz" "$dir"
        echo "Backed up: $user"
    fi
done

echo "Backup complete!"
```

### System Check Script

```bash
#!/usr/local/bin/hash-shell

echo "System Check Report"
echo "==================="

# Check disk usage
echo ""
echo "Disk Usage:"
df -h /

# Check memory
echo ""
echo "Memory Usage:"
free -m

# Check if services are running
for service in sshd nginx mysql; do
    if pgrep -x "$service" > /dev/null; then
        echo "$service: running"
    else
        echo "$service: not running"
    fi
done
```

## Best Practices

1. **Always quote variables**: `"$var"` prevents word splitting
2. **Use meaningful variable names**: `FILE_COUNT` not `fc`
3. **Check for errors**: Test return codes and file existence
4. **Add comments**: Explain complex logic
5. **Use exit codes**: Return 0 for success, non-zero for errors
6. **Validate input**: Check arguments before using them

## See Also

- [Control Structures](CONTROL_STRUCTURES.md)
- [Test Command](TEST_COMMAND.md)
- [Variable Expansion](VARIABLES.md)
- [Command Chaining](COMMAND_CHAINING.md)

Back to [README](../README.md)
