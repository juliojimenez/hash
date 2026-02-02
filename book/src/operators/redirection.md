# I/O Redirection

Control where commands read from and write to.

## Basic Redirections

### Output (>)

Redirect stdout to file (overwrites):

```bash
echo "Hello" > output.txt
ls -la > filelist.txt
```

### Append (>>)

Append to file:

```bash
echo "Line 1" > log.txt
echo "Line 2" >> log.txt
```

### Input (<)

Read from file:

```bash
cat < input.txt
wc -l < file.txt
sort < unsorted.txt > sorted.txt
```

## Error Redirection

### Stderr (2>)

```bash
ls /nonexistent 2> errors.txt
command 2>> errors.log  # Append
```

### Stderr to Stdout (2>&1)

```bash
command 2>&1 | grep error
make 2>&1 | tee build.log
```

### Both Streams (&>)

```bash
command &> all.log
# Same as: command > all.log 2>&1
```

## Common Patterns

### Discard Output

```bash
command > /dev/null           # Discard stdout
command 2> /dev/null          # Discard stderr
command &> /dev/null          # Discard both
```

### Logging

```bash
./script.sh &> script.log
./daily.sh &>> daily.log      # Append
```

### Separate Streams

```bash
command > output.txt 2> errors.txt
```

### Processing Files

```bash
tr '[:lower:]' '[:upper:]' < input.txt > output.txt
sort < data.txt > sorted.txt
```

## Order Matters

```bash
# Both to file
command > file 2>&1

# Both to pipe
command 2>&1 | grep error
```

## File Descriptors

- `0` = stdin
- `1` = stdout
- `2` = stderr

```bash
2> file    # Redirect fd 2
1> file    # Same as > file
0< file    # Same as < file
```

## Troubleshooting

### Permission Denied

Check file/directory permissions.

### Accidental Overwrite

Use `>>` to append, or check first:
```bash
test -f file.txt && echo "File exists!"
```

## Limitations

- No heredoc (`<< EOF`)
- No here-string (`<<< "string"`)
- No process substitution (`<(command)`)
- No noclobber protection
