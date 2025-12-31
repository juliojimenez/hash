# I/O Redirection in Hash Shell

Hash shell supports Unix I/O redirection, allowing you to control where commands read input from and send output to.

## Basic Redirections

### Output Redirection (>)

Redirect stdout to a file:

```bash
#> echo "Hello World" > output.txt
#> cat output.txt
Hello World

#> ls -la > filelist.txt
# Saves directory listing to file
```

**Behavior:**
- Creates file if it doesn't exist
- **Overwrites** file if it exists
- File permissions: 0644 (rw-r--r--)

### Append Redirection (>>)

Append stdout to a file:

```bash
#> echo "Line 1" > log.txt
#> echo "Line 2" >> log.txt
#> cat log.txt
Line 1
Line 2
```

**Behavior:**
- Creates file if it doesn't exist
- Appends to existing file
- Doesn't overwrite

### Input Redirection (<)

Read stdin from a file:

```bash
#> cat < input.txt
# Displays contents of input.txt

#> wc -l < file.txt
# Counts lines, reading from file
```

## Error Redirection

### Redirect stderr (2>)

Redirect error messages to a file:

```bash
#> ls /nonexistent 2> errors.txt
#> cat errors.txt
ls: cannot access '/nonexistent': No such file or directory
```

### Append stderr (2>>)

Append errors to a file:

```bash
#> command1 2> errors.log
#> command2 2>> errors.log
# Both errors in same file
```

### Redirect stderr to stdout (2>&1)

Combine stderr and stdout:

```bash
#> command 2>&1 | grep error
# Searches both stdout and stderr

#> make 2>&1 | tee build.log
# Captures both stdout and stderr
```

### Redirect both (stdout and stderr) (&>)

Shorthand for redirecting both streams:

```bash
#> command &> all.log
# Same as: command > all.log 2>&1

#> make &> build.log
# Captures all output
```

## Combining Redirections

### Input and Output

```bash
#> cat < input.txt > output.txt
# Read from input.txt, write to output.txt

#> sort < unsorted.txt > sorted.txt
```

### Multiple Outputs

```bash
#> command > output.txt 2> errors.txt
# stdout to output.txt, stderr to errors.txt

#> make > build.log 2> errors.log
```

### Redirecting Errors

```bash
#> command 2>&1 > output.txt
# Order matters! stderr to stdout, then both to file
```

## Common Patterns

### Discarding Output

```bash
# Discard stdout
#> command > /dev/null

# Discard stderr
#> command 2> /dev/null

# Discard everything
#> command &> /dev/null
```

### Logging

```bash
# Save all output
#> ./script.sh &> script.log

# Append to existing log
#> ./daily-task.sh &>> daily.log
```

### Processing Files

```bash
# Transform file
#> tr '[:lower:]' '[:upper:]' < input.txt > output.txt

# Sort file
#> sort < unsorted.txt > sorted.txt

# Filter and save
#> grep pattern < large.txt > matches.txt
```

### Error Handling

```bash
# Separate good and bad output
#> command > success.log 2> errors.log

# Check for errors
#> command 2> errors.txt
#> test -s errors.txt && echo "Errors occurred"
```

## Advanced Usage

### With Pipes

Combine redirection with pipes:

```bash
#> cat file.txt | grep pattern > results.txt

#> ls -la | grep txt > txtfiles.txt

#> command 2>&1 | grep ERROR > errors.txt
```

### With Command Chaining

```bash
#> echo "start" > log.txt && command >> log.txt

#> make > /dev/null 2>&1 && echo "Success" || echo "Failed"
```

### With Variables

```bash
#> export OUTPUT=~/results.txt
#> command > $OUTPUT

#> export LOGDIR=/var/log/myapp
#> command &> $LOGDIR/output.log
```

### Temporary Files

```bash
#> command > /tmp/temp.txt
#> process < /tmp/temp.txt > final.txt
#> rm /tmp/temp.txt
```

## Order Matters

The order of redirections can affect behavior:

```bash
# stderr to file, stdout to terminal
#> command 2> errors.txt

# stdout to file, stderr to stdout (both to file)
#> command > output.txt 2>&1

# stdout to file, then stderr to OLD stdout (terminal)
#> command 2>&1 > output.txt
# (Generally not what you want)
```

**Recommended order:**
```bash
command > file 2>&1   # Both to file
command 2>&1 | grep   # Both to pipe
```

## File Descriptors

Standard file descriptors:
- `0` = stdin (standard input)
- `1` = stdout (standard output)
- `2` = stderr (standard error)

**Examples:**
```bash
2> file    # Redirect fd 2 (stderr)
1> file    # Redirect fd 1 (stdout) - same as > file
0< file    # Redirect fd 0 (stdin) - same as < file
```

## Examples

### Simple File Processing

```bash
#> cat < input.txt > output.txt
#> wc -l < file.txt > count.txt
#> sort < data.txt > sorted.txt
```

### Logging

```bash
#> ./script.sh > output.log 2> error.log
#> cron-job.sh &>> /var/log/cron.log
```

### Build Systems

```bash
#> make > build.log 2>&1
#> gcc program.c 2> warnings.txt
```

### Data Pipelines

```bash
#> cat data.csv | cut -d, -f2 | sort | uniq > unique_values.txt
#> grep pattern file.txt | wc -l > match_count.txt
```

## Troubleshooting

### Permission Denied

```bash
#> echo test > /root/file.txt
bash: /root/file.txt: Permission denied
```

Check file/directory permissions.

### No Such File or Directory

For input redirection:
```bash
#> cat < nonexistent.txt
bash: nonexistent.txt: No such file or directory
```

File must exist for input redirection.

### Accidentally Overwriting Files

```bash
#> important-command > important.txt
# Overwrites important.txt!
```

Use `>>` to append, or check first:
```bash
#> test -f important.txt && echo "File exists!"
```

## Limitations

Current limitations:
- ❌ No heredoc: `<< EOF`
- ❌ No here-string: `<<< "string"`
- ❌ No process substitution: `<(command)`
- ❌ No file descriptor duplication: `3>&1`
- ❌ No noclobber protection

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| `>` | ✅ | ✅ |
| `>>` | ✅ | ✅ |
| `<` | ✅ | ✅ |
| `2>` | ✅ | ✅ |
| `2>>` | ✅ | ✅ |
| `&>` | ✅ | ✅ |
| `2>&1` | ✅ | ✅ |
| `<<` (heredoc) | ❌ | ✅ |
| `<<<` (here-string) | ❌ | ✅ |
| `<()` (proc sub) | ❌ | ✅ |

Back to [README](../README.md)
