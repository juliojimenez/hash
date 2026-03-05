# hash-shell demo
# Install VHS: brew install vhs
# Run: vhs demo.tape

Output demo.gif

Set FontSize 16
Set Width 900
Set Height 500
Set Shell "bash"
Set Theme "Catppuccin Mocha"
Set TypingSpeed 100ms

# Launch hash-shell
Hide
Type "export TERM=xterm-256color && unset PS1 && hash-shell"
Enter
Sleep 1s
Show

# Show the prompt with git info and syntax highlighting
Sleep 1s
Type "echo 'Hello from hash-shell!'"
Sleep 500ms
Enter
Sleep 1s

# Pipe and chain operators
Type "ls src/*.c | wc -l && echo 'source files'"
Sleep 500ms
Enter
Sleep 1s

# Tab completion
Type "cat READ"
Sleep 300ms
Tab
Sleep 500ms
Enter
Sleep 1.5s

# Aliases
Type "alias ll='ls -lah'"
Sleep 300ms
Enter
Sleep 500ms
Type "ll src/"
Sleep 300ms
Enter
Sleep 1.5s

# Redirection
Type "echo 'hello world' > /tmp/demo.txt && cat /tmp/demo.txt"
Sleep 500ms
Enter
Sleep 1s
