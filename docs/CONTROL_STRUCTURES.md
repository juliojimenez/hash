# Control Structures in Hash Shell

Hash shell supports POSIX-compatible control structures for flow control in scripts and interactive use.

## If/Then/Else

Conditional execution based on command exit codes.

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

### Basic Examples

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

### Condition is Any Command

The condition can be any command - `if` checks its exit code:

```bash
# Using grep
if grep -q "pattern" file.txt; then
    echo "Pattern found"
fi

# Using command
if command -v git > /dev/null; then
    echo "Git is installed"
fi

# Using ping
if ping -c 1 google.com > /dev/null 2>&1; then
    echo "Network is up"
fi
```

### One-Line Syntax

```bash
if [ -f "$file" ]; then echo "exists"; else echo "not found"; fi
```

### Negating Conditions

```bash
# Using !
if ! [ -f "$file" ]; then
    echo "File not found"
fi

# Or [ ! ... ]
if [ ! -f "$file" ]; then
    echo "File not found"
fi
```

## For Loops

Iterate over a list of values.

### Syntax

```bash
for var in list; do
    commands
done
```

### Iterating Over Values

```bash
# List of words
for name in Alice Bob Charlie; do
    echo "Hello, $name"
done

# Output:
# Hello, Alice
# Hello, Bob
# Hello, Charlie
```

### Iterating Over Files

```bash
# All .txt files
for file in *.txt; do
    echo "Processing: $file"
done

# Files in a directory
for item in /tmp/*; do
    if [ -f "$item" ]; then
        echo "File: $item"
    elif [ -d "$item" ]; then
        echo "Directory: $item"
    fi
done
```

### Iterating Over Arguments

```bash
#!/usr/local/bin/hash-shell

# Process all script arguments
for arg in "$@"; do
    echo "Processing argument: $arg"
done
```

### Iterating Over Command Output

```bash
# Process each line of output
for user in $(cat /etc/passwd | cut -d: -f1); do
    echo "User: $user"
done

# List of directories in PATH
for dir in $(echo $PATH | tr ':' ' '); do
    echo "Path directory: $dir"
done
```

### Number Sequences

```bash
# Using seq command
for i in $(seq 1 5); do
    echo "Number: $i"
done

# Output:
# Number: 1
# Number: 2
# Number: 3
# Number: 4
# Number: 5
```

### One-Line Syntax

```bash
for i in 1 2 3; do echo "i=$i"; done
```

## While Loops

Execute commands while a condition is true.

### Syntax

```bash
while command; do
    commands
done
```

### Basic Example

```bash
count=0
while [ $count -lt 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

# Output:
# Count: 0
# Count: 1
# Count: 2
# Count: 3
# Count: 4
```

### Reading Lines from File

```bash
while read line; do
    echo "Line: $line"
done < file.txt
```

### Infinite Loop

```bash
while true; do
    echo "Press Ctrl+C to stop"
    sleep 1
done
```

### Using Command as Condition

```bash
# Wait for file to appear
while [ ! -f "/tmp/ready" ]; do
    echo "Waiting for ready file..."
    sleep 1
done
echo "Ready file found!"
```

### One-Line Syntax

```bash
while [ $i -lt 5 ]; do echo $i; i=$((i+1)); done
```

## Until Loops

Execute commands until a condition becomes true (opposite of while).

### Syntax

```bash
until command; do
    commands
done
```

### Basic Example

```bash
count=0
until [ $count -ge 5 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

# Output: same as while example above
```

### Wait for Condition

```bash
# Wait until service is running
until pgrep -x "nginx" > /dev/null; do
    echo "Waiting for nginx to start..."
    sleep 2
done
echo "nginx is running!"
```

## Case Statements

Pattern matching for multiple conditions.

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
        default commands
        ;;
esac
```

### Basic Example

```bash
case "$1" in
    start)
        echo "Starting service..."
        ;;
    stop)
        echo "Stopping service..."
        ;;
    restart)
        echo "Restarting service..."
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
```

### Pattern Matching

```bash
case "$filename" in
    *.txt)
        echo "Text file"
        ;;
    *.sh)
        echo "Shell script"
        ;;
    *.tar.gz|*.tgz)
        echo "Compressed archive"
        ;;
    *)
        echo "Unknown file type"
        ;;
esac
```

### Character Classes

```bash
case "$input" in
    [0-9])
        echo "Single digit"
        ;;
    [0-9][0-9])
        echo "Two digits"
        ;;
    [a-zA-Z]*)
        echo "Starts with letter"
        ;;
    *)
        echo "Other"
        ;;
esac
```

### Yes/No Prompt

```bash
echo -n "Continue? [y/n] "
read answer
case "$answer" in
    [yY]|[yY][eE][sS])
        echo "Proceeding..."
        ;;
    [nN]|[nN][oO])
        echo "Aborting."
        exit 0
        ;;
    *)
        echo "Invalid response"
        exit 1
        ;;
