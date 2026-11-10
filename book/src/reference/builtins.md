# Built-in Commands

<!-- docs-parity: generated from the builtin table in src/builtins.c -->
<!-- by scripts/docs-parity.py; `help NAME` prints the same text. Edit the table, then run: make docs -->

The 44 builtins, as `help` lists them in the shell; `help NAME` prints one entry.
Every builtin takes `--` as the end of its options. The [quick reference](index.md#built-in-commands)
has the one-line form.

## . {#dot}

```
. file
```

Read and run file in the current shell (POSIX spelling of source). `return` ends the file; `exit` ends the shell.

## : {#colon}

```
: [arg...]
```

Do nothing; the arguments are expanded and dropped. Status 0.

## [ {#bracket}

```
[ expr ]
```

The test command written with brackets; the closing ] is required. See `help test` for the operators.

More: [test command](../scripting/test-command.md)

## [[ {#[[}

```
[[ expr ]]
```

Extended test: the operators of test plus pattern matching with == and !=, regular expressions with =~, and && and || inside the brackets. Words are not split or glob-expanded.

More: [test command](../scripting/test-command.md)

## alias {#alias}

```
alias [name[=value]...]
```

Define aliases, or print them: `alias` lists every alias, `alias name` prints one. An alias is expanded when its name is the first word of a command.

More: [hashrc](../getting-started/hashrc.md)

## bg {#bg}

```
bg [%job]
```

Continue a stopped job in the background.

More: [background](../operators/background.md)

## break {#break}

```
break [n]
```

Leave the innermost n enclosing loops (one by default).

## cd {#cd}

```
cd [-L|-P] [dir]
```

Change the working directory to dir, or to $HOME without one. -L, the default, keeps the logical path, so `cd ..` under a symlinked directory goes to the parent of the link; -P follows symlinks and sets PWD to the physical path. `cd -` goes back to the previous directory and prints it. PWD and OLDPWD are updated.

## command {#command}

```
command name [arg...]
command -v | -V name...
```

Run name without looking up functions, so a function can call the command it shadows; a failing special builtin does not end the shell, but break, continue, return and exit keep their effect. -v prints how name would be resolved, -V describes it.

## complete {#complete}

```
complete [-abcdfuv] [-o default|dirnames] [-W words] [-F function] name...
complete -p [name...]
complete -r [name...]
```

Record how a command's arguments are tab-completed. -W gives the candidate words, -F a function whose output lines are candidates ($1 command, $2 word, $3 previous word), -a -b -c -d -f -u -v add aliases, builtins, commands, directories, files, users or variables, -o default falls back to files. -p prints the definitions in a form that can be typed back, -r removes them.

More: [tab completion](../shell-features/tab-completion.md)

## continue {#continue}

```
continue [n]
```

Resume the next iteration of the innermost n enclosing loops.

## echo {#echo}

```
echo [-neE] [arg...]
```

Print the arguments separated by spaces, then a newline. -n leaves out the newline, -e interprets backslash escapes (\n \t \\ ...), -E does not.

## eval {#eval}

```
eval [arg...]
```

Join the arguments with spaces and run the result as a command in the current shell.

## exec {#exec}

```
exec [command [arg...]]
exec redirection...
```

Replace the shell with command. With only redirections, apply them to the shell itself for the commands that follow (`exec 3< file`, `exec >log`).

More: [redirection](../operators/redirection.md)

## exit {#exit}

```
exit [n]
```

Leave the shell with status n, or with the status of the last command. The EXIT trap runs first; a login shell also runs ~/.hash_logout.

## export {#export}

```
export [-p] [name[=value]...]
```

Mark variables for export to the environment of commands, assigning a value when one is given. -p, or no arguments, prints the exported variables.

## false {#false}

```
false
```

Do nothing, with status 1.

## fg {#fg}

```
fg [%job]
```

Bring a job to the foreground and wait for it. Without an argument, the current job.

More: [background](../operators/background.md)

## getopts {#getopts}

```
getopts optstring name [arg...]
```

Parse options from the positional parameters (or arg...) one call at a time, placing the letter in name, its argument in OPTARG and the next index in OPTIND. A colon after a letter in optstring means it takes an argument; a leading colon selects silent error reporting.

## hash {#hash}

```
hash [-r] [name...]
```

Remember where commands live. With names, look them up now; -r forgets everything remembered; alone, print the table.

## help {#help}

```
help [-s] [pattern...]
```

Describe the builtins and the shell's keywords. Alone, list every topic with its synopsis; with names or glob patterns, print the synopsis and a short description of each; -s prints the synopsis only. The book has the same text at https://hash-shell.org/reference/builtins.html

## history {#history}

```
history [-c | -w | -r]
```

Print the command history with its numbers. -c clears it, -w writes it to the history file, -r reads the file back. HISTFILE, HISTSIZE, HISTFILESIZE and HISTCONTROL apply; `!!`, `!n`, `!-n` and `!prefix` expand entries.

More: [history](../shell-features/history.md)

## jobs {#jobs}

```
jobs [-l | -p]
```

List the background jobs of this shell. -l adds the process IDs, -p prints only them.

