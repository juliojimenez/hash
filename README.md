<img style="width:128px;" src="images/hash.png" />

[![Tests](https://github.com/juliojimenez/hash/actions/workflows/tests.yml/badge.svg)](https://github.com/juliojimenez/hash/actions/workflows/tests.yml) [![Sanitizers & Fuzzing](https://github.com/juliojimenez/hash/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/juliojimenez/hash/actions/workflows/sanitizers.yml) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=hash-shell&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=hash-shell) ![GitHub Release](https://img.shields.io/github/v/release/juliojimenez/hash?display_name=release)


# hash
Command line interpreter (shell) for the Linux operating system. 

## Install

### From Release

#### Linux x86_64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v19/hash-shell-v19-linux-x86_64
chmod +x hash-shell-v19-linux-x86_64
sudo mv hash-shell-v19-linux-x86_64 /usr/local/bin/hash-shell
hash-shell
```

#### Linux ARM64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v19/hash-shell-v19-linux-aarch64
chmod +x hash-shell-v19-linux-aarch64
sudo mv hash-shell-v19-linux-aarch64 /usr/local/bin/hash-shell
hash-shell
```

#### macOS (Apple Silicon)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v19/hash-shell-v19-darwin-arm64
chmod +x hash-shell-v19-darwin-arm64
sudo mv hash-shell-v19-darwin-arm64 /usr/local/bin/hash-shell
```

### From Source

```bash
git clone https://github.com/juliojimenez/hash
cd hash
make
sudo make install
hash-shell
```

## Usage

### Start hash

```bash
hash-shell
```

### Execute a Script

```bash
hash-shell script.sh arg1 arg2
```

### Execute Command String

```bash
hash-shell -c 'echo "Hello, World!"'
```

### Change Default Shell To hash

Log off and log on to your session for changes to take effect.

#### chsh

```bash
chsh -s /usr/local/bin/hash-shell
```

#### usermod

```bash
sudo usermod -s /usr/local/bin/hash-shell your_username
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

## See Also

- [Shell Scripting](./docs/SCRIPTING.md)
  - [Scripting Quick Reference](./docs/SCRIPTING_QUICKREF.md)
  - [Control Structures](./docs/CONTROL_STRUCTURES.md)
  - [Test Command](./docs/TEST_COMMAND.md)
- [.hashrc](./docs/HASHRC.md)
  - [.hashrc Quick Start](./docs/QUICK_START_HASHRC.md)
- [Variable Expansion](./docs/VARIABLES.md)
  - [Variables Quick Reference](./docs/VARIABLES_QUICKREF.md)
- [Command Substitution](./docs/COMMAND_SUBSTITUTION.md)
- [Line Editing](./docs/LINE_EDITING.md)
- [Prompt Customization](./docs/PROMPT.md)
  - [PS1 Reference](./docs/PS1.md)
- [Pipes](./docs/PIPES.md)
- [Command Chaining](./docs/COMMAND_CHAINING.md)
- [Background Processes](./docs/BACKGROUND.md)
- [I/O Redirection](./docs/REDIRECTION.md)
- [Tilde Expansion](./docs/EXPANSION.md)
- [Command History](./docs/HISTORY.md)
  - [HISTCONTROL](./docs/HISTCONTROL.md)
- [Tab Completion](./docs/TAB_COMPLETION.md)
  - [Tab Completion Quick Reference](./docs/COMPLETION_QUICKREF.md)
- [Login Shell](./docs/LOGIN_SHELL.md)
- [Update Notifications](./docs/UPDATE.md)
- [Safe String Utilities](./docs/SAFE_STRING.md)
- [Testing](./docs/TESTING.md)
- [Contributing](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)
- [License](LICENSE)

## Newsletter

Keep up with development updates on [Rede Livre](https://redelivre.net/newsletters/hash) <img style="width:16px;margin-left:3px;margin-top:2px;" src="images/redelivre.png" />
