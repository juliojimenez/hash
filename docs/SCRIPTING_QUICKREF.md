# Shell Scripting Quick Reference

## Running Scripts

```bash
# With shebang
#!/usr/local/bin/hash-shell
chmod +x script.sh && ./script.sh

# Direct execution
hash-shell script.sh arg1 arg2

# Command string
hash-shell -c 'echo "Hello"'

# From stdin
echo 'echo "Hello"' | hash-shell -s
```

## Variables

| Variable | Description |
|----------|-------------|
| `$0` | Script name |
| `$1`-`$9` | Positional arguments |
| `$#` | Argument count |
| `$@` | All arguments |
| `$?` | Last exit code |
| `$$` | Process ID |

```bash
NAME="value"          # Assignment
export NAME="value"   # Export
echo $NAME           # Use
```

## If/Then/Else

```bash
if [ condition ]; then
    commands
elif [ condition ]; then
    commands
else
    commands
fi
```

## For Loop

```bash
for var in list; do
    commands
done

# Examples
for i in 1 2 3; do echo $i; done
for f in *.txt; do cat "$f"; done
```

## While Loop

```bash
while [ condition ]; do
    commands
done
```

## Until Loop

```bash
until [ condition ]; do
    commands
done
```

## Case Statement

```bash
case "$var" in
    pattern1)
        commands ;;
    pattern2|pattern3)
        commands ;;
    *)
        default ;;
esac
```

## Test Conditions

### Files
| Test | True if |
|------|---------|
| `-e file` | Exists |
| `-f file` | Regular file |
| `-d file` | Directory |
| `-r file` | Readable |
| `-w file` | Writable |
| `-x file` | Executable |
| `-s file` | Size > 0 |

### Strings
| Test | True if |
|------|---------|
| `-z str` | Empty |
| `-n str` | Not empty |
| `s1 = s2` | Equal |
| `s1 != s2` | Not equal |

### Numbers
| Test | True if |
|------|---------|
| `n1 -eq n2` | Equal |
| `n1 -ne n2` | Not equal |
| `n1 -lt n2` | Less than |
| `n1 -gt n2` | Greater than |
| `n1 -le n2` | ≤ |
| `n1 -ge n2` | ≥ |

### Logical
| Operator | Description |
|----------|-------------|
| `! expr` | NOT |
| `e1 -a e2` | AND |
| `e1 -o e2` | OR |

## Functions

```bash
funcname() {
    echo "Arg 1: $1"
    return 0
}

funcname "hello"
```

## Exit Codes

```bash
exit 0    # Success
exit 1    # Error
```

## Common Patterns

```bash
# Check if file exists
[ -f "$file" ] && echo "exists"

# Check for required argument
[ -z "$1" ] && echo "Usage: $0 file" && exit 1

# Default value
name="${1:-default}"

# Loop over arguments
for arg in "$@"; do
    process "$arg"
done
```

Back to [README](../README.md)
