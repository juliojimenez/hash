# Introduction

**Hash** is a modern, POSIX-compliant command line interpreter for Linux, macOS, and BSD.

## Features

- **POSIX Compliance** - Works with existing shell scripts and POSIX-standard features
- **Interactive Editing** - Full line editing with cursor navigation, history, and tab completion
- **Customizable Prompt** - Git integration, colors, and flexible PS1 escape sequences
- **Scripting Support** - Variables, control structures, functions, and command substitution
- **Modern Conveniences** - Syntax highlighting, autosuggestions, and configurable colors

## Quick Start

```bash
# Install on macOS
brew install juliojimenez/hash/hash-shell

# Install on Debian/Ubuntu
echo "deb [trusted=yes] https://hash-shell.org/apt/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list
sudo apt update && sudo apt install hash-shell

# Run hash
hash-shell
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-c string` | Execute commands from string |
| `-i` | Force interactive mode |
| `-l`, `--login` | Run as a login shell |
| `-s` | Read commands from standard input |
| `-v`, `--version` | Print version information |
| `-h`, `--help` | Show help message |

## Example Session

```bash
$ hash-shell
hash v40
Type 'exit' to quit

#> echo "Hello, World!"
Hello, World!

#> for i in 1 2 3; do echo "Number: $i"; done
Number: 1
Number: 2
Number: 3

#> exit
```

## Getting Help

- **Documentation**: You're reading it!
- **Issues**: [GitHub Issues](https://github.com/juliojimenez/hash/issues)
- **Discussions**: [GitHub Discussions](https://github.com/juliojimenez/hash/discussions)

## License

Hash is open source software licensed under the Apache 2.0 License.

---

Continue to [Installation](installation/README.md) to get started.
