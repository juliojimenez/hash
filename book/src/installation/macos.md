# macOS Installation

## Homebrew (Recommended)

The easiest way to install hash on macOS:

```bash
brew install juliojimenez/hash/hash-shell
hash-shell
```

### Upgrade

```bash
brew upgrade hash-shell
```

### Uninstall

```bash
brew uninstall hash-shell
```

## Binary Release

### Apple Silicon (M1/M2/M3)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v38/hash-shell-v38-darwin-arm64
chmod +x hash-shell-v38-darwin-arm64
sudo mv hash-shell-v38-darwin-arm64 /usr/local/bin/hash-shell
hash-shell
```

## From Source

See [Building from Source](source.md) for compilation instructions.

## Setting as Default Shell

### Homebrew Installation

```bash
sudo bash -c 'echo "$(brew --prefix)/bin/hash-shell" >> /etc/shells'
chsh -s "$(brew --prefix)/bin/hash-shell"
```

### Binary Installation

```bash
sudo bash -c 'echo "/usr/local/bin/hash-shell" >> /etc/shells'
chsh -s /usr/local/bin/hash-shell
```

Log out and log back in for changes to take effect.

## Verify Installation

```bash
hash-shell --version
hash-shell
```
