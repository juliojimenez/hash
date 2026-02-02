# Shell Scripting

Hash shell supports POSIX-compatible shell scripting for automation and reusable scripts.

## Running Scripts

### Shebang Line

Create a script with the hash shebang:

```bash
#!/usr/local/bin/hash-shell

echo "Hello from hash!"
```

Make executable and run:

```bash
chmod +x script.sh
./script.sh
```

### Direct Execution

```bash
hash-shell script.sh
```

### Command String

```bash
hash-shell -c 'echo "Hello"; echo "World"'
```

### From Standard Input

```bash
echo 'echo "Hello"' | hash-shell -s
```

## Script Arguments

### Positional Parameters

```bash
#!/usr/local/bin/hash-shell

echo "Script name: $0"
echo "First arg: $1"
echo "Second arg: $2"
echo "All args: $@"
echo "Arg count: $#"
```

Run:
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
| `$1`-`$9` | Positional arguments |
| `$#` | Number of arguments |
| `$@` | All arguments (separate words) |
| `$*` | All arguments (single word) |
| `$?` | Exit code of last command |
| `$$` | Process ID of shell |

## Functions

```bash
greet() {
    echo "Hello, $1!"
}

greet "World"
```

With return value:

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
echo $NAME          # Simple
echo ${NAME}        # Braced
echo "${NAME}"      # Quoted (preserves whitespace)
```

## Exit Codes

```bash
#!/usr/local/bin/hash-shell

if [ ! -f "$1" ]; then
    echo "Error: File not found" >&2
    exit 1
fi

# Process file...
exit 0
```

Common codes:
- `0` - Success
- `1` - General error
- `2` - Misuse of command

## Comments

```bash
# This is a comment
echo "Hello"  # Inline comment
```

## Best Practices

1. **Always quote variables**: `"$var"` prevents word splitting
2. **Use meaningful names**: `FILE_COUNT` not `fc`
3. **Check for errors**: Test return codes
4. **Add comments**: Explain complex logic
5. **Use exit codes**: Return 0 for success
6. **Validate input**: Check arguments first

## Practical Example

```bash
#!/usr/local/bin/hash-shell

# Backup script
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

## See Also

- [Variables & Expansion](variables.md)
- [Control Structures](control-structures.md)
- [Test Command](test-command.md)
