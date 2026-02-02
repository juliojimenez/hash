# Test Command

The `test` command (or `[ ]`) evaluates expressions for use in conditionals.

## File Tests

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

### Examples

```bash
if [ -f "/etc/passwd" ]; then
    echo "File exists"
fi

if [ -d "$dir" ]; then
    echo "Is a directory"
fi

if [ -r "$file" ] && [ -w "$file" ]; then
    echo "Readable and writable"
fi
```

## String Tests

| Test | Description |
|------|-------------|
| `-z string` | String is empty |
| `-n string` | String is not empty |
| `s1 = s2` | Strings are equal |
| `s1 != s2` | Strings are not equal |

### Examples

```bash
if [ -z "$var" ]; then
    echo "Variable is empty"
fi

if [ "$USER" = "root" ]; then
    echo "Running as root"
fi

if [ "$a" != "$b" ]; then
    echo "Strings differ"
fi
```

## Numeric Tests

| Test | Description |
|------|-------------|
| `n1 -eq n2` | Equal |
| `n1 -ne n2` | Not equal |
| `n1 -lt n2` | Less than |
| `n1 -le n2` | Less than or equal |
| `n1 -gt n2` | Greater than |
| `n1 -ge n2` | Greater than or equal |

### Examples

```bash
if [ $count -eq 0 ]; then
    echo "Count is zero"
fi

if [ $x -lt 10 ]; then
    echo "x is less than 10"
fi

if [ $a -ge $b ]; then
    echo "a >= b"
fi
```

## Logical Operators

| Operator | Description |
|----------|-------------|
| `! expr` | NOT |
| `expr -a expr` | AND |
| `expr -o expr` | OR |

### Examples

```bash
# NOT
if [ ! -f "$file" ]; then
    echo "File not found"
fi

# AND
if [ -f "$file" -a -r "$file" ]; then
    echo "File exists and is readable"
fi

# OR
if [ -z "$a" -o -z "$b" ]; then
    echo "At least one is empty"
fi
```

## Combining with && and ||

```bash
# Preferred over -a and -o
if [ -f "$file" ] && [ -r "$file" ]; then
    echo "File exists and readable"
fi

if [ -z "$a" ] || [ -z "$b" ]; then
    echo "At least one is empty"
fi
```

## Common Patterns

### Check File Exists

```bash
if [ -f "$config" ]; then
    source "$config"
fi
```

### Check Directory

```bash
if [ ! -d "$dir" ]; then
    mkdir -p "$dir"
fi
```

### Validate Arguments

```bash
if [ $# -eq 0 ]; then
    echo "Usage: $0 file" >&2
    exit 1
fi
```

### Compare Strings

```bash
if [ "$answer" = "yes" ]; then
    proceed
fi
```

### Numeric Comparison

```bash
if [ $count -gt 100 ]; then
    echo "Too many items"
fi
```

## Important Notes

### Spaces Required

```bash
# Wrong
if [-f "$file"]; then

# Correct
if [ -f "$file" ]; then
```

### Quote Variables

```bash
# Wrong (breaks if file has spaces)
if [ -f $file ]; then

# Correct
if [ -f "$file" ]; then
```

### = vs ==

In POSIX shell, use `=` for string comparison:

```bash
# POSIX compliant
if [ "$a" = "$b" ]; then

# Also works but less portable
if [ "$a" == "$b" ]; then
```
