# Command Substitution

Use command output as part of another command.

## Syntax

### $(...) - Modern Syntax (Recommended)

```bash
result=$(command)
echo "Today is $(date)"
```

### \`...\` - Backtick Syntax

```bash
result=`command`
echo "Today is `date`"
```

The `$(...)` syntax is preferred as it's easier to nest and read.

## Basic Usage

### Capture Output

```bash
current_dir=$(pwd)
echo "You are in: $current_dir"

file_count=$(ls | wc -l)
echo "Files: $file_count"
```

### In Commands

```bash
echo "Today is $(date)"
# Today is Mon Jan 1 12:00:00 UTC 2024

cd $(dirname "$0")
# Change to script's directory
```

### In Variables

```bash
VERSION=$(cat VERSION)
HOSTNAME=$(hostname)
USER_HOME=$(eval echo ~$USER)
```

## Common Patterns

### Get Current Date/Time

```bash
NOW=$(date +%Y%m%d)
TIMESTAMP=$(date +%H%M%S)
```

### Count Items

```bash
NUM_FILES=$(ls *.txt | wc -l)
NUM_LINES=$(wc -l < file.txt)
```

### Find Commands

```bash
EDITOR=$(which vim)
GIT_ROOT=$(git rev-parse --show-toplevel)
```

### Process Output

```bash
for file in $(find . -name "*.txt"); do
    echo "Processing: $file"
done

for user in $(cat /etc/passwd | cut -d: -f1); do
    echo "User: $user"
done
```

## Nesting

The `$(...)` syntax allows easy nesting:

```bash
echo "Script dir: $(dirname $(realpath $0))"

# With backticks (harder to read)
echo "Script dir: `dirname \`realpath $0\``"
```

## In Arithmetic

```bash
total=$(($(cat file1 | wc -l) + $(cat file2 | wc -l)))
```

## With Quoting

```bash
# Preserve whitespace
files="$(ls -la)"
echo "$files"

# Word splitting (each line becomes argument)
for line in $(cat file.txt); do
    echo "$line"
done
```

## Exit Codes

Command substitution captures output, not exit code:

```bash
result=$(false)
echo $?  # 1 (exit code available immediately after)

# Check both
if output=$(command); then
    echo "Success: $output"
else
    echo "Failed"
fi
```

## Examples

### Backup with Date

```bash
backup_file="backup_$(date +%Y%m%d).tar.gz"
tar -czf "$backup_file" /data
```

### Dynamic Paths

```bash
cd "$(git rev-parse --show-toplevel)"
source "$(dirname $0)/config.sh"
```

### Conditional on Output

```bash
if [ "$(whoami)" = "root" ]; then
    echo "Running as root"
fi
```

### Build Version String

```bash
VERSION="$(cat VERSION)-$(git rev-parse --short HEAD)"
```

## Limitations

- Trailing newlines are stripped
- Very large output may be slow
- Exit code only available immediately after
