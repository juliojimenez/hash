# Command Chaining

Execute multiple commands with conditional or sequential logic.

## Operators

| Operator | Name | Behavior |
|----------|------|----------|
| `&&` | AND | Run next if previous succeeded (exit 0) |
| `\|\|` | OR | Run next if previous failed (exit non-0) |
| `;` | Sequential | Run next regardless |

## AND (&&)

Run second command only if first succeeds:

```bash
make && make install
# make install runs only if make succeeds

cd /project && npm install
# npm install runs only if cd succeeds

mkdir -p dir && cd dir && touch file
# Creates dir, enters it, creates file
```

## OR (||)

Run second command only if first fails:

```bash
cd /nonexistent || echo "Directory not found"
# echo runs because cd failed

command || exit 1
# Exit script if command fails

test -f config || cp default.conf config
# Copy default only if config doesn't exist
```

## Sequential (;)

Run commands in sequence regardless of exit codes:

```bash
echo "Start"; command; echo "End"
# All three run

cd /tmp; ls; pwd
# All run even if one fails
```

## Combined Patterns

### Build and Install

```bash
make clean && make && make install
```

### Create If Not Exists

```bash
test -d "$dir" || mkdir -p "$dir"
```

### Try Multiple Options

```bash
command1 || command2 || command3
# Tries each until one succeeds
```

### Fallback with Message

```bash
./run.sh || echo "Script failed" && exit 1
```

### Grouped Commands

```bash
(cd /project && make) || echo "Build failed"
```

## Exit Codes

### AND Chain

```bash
true && echo "runs"     # runs
false && echo "skipped" # skipped
```

### OR Chain

```bash
true || echo "skipped"  # skipped
false || echo "runs"    # runs
```

### Chain Exit Code

Exit code is from the last executed command:

```bash
true && false
echo $?  # 1 (from false)

false || true
echo $?  # 0 (from true)
```

## Common Use Cases

### Safe Directory Operations

```bash
cd /some/dir && rm -rf temp/
# Only deletes if cd succeeds
```

### Conditional Execution

```bash
[ -f config.yml ] && source config.yml
```

### Error Handling

```bash
command || { echo "Error" >&2; exit 1; }
```

### Cleanup on Success

```bash
make && make test && make clean
```

## With Other Features

### With Pipes

```bash
cat file | grep pattern && echo "Found"
make 2>&1 | tee log && echo "Success"
```

### With Redirection

```bash
command > output && echo "Saved"
```

### With Variables

```bash
export DEBUG=1 && ./run.sh
```
