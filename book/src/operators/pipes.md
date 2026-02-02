# Pipes

Connect the output of one command to the input of another.

## Basic Usage

```bash
#> ls | grep txt
file1.txt
test.txt

#> cat file.txt | grep pattern | sort | uniq
```

## How Pipes Work

```bash
command1 | command2 | command3
```

- `command1` stdout → `command2` stdin
- `command2` stdout → `command3` stdin
- `command3` stdout → terminal

Each command runs in its own process.

## Exit Code

Pipeline exit code is from the **last command**:

```bash
#> false | true
#> echo $?
0  # From 'true', not 'false'
```

## Common Patterns

### Filtering

```bash
ls -la | grep txt
ps aux | grep nginx
cat log.txt | grep error
```

### Counting

```bash
ls | wc -l                    # Count files
cat file.txt | wc -l          # Count lines
ps aux | grep python | wc -l  # Count processes
```

### Text Processing

```bash
cat names.txt | sort | uniq
ls -la | awk '{print $5, $9}'
echo hello | tr '[:lower:]' '[:upper:]'
```

### Data Analysis

```bash
cat data.csv | cut -d, -f2 | sort | uniq -c | sort -rn | head -10
```

## With Other Features

### With Aliases

```bash
alias count='wc -l'
ls | count
```

### With Variables

```bash
export PATTERN=error
cat log.txt | grep $PATTERN
```

### With Chaining

```bash
ls | grep txt && echo "Found txt files"
```

## Difference from ||

| Operator | Purpose |
|----------|---------|
| `\|` | Connect stdout to stdin |
| `\|\|` | Execute if previous failed |

```bash
echo hello | cat      # Pipe: passes "hello" to cat
false || echo hello   # OR: runs echo because false failed
```

## Troubleshooting

### Broken Pipe

If first command produces lots of output but second exits early:
```bash
cat huge_file.txt | head -10
# Normal behavior, handled automatically
```

### No Output / Hanging

Command waiting for input:
```bash
cat | grep something
# Waiting forever - press Ctrl+C
```

## Limitations

- No `|&` (pipe stderr)
- No process substitution `<(command)`
