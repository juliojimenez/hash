# Pipes in Hash Shell

Hash shell supports Unix pipes, allowing you to connect the output of one command to the input of another.

## Basic Usage

### Simple Pipe

```bash
#> ls | grep txt
file1.txt
test.txt
```

The `|` operator connects commands:
- `ls` outputs file names
- `grep txt` filters for lines containing "txt"

### Multiple Pipes

Chain multiple commands together:

```bash
#> ls -la | grep -v '^d' | wc -l
# Count non-directory entries

#> cat file.txt | grep pattern | sort | uniq
# Find, sort, and deduplicate patterns
```

## How Pipes Work

### Data Flow

```bash
command1 | command2 | command3
```

- `command1` stdout → `command2` stdin
- `command2` stdout → `command3` stdin
- `command3` stdout → terminal (visible to you)

### Process Creation

Each command in a pipeline runs in its own process:

```bash
#> ps aux | grep firefox
```

Creates two processes:
1. `ps aux` - lists processes
2. `grep firefox` - filters output

### Exit Codes

The pipeline exit code is the exit code of the **last command**:

```bash
#> false | true
#> echo $?
0  ← Exit code from 'true', not 'false'
```

## Common Patterns

### Filtering Output

```bash
# Find specific files
#> ls -la | grep txt

# Show running processes
#> ps aux | grep nginx

# Search logs
#> cat /var/log/syslog | grep error
```

### Counting

```bash
# Count files
#> ls | wc -l

# Count lines in file
#> cat file.txt | wc -l

# Count matching lines
#> ps aux | grep python | wc -l
```

### Text Processing

```bash
# Sort and deduplicate
#> cat names.txt | sort | uniq

# Extract columns
#> ls -la | awk '{print $5, $9}'

# Head and tail
#> cat large.txt | head -100 | tail -10
```

### Transforming Output

```bash
# Convert to uppercase
#> echo hello | tr '[:lower:]' '[:upper:]'

# Replace text
#> echo "hello world" | sed 's/world/universe/'

# Extract fields
#> echo "field1:field2:field3" | cut -d: -f2
```

## Advanced Usage

### Combining with Other Features

**Pipes + Aliases:**
```bash
#> alias count='wc -l'
#> ls | count
```

**Pipes + Variables:**
```bash
#> export PATTERN=error
#> cat log.txt | grep $PATTERN
```

**Pipes + Command Chaining:**
```bash
#> ls | grep txt && echo "Found txt files"
#> cat file.txt | grep pattern || echo "Pattern not found"
```

**Pipes + Tilde:**
```bash
#> cat ~/.bashrc | grep alias
```

### Complex Pipelines

```bash
# Multi-stage data processing
#> cat data.csv | cut -d, -f2 | sort | uniq -c | sort -rn | head -10

# Log analysis
#> cat /var/log/nginx/access.log | grep "404" | wc -l

# Process monitoring
#> ps aux | grep -v grep | grep python | awk '{print $2, $11}'
```

## Pipe Behavior

### Quote Handling

Pipes inside quotes are literal, not operators:

```bash
#> echo "test | test"
test | test  ← Literal pipe character

#> echo 'ls | grep' | cat
ls | grep  ← Passed to cat as input
```

### Whitespace

Whitespace around pipes is optional:

```bash
# All equivalent
#> ls | grep txt
#> ls|grep txt
#> ls |grep txt
#> ls| grep txt
```

### Empty Commands

Empty commands are skipped:

```bash
#> ls | | grep txt  
# Second pipe is ignored
```

## Differences from `||` (OR Operator)

| Operator | Purpose | Example |
|----------|---------|---------|
| `\|` (single) | Connect stdout to stdin | `ls \| grep txt` |
| `\|\|` (double) | Execute if previous failed | `false \|\| echo "failed"` |

**Pipe (data):**
```bash
#> echo hello | cat
hello  ← Output passed through pipe
```

**OR (conditional):**
```bash
#> false || echo hello
hello  ← Executed because false failed
```

## Performance

### Parallel Execution

All commands in a pipeline run **simultaneously**:

```bash
#> slow_command | fast_command | medium_command
# All three run at the same time!
```

### Buffering

Hash uses standard Unix pipes:
- Default 64KB buffer per pipe
- Blocks when buffer is full
- Resumes when data is read

### Process Count

Each command creates a new process:

```bash
#> ps aux | grep nginx | wc -l
# Creates 3 child processes + parent
```

## Limitations

Current limitations:
- ❌ No pipe redirection: `cmd1 |& cmd2` (stderr and stdout)
- ❌ No process substitution: `<(command)`
- ❌ No named pipes: `mkfifo`

## Troubleshooting

### Broken Pipe

If first command generates lots of output but second command exits early:

```bash
#> cat huge_file.txt | head -10
# head exits after 10 lines
# cat receives SIGPIPE (handled automatically)
```

### No Output

If pipe appears to hang:
```bash
#> command_that_never_outputs | grep something
# grep waits forever for input
# Press Ctrl+C to cancel
```

### Unexpected Results

Remember: exit code is from **last** command:

```bash
#> false | true
#> echo $?
0  ← From 'true'
```

## Examples

### Quick Filters

```bash
# Find large files
#> ls -lh | grep "^-" | awk '$5 ~ /M$/ {print $9}'

# Active connections
#> netstat -an | grep ESTABLISHED

# Disk usage
#> du -sh * | sort -h
```

### Log Analysis

```bash
# Error count
#> cat error.log | wc -l

# Unique IPs
#> cat access.log | awk '{print $1}' | sort | uniq

# Recent errors
#> cat error.log | tail -100 | grep CRITICAL
```

### Data Processing

```bash
# CSV column extraction
#> cat data.csv | cut -d, -f2,3

# JSON parsing (with jq)
#> cat data.json | jq '.users[]'

# Top 10 IPs
#> cat access.log | awk '{print $1}' | sort | uniq -c | sort -rn | head -10
```

Back to [README](../README.md)