More: [background](../operators/background.md)

## kill {#kill}

```
kill [-s signal | -signal] pid | %job ...
kill -l
```

Send a signal (TERM by default) to processes or jobs, by number or name. -l lists the signal names.

More: [background](../operators/background.md)

## local {#local}

```
local [name[=value]...]
```

Declare variables local to the current function; the previous values come back when it returns. Outside a function it is an error.

More: [index](../scripting/index.md#functions)

## logout {#logout}

```
logout
```

Leave a login shell, running ~/.hash_logout first. In a shell that is not a login shell it fails; use exit.

## pwd {#pwd}

```
pwd [-L|-P]
```

Print the working directory. -L (the default) prints $PWD, the path cd built, when it still names the current directory; -P prints the physical path with every symbolic link resolved.

## read {#read}

```
read [-r] name...
```

Read one line from standard input and split it into the variables; the last one takes the rest. Backslash continues a line unless -r is given. The status is 1 at end of input.

## readonly {#readonly}

```
readonly [-p] [name[=value]...]
```

Mark variables read-only, assigning a value when one is given; later assignments and unset fail. -p, or no arguments, prints them.

## return {#return}

```
return [n]
```

Leave the current function or sourced file with status n, or with the status of the last command.

More: [index](../scripting/index.md#functions)

## set {#set}

```
set [-abCefhmnuvx] [-o option] [+...] [--] [arg...]
set name=value
```

Set shell options with -, clear them with +: -a allexport, -b notify, -C noclobber, -e errexit, -f noglob, -h, -m monitor, -n noexec, -u nounset, -v verbose, -x xtrace; -o takes a name, also emacs, vi, nolog and nonlexicalctrl. `set -o` lists the options, `set +o` prints them as commands, $- holds the letters in effect. Arguments after the options (or after --) become the positional parameters. hash's own settings use name=value: prompt=NAME picks a prompt preset (classic, plain, minimal, two-line, powerline; prompt=list shows them), PS1=... builds one, and colors=on|off, welcome=on|off, color.<element>=<spec>, syntax_highlight=on|off, autosuggest=on|off, danger_highlight=on|off. They work at the prompt, in scripts and in ~/.hashrc.

More: [index](../reference/index.md#shell-options)

## shift {#shift}

```
shift [n]
```

Drop the first n positional parameters (one by default). A count past $# is an error.

## source {#source}

```
source file
```

Read and run file in the current shell, like `.`. Functions, variables and aliases it defines stay. A file name without a slash is looked up in PATH.

## test {#test}

```
test expr
```

Evaluate a conditional expression: file tests (-e -f -d -r -w -x -s -L and more), string tests (-z -n = !=), numbers (-eq -ne -lt -le -gt -ge), and ! -a -o with parentheses. The status is 0 when expr is true. `[ expr ]` is the same command with a closing bracket.

More: [test command](../scripting/test-command.md)

## times {#times}

```
times
```

Print the user and system times used by the shell and by its children.

## trap {#trap}

```
trap [action] [signal...]
trap -l | -p
```

Run action when a signal arrives, or at EXIT. An empty action ignores the signal, `-` restores the default. -l lists the signal names, -p prints the traps in effect. The EXIT trap sees the shell's status as $?.

## true {#true}

```
true
```

Do nothing, successfully.

## type {#type}

```
type [-afptP] name...
```

Say how each name would be interpreted: an alias, a shell keyword, a function, a builtin, or a file found in PATH, in that order. -t prints one word (alias, keyword, function, builtin, file); -p prints the file that would run and nothing for anything else; -P searches PATH even when an alias, function or builtin would run; -a prints every match; -f skips functions. Status 1 when a name is not found.

## ulimit {#ulimit}

```
ulimit [-HS] [-a | -cdfnstuv] [limit]
```

Show or set a resource limit of the shell and the commands it runs. -c core file (512-byte blocks), -d data (KB), -f file size (blocks), -n open files, -s stack (KB), -t CPU seconds, -u processes, -v virtual memory (KB); -f when no resource is named. -a shows them all. -H acts on the hard limit, -S on the soft one (both when setting, the soft one when showing). The limit is a number or unlimited.

## umask {#umask}

```
umask [-S] [mode]
```

Show or set the file creation mask. Without a mode, print it in octal, or with -S as u=rwx,g=rx,o=rx. A mode is octal (022) or symbolic (u+r, g-w,o=, a=rx) as for chmod.

## unalias {#unalias}

```
unalias [-a] name...
```

Remove the named aliases, or every alias with -a. A name that is not an alias is reported and the status is 1; the other names are still removed.

More: [hashrc](../getting-started/hashrc.md)

## unset {#unset}

```
unset [-f | -v] name...
```

Remove variables (-v, the default) or functions (-f). Readonly variables cannot be unset.

## update {#update}

```
update
```

Check for a newer release of hash and install it when one is available. HASH_DISABLE_UPDATE_CHECK=1 turns the startup check off.

## wait {#wait}

```
wait [pid | %job ...]
```

Wait for the given background jobs, or for all of them, and return the status of the last one waited for.

More: [background](../operators/background.md)
