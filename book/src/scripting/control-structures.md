# Control Structures

Hash shell supports POSIX-compatible control structures for flow control.

## If/Then/Else

### Syntax

```bash
if command; then
    commands
fi

if command; then
    commands
else
    commands
fi

if command; then
    commands
elif command; then
    commands
else
    commands
fi
```

### Examples

```bash
# Simple if
if [ -f "/etc/passwd" ]; then
    echo "passwd file exists"
fi

# If/else
if [ "$USER" = "root" ]; then
    echo "Running as root"
else
    echo "Running as $USER"
fi

# If/elif/else
if [ $count -lt 0 ]; then
    echo "Negative"
elif [ $count -eq 0 ]; then
    echo "Zero"
else
    echo "Positive"
fi
```

### Any Command as Condition

```bash
if grep -q "pattern" file.txt; then
    echo "Found"
fi

if command -v git > /dev/null; then
    echo "Git installed"
fi
```

## For Loops

### Syntax

```bash
for var in list; do
    commands
done
```

### Examples

```bash
# List of words
for name in Alice Bob Charlie; do
    echo "Hello, $name"
done

# Files
for file in *.txt; do
    echo "Processing: $file"
done

# Arguments
for arg in "$@"; do
    echo "Argument: $arg"
done

# Numbers
for i in $(seq 1 5); do
    echo "Number: $i"
done
```

## While Loops

### Syntax

```bash
while command; do
    commands
done
```

### Examples

```bash
count=0
while [ $count -lt 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

# Read lines
while read line; do
    echo "Line: $line"
done < file.txt

# Infinite loop
while true; do
    echo "Press Ctrl+C"
    sleep 1
done
```

## Until Loops

### Syntax

```bash
until command; do
    commands
done
```

### Example

```bash
count=0
until [ $count -ge 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done
```

## Case Statements

### Syntax

```bash
case word in
    pattern1)
        commands
        ;;
    pattern2|pattern3)
        commands
        ;;
    *)
        default
        ;;
esac
```

### Examples

```bash
case "$1" in
    start)
        echo "Starting..."
        ;;
    stop)
        echo "Stopping..."
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        ;;
esac

# Pattern matching
case "$filename" in
    *.txt)
        echo "Text file"
        ;;
    *.sh)
        echo "Shell script"
        ;;
    *)
        echo "Unknown"
        ;;
esac
```

## Nesting

```bash
for dir in /home/*; do
    if [ -d "$dir" ]; then
        for file in "$dir"/*.txt; do
            if [ -f "$file" ]; then
                echo "Found: $file"
            fi
        done
    fi
done
```

## Common Mistakes

### Missing Spaces

```bash
# Wrong
if [-f "$file"]; then

# Correct
if [ -f "$file" ]; then
```

### Unquoted Variables

```bash
# Wrong
for file in $files; do

# Correct
for file in "$files"; do
```

### = vs -eq

```bash
# String comparison
if [ "$str" = "value" ]; then

# Numeric comparison
if [ $num -eq 5 ]; then
```
