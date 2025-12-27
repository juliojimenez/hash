# PS1 Quick Reference

Quick reference guide for hash shell prompt customization.

## Escape Sequences

| Sequence | Description | Example Output |
|----------|-------------|----------------|
| `\w` | Full path (~ for home) | `/home/user/projects` or `~/projects` |
| `\W` | Current directory name | `projects` |
| `\u` | Username | `julio` |
| `\h` | Hostname | `laptop` |
| `\g` | Git branch with status | ` git:(main)` |
| `\e` | Exit code color | (sets color) |
| `\$` | `$` for user, `#` for root | `$` or `#` |
| `\n` | Newline | (line break) |
| `\\` | Literal backslash | `\` |

## Common Patterns

### Minimal

```bash
set PS1='\W #> '
```

Output: `projects #> `

### Default
```bash
set PS1='\w\g \e#>\e '
```
Output: `/home/user/projects git:(main) #> `

### Compact with Git
```bash
set PS1='\W\g \e#>\e '
```
Output: `projects git:(main) #> `

### Classic Bash-style
```bash
set PS1='\u@\h:\w\$ '
```
Output: `user@hostname:/home/user/projects $ `

### Two-Line
```bash
set PS1='\w\g\n\e#>\e '
```
Output:
```
/home/user/projects git:(main)
#> 
```

### User and Directory
```bash
set PS1='\u:\W\$ '
```
Output: `julio:projects $ `

## Color Indicators

### Git Status Colors
- `git:` **green** = clean repository
- `git:` **yellow** = uncommitted changes
- Branch name **cyan**

### Exit Code Colors
Wrap with `\e...\e` to color based on exit code:
- **Blue** = success (exit code 0)
- **Red** = failure (non-zero exit code)

Example: `\e#>\e` makes `#>` blue or red

## Quick Tips

1. **Test first**: Try PS1 values interactively before adding to .hashrc
   ```bash
   export PS1='\W\g \e#>\e '
   ```

2. **Keep it simple**: Complex prompts (especially with git) can slow down the shell

3. **Two-line for long paths**: Prevents command wrapping
   ```bash
   set PS1='\w\g\n\e#>\e '
   ```

4. **No git in large repos**: Skip `\g` if git checks are slow
   ```bash
   set PS1='\w \e#>\e '
   ```

## Special Cases

### Show Path for Root
```bash
# Different for root user
set PS1='\u@\h:\w\# '
```

### No Colors
```bash
# Without color codes
set PS1='\W $ '
```

### Minimal Debug
```bash
# Just to see what's happening
set PS1='[\w] $ '
```

## Examples by Use Case

### For Developers
```bash
# Full context with git
set PS1='\W\g \e#>\e '
```

### For Sysadmins
```bash
# User@host for ssh sessions
set PS1='\u@\h:\w\$ '
```

### For Minimalists
```bash
# Just directory
set PS1='\W $ '
```

### For Power Users
```bash
# Everything on two lines
set PS1='\u@\h:\w\g\n\e└─\$\e '
```

Back to [README](../README.md)
