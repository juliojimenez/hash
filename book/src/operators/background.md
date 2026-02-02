# Background Processes

Run commands in the background while continuing to use the shell.

## Basic Usage

Add `&` at the end of a command:

```bash
#> long_running_command &
[1] 12345
#>
```

The shell prints:
- Job number in brackets `[1]`
- Process ID `12345`
- Returns to prompt immediately

## Job Control

### List Jobs

```bash
#> jobs
[1]  Running    sleep 100 &
[2]  Running    make &
```

### Bring to Foreground

```bash
#> fg %1
# Job 1 now in foreground
```

### Send to Background

```bash
#> bg %1
# Continues job 1 in background
```

## Stopping Jobs

### Kill by Job Number

```bash
#> kill %1
```

### Kill by PID

```bash
#> kill 12345
```

## Job Completion

When a background job completes:

```bash
[1]  Done       sleep 10 &
```

## Common Patterns

### Run Long Command

```bash
make &
# Continue working while make runs
```

### Multiple Background Jobs

```bash
command1 &
command2 &
command3 &
# All three run simultaneously
```

### Redirect Output

```bash
long_command > output.log 2>&1 &
# Run in background, save output to file
```

### Discard Output

```bash
noisy_command &> /dev/null &
```

## Background Process Behavior

### stdin

Background processes have stdin disconnected from terminal:

```bash
cat &
# Will receive EOF immediately
```

### Signals

Background processes:
- Ignore `SIGINT` (Ctrl+C)
- Receive `SIGHUP` when shell exits (unless using `nohup`)

### Exit Warning

When exiting with running jobs:

```bash
#> exit
hash: There are running jobs.
#> exit
# Second exit forces quit
```

## Practical Examples

### Build While Working

```bash
make clean && make &
# Edit files while building
```

### Download in Background

```bash
curl -O http://example.com/large-file.zip &
```

### Run Server

```bash
python -m http.server 8000 &
```

### Watch Logs

```bash
tail -f /var/log/syslog &
```

## Tips

### Check if Job Running

```bash
jobs
# or
ps aux | grep command
```

### Wait for Completion

```bash
command &
pid=$!
# Do other work...
wait $pid
echo "Command finished"
```

### Nohup for Persistent Jobs

For jobs that should survive logout:

```bash
nohup long_command &
# Output goes to nohup.out
```

## Limitations

- Job control is basic compared to bash
- No `disown` command
- No sophisticated job status tracking
