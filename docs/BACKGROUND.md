# Background Processes in Hash Shell

Hash shell supports running commands in the background, allowing you to continue working while long-running processes execute.

## Basic Usage

### Running a Command in Background

Add `&` at the end of a command to run it in the background:

```bash
#> sleep 10 &
[1] 12345
#> 
```

The shell immediately returns to the prompt while `sleep` runs in the background.

**Output:**
- `[1]` - Job number (used with `fg`, `bg`, `jobs`)
- `12345` - Process ID (PID)

### Examples

```bash
# Long-running compilation
#> make all &
[1] 12346

# Download in background
#> curl -O https://example.com/large-file.zip &
[2] 12347

# Run a server
#> python3 -m http.server 8000 &
[3] 12348
```

## Job Control Commands

### jobs - List Background Jobs

Show all background jobs:

```bash
#> jobs
[1]- Running       sleep 100 &
[2]+ Running       make all &
```

**Markers:**
- `+` - Current job (most recent)
- `-` - Previous job

**States:**
- `Running` - Job is executing
- `Stopped` - Job was suspended (Ctrl+Z)
- `Done` - Job completed
- `Terminated` - Job was killed

### fg - Bring Job to Foreground

Bring a background job to the foreground:

```bash
# Bring current job to foreground
#> fg

# Bring specific job to foreground
#> fg %1
#> fg 1

# Using job number
#> fg %2
```

### bg - Continue in Background

Continue a stopped job in the background:

```bash
# Continue current stopped job
#> bg

# Continue specific job
#> bg %1
```

## Job Specifications

Jobs can be specified in multiple ways:

| Syntax | Meaning |
|--------|---------|
| `%n` | Job number n |
| `n` | Job number n |
| `%%` | Current job |
| `%+` | Current job |
| `%-` | Previous job |

**Examples:**
```bash
#> fg %1     # Foreground job 1
#> fg 2      # Foreground job 2
#> bg %%     # Background current job
```

## Stopping and Resuming Jobs

### Ctrl+Z - Suspend Foreground Job

Press `Ctrl+Z` to suspend the current foreground process:

```bash
#> sleep 100
^Z
[1]+  Stopped       sleep 100
#> 
```

The job is now stopped and can be:
- Resumed in foreground with `fg`
- Resumed in background with `bg`

### Example Workflow

```bash
# Start a long process
#> make

# Oops, want to do something else
# Press Ctrl+Z
^Z
[1]+  Stopped       make

# Check email or do other work
#> cat /var/mail/user

# Resume compilation in background
#> bg
[1]+ make &

# Or bring back to foreground
#> fg
```

## Job Completion Notifications

When a background job completes, you'll be notified at the next prompt:

```bash
#> sleep 5 &
[1] 12345
#> 
# ... wait 5 seconds and press Enter ...
[1]  Done          sleep 5
#> 
```

## Multiple Background Jobs

You can run multiple commands in the background:

```bash
#> make &
[1] 12345
#> npm install &
[2] 12346
#> python3 test.py &
[3] 12347

#> jobs
[1]  Running       make &
[2]- Running       npm install &
[3]+ Running       python3 test.py &
```

## Background with Pipelines

Pipelines can also be run in the background:

```bash
#> cat large.txt | sort | uniq > sorted.txt &
[1] 12345
```

## Background with Command Chaining

Use `&` with command chaining operators:

```bash
# Run make in background, then continue
#> make & echo "Build started"
[1] 12345
Build started

# Sequential commands, last in background
#> cd project ; make &
[1] 12346
```

## Exiting with Background Jobs

If you try to exit with running background jobs:

```bash
#> exit
There are 2 running job(s).
Use 'exit' again to force exit, or 'jobs' to see them.
#> jobs
[1]- Running       sleep 100 &
[2]+ Running       make &
#> exit
Bye :)
```

The shell warns you but allows force exit on second attempt.

## Output from Background Jobs

Background job output goes to the terminal:

```bash
#> echo "hello" &
[1] 12345
hello
[1]  Done          echo "hello"
```

To redirect output:

```bash
# Redirect stdout
#> long_command > output.log &

# Redirect both stdout and stderr
#> long_command &> all.log &

# Discard output
#> noisy_command > /dev/null 2>&1 &
```

## Tips and Best Practices

### 1. Redirect Output for Long Jobs

```bash
#> ./build.sh > build.log 2>&1 &
```

### 2. Use nohup for Persistent Jobs

Jobs started with `&` will be terminated when the shell exits. For persistent jobs:

```bash
#> nohup ./server.sh > server.log 2>&1 &
```

### 3. Check Job Status Regularly

```bash
#> jobs
```

### 4. Clean Up Before Exiting

```bash
#> jobs           # See what's running
#> fg %1          # Bring to foreground and wait
# or
#> kill %1        # Kill the job
```

## Limitations

Current limitations:
- ❌ `disown` command (remove job from table)
- ❌ `wait` command (wait for background jobs)
- ❌ Job control in subshells
- ❌ `%string` job specification (by command prefix)
- ❌ `%?string` job specification (by command substring)

## Comparison with Bash

| Feature | Hash | Bash |
|---------|------|------|
| `command &` | ✅ | ✅ |
| `jobs` | ✅ | ✅ |
| `fg` | ✅ | ✅ |
| `bg` | ✅ | ✅ |
| `%n` job spec | ✅ | ✅ |
| `%%`, `%+` | ✅ | ✅ |
| Ctrl+Z suspend | ✅ | ✅ |
| Job notifications | ✅ | ✅ |
| `disown` | ❌ | ✅ |
| `wait` | ❌ | ✅ |
| `%string` spec | ❌ | ✅ |

## Troubleshooting

### Job Not Responding to fg

Check if the process is still alive:

```bash
#> jobs
[1]+ Running       my_command &

# If it shows "Running" but fg doesn't work:
#> kill -0 %1  # Check if process exists
```

### Background Job Output Interferes

Redirect output to avoid cluttering the terminal:

```bash
#> noisy_command > /dev/null 2>&1 &
```

### Jobs Disappear After Exit

Background jobs are children of the shell. When the shell exits, they receive SIGHUP. Use `nohup` for persistent jobs.

Back to [README](../README.md)