esac
```

## Nesting Control Structures

Control structures can be nested inside each other:

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

### Nested If Example

```bash
if [ -f "$config" ]; then
    if [ -r "$config" ]; then
        echo "Config is readable"
        if grep -q "debug=true" "$config"; then
            echo "Debug mode enabled"
        fi
    else
        echo "Config exists but is not readable"
    fi
else
    echo "Config file not found"
fi
```

## Loop Control (Not Yet Implemented)

**Note:** `break` and `continue` are not yet implemented in hash shell.

When implemented:
- `break` - Exit the current loop
- `continue` - Skip to next iteration
- `break n` - Exit n levels of nested loops
- `continue n` - Skip n levels up

## Exit Codes in Control Structures

### If Statement Exit Code

The exit code of an `if` statement is:
- Exit code of last executed command if condition was true
- Exit code of last command in else block if condition was false  
- 0 if no else block and condition was false

### Loop Exit Codes

- `for` and `while` return exit code of last executed command
- Return 0 if loop body never executed

## Combining with Other Features

### With Command Chaining

```bash
if [ -f "$file" ] && grep -q "pattern" "$file"; then
    echo "Found pattern in file"
fi
```

### With Pipes

```bash
for user in $(cat /etc/passwd | cut -d: -f1 | head -5); do
    echo "User: $user"
done
```

### With Redirection

```bash
while read line; do
    process "$line"
done < input.txt > output.txt 2> errors.txt
```

### With Command Substitution

```bash
for file in $(find . -name "*.log"); do
    echo "Log file: $file"
done
```

## Practical Examples

### Process File Arguments

```bash
#!/usr/local/bin/hash-shell

if [ $# -eq 0 ]; then
    echo "Usage: $0 file1 [file2 ...]" >&2
    exit 1
fi

for file in "$@"; do
    if [ -f "$file" ]; then
        echo "Processing: $file"
        wc -l "$file"
    else
        echo "Warning: $file not found" >&2
    fi
done
```

### Menu System

```bash
#!/usr/local/bin/hash-shell

while true; do
    echo ""
    echo "Menu:"
    echo "1. Show date"
    echo "2. Show uptime"
    echo "3. Show disk usage"
    echo "4. Exit"
    echo -n "Choice: "
    read choice
    
    case "$choice" in
        1) date ;;
        2) uptime ;;
        3) df -h ;;
        4) echo "Goodbye!"; exit 0 ;;
        *) echo "Invalid choice" ;;
    esac
done
```

### Retry Logic

```bash
#!/usr/local/bin/hash-shell

max_attempts=5
attempt=0

until [ $attempt -ge $max_attempts ]; do
    attempt=$((attempt + 1))
    echo "Attempt $attempt of $max_attempts..."
    
    if some_command; then
        echo "Success!"
        exit 0
    fi
    
    sleep 2
done

echo "Failed after $max_attempts attempts"
exit 1
```

### Configuration Validation

```bash
#!/usr/local/bin/hash-shell

config_file="$1"

if [ -z "$config_file" ]; then
    echo "Usage: $0 config_file" >&2
    exit 1
fi

if [ ! -f "$config_file" ]; then
    echo "Error: Config file not found" >&2
    exit 1
fi

errors=0
while read line; do
    # Skip comments and empty lines
    case "$line" in
        '#'*|'') continue ;;
    esac
    
    # Validate key=value format
    if ! echo "$line" | grep -q '='; then
        echo "Invalid line: $line"
        errors=$((errors + 1))
    fi
done < "$config_file"

if [ $errors -gt 0 ]; then
    echo "Found $errors error(s)"
    exit 1
fi

echo "Configuration is valid"
```

## Common Mistakes

### Missing Spaces in Tests

```bash
# Wrong
if [-f "$file"]; then

# Correct
if [ -f "$file" ]; then
```

### Unquoted Variables

```bash
# Wrong - fails if file has spaces
for file in $files; do

# Correct
for file in "$files"; do
```

### Missing Semicolons

```bash
# Wrong
if [ -f "$file" ]
then

# Correct (two lines)
if [ -f "$file" ]; then

# Or with newline before then
if [ -f "$file" ]
then
```

### Using = for Integer Comparison

```bash
# Wrong - string comparison
if [ $count = 5 ]; then

# Correct - numeric comparison  
if [ $count -eq 5 ]; then
```

## See Also

- [Test Command](TEST_COMMAND.md)
- [Shell Scripting](SCRIPTING.md)
- [Command Chaining](COMMAND_CHAINING.md)
- [Variable Expansion](VARIABLES.md)

Back to [README](../README.md)
