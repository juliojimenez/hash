# Command Chaining in Hash Shell

Hash shell supports chaining multiple commands together using shell operators.

## Operators

### Semicolon (;) - Always Execute

Execute commands sequentially, regardless of success or failure:

```bash
#> echo first ; echo second ; echo third
first
second
third
```

The `;` operator always executes the next command, even if the previous one failed:

```bash
#> false ; echo "This still runs"
This still runs
```

### AND (&&) - Execute on Success

Execute the next command **only if** the previous command succeeded (exit code 0):

```bash
#> true && echo "Success!"
Success!

#> false && echo "Won't see this"
# (nothing printed)
```

**Common use cases:**
```bash
# Compile and run if compilation succeeds
#> gcc program.c -o program && ./program

# Create directory and cd into it
#> mkdir newdir && cd newdir

# Check if file exists and cat it
#> test -f file.txt && cat file.txt
```

### OR (||) - Execute on Failure

Execute the next command **only if** the previous command failed (non-zero exit code):

```bash
#> true || echo "Won't see this"
# (nothing printed)

#> false || echo "Failure!"
Failure!
```

**Common use cases:**
```bash
# Try command, print error if it fails
#> command || echo "Command failed!"

# Try primary, fall back to secondary
#> which python3 || which python

# Create directory if it doesn't exist
#> test -d mydir || mkdir mydir
```

## Combining Operators

You can combine different operators in the same line:

```bash
# Try to compile, run if successful, or print error
#> gcc program.c && ./a.out || echo "Failed"

# Chain multiple commands
#> cd /tmp ; ls -la && echo "Listed /tmp"

# Complex workflow
#> make clean ; make && make test || echo "Build failed"
```

**Execution order is left-to-right:**
```bash
#> cmd1 && cmd2 || cmd3 ; cmd4
```
1. Execute `cmd1`
2. If `cmd1` succeeds, execute `cmd2`
3. If `cmd2` fails, execute `cmd3`
4. Always execute `cmd4`

## Operators in Quotes

Operators inside quotes are treated as literal text, not operators:

```bash
#> echo "test && test"
test && test

#> echo 'first ; second'
first ; second

#> alias test='echo "a && b"'
#> test
a && b
```

## Whitespace

Whitespace around operators is optional:

```bash
# All equivalent
#> echo a ; echo b
#> echo a; echo b
#> echo a ;echo b
#> echo a;echo b
```

## Exit Codes

Each command in a chain has its own exit code:

```bash
#> true && echo "Success"
Success
# Exit code: 0

#> false && echo "Won't run"
# Exit code: 1 (from false)

#> false || echo "Fallback"
Fallback
# Exit code: 0 (from echo)
```

The **last executed command** determines the overall exit code.

## Practical Examples

### Build and Test Workflow

```bash
#> make clean ; make && make test && echo "All tests passed!"
```

### File Operations

```bash
# Create backup before editing
#> cp file.txt file.txt.bak && vim file.txt

# Remove file if it exists
#> test -f oldfile && rm oldfile

# Create directory structure
#> mkdir -p project/src && cd project/src
```

### Error Handling

```bash
# Try to start service, report status
#> systemctl start myservice && echo "Started" || echo "Failed to start"

# Download file with fallback
#> curl -O url1 || curl -O url2 || echo "All downloads failed"
```

### Conditional Execution

```bash
# Only run tests if build succeeds
#> make && make test

# Clean up only if command succeeds
#> ./process_data && rm temp_files/*

# Chain git commands
#> git add . && git commit -m "Update" && git push
```

### Aliases with Chaining

You can use chaining in aliases:

```bash
# In .hashrc
alias update='sudo apt update && sudo apt upgrade'
alias gc='git add . && git commit'
alias deploy='make clean && make && make deploy'
```

## Limitations

### Not Supported (Yet)

- **Pipes** (`|`) - Redirect output between commands
- **Background execution** (`&`) - Run commands in background
- **Grouping** (`(cmd1 && cmd2)`) - Group commands
- **Redirection chaining** - Complex I/O redirection

### Workarounds

**For pipes**, use intermediate files:
```bash
#> ls > tmp.txt ; grep txt tmp.txt ; rm tmp.txt
```

**For background**, run in a separate terminal or use screen/tmux.

## Tips

1. **Test each command first** - Make sure individual commands work before chaining

2. **Use && for safety** - Prevents later commands if earlier ones fail:
   ```bash
   # Good
   cd important-dir && rm -rf *
   
   # Dangerous (if cd fails, rm runs in wrong directory!)
   cd important-dir ; rm -rf *
   ```

3. **OR for fallbacks** - Provide alternatives:
   ```bash
   command || fallback-command || echo "All failed"
   ```

4. **Semicolon for sequences** - When you want all commands to run:
   ```bash
   echo "Starting" ; long-command ; echo "Done"
   ```

5. **Check exit codes** - The prompt shows if last command failed (red `#>`)

## Comparison with Bash

Hash shell's command chaining works like bash:

| Operator | Hash | Bash |
|----------|------|------|
| `;` | ✅ Yes | ✅ Yes |
| `&&` | ✅ Yes | ✅ Yes |
| `\|\|` | ✅ Yes | ✅ Yes |
| `\|` | ❌ Not yet | ✅ Yes |
| `&` | ❌ Not yet | ✅ Yes |
| `()` | ❌ Not yet | ✅ Yes |

## See Also

- [Built-in Commands](BUILTINS.md)
- [.hashrc Configuration](HASHRC.md)
- [Prompt Customization](PROMPT.md) - Exit code indicators

